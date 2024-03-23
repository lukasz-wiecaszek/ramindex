/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ramindex-main.c
 *
 * Copyright (C) 2024 Lukasz Wiecaszek <lukasz.wiecaszek(at)gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License (in file COPYING) for more details.
 */

//TODO: UID:GID for device file

#define pr_fmt(fmt) "ramindex: " fmt

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/delay.h>

#include <linux/uaccess.h>

#include "ramindex.h"
#include "ramindex-ops.h"
#include "ramindex-cortex-a72.h"
#include "ramindex-cortex-a720.h"

#define ramindex_dbg_at1(args...) do { if (ramindex_debug_level >= 1) pr_info(args); } while (0)
#define ramindex_dbg_at2(args...) do { if (ramindex_debug_level >= 2) pr_info(args); } while (0)
#define ramindex_dbg_at3(args...) do { if (ramindex_debug_level >= 3) pr_info(args); } while (0)
#define ramindex_dbg_at4(args...) do { if (ramindex_debug_level >= 4) pr_info(args); } while (0)

#define RAMINDEX_DEVICE_NAME "ramindex"

#define RAMINDEX_VERSION_STR \
	__stringify(RAMINDEX_VERSION_MAJOR) "." \
	__stringify(RAMINDEX_VERSION_MINOR) "." \
	__stringify(RAMINDEX_VERSION_MICRO)

/* module's params */
static int ramindex_debug_level = 0; /* do not emmit any traces by default */
module_param_named(debug, ramindex_debug_level, int, 0660);
MODULE_PARM_DESC(debug,
	"Verbosity of debug messages (range: [0(none)-4(max)], default: 0)");

/**
 * struct ramindex_device - groups device related data structures
 * @miscdev:	our character device
 */
struct ramindex_device {
	struct miscdevice miscdev;
	__u64 midr_el1;
	__u64 clidr_el1;
	const struct ramindex_ops *ops;
};

static struct ramindex_device ramindex_device;

static void ramindex_get_ccsidr(struct ramindex_ccsidr *ccsidr)
{
	__u64 csselr_el1;
	__u64 id_aa64mmfr2_el1;
	__u64 ccsidr_el1;
	__s32 ccidx;

	csselr_el1 = ((ccsidr->level & 0x7) << 1) | (ccsidr->icache & 0x1);

	asm volatile("msr csselr_el1, %0" : : "r" (csselr_el1)); /* select cache level */
	asm volatile("isb"); /* sync change of cssidr_el1 */
	asm volatile("mrs %0, s3_0_c0_c7_2" : "=r" (id_aa64mmfr2_el1)); /* read the id_aa64mmfr2_el1 */
	asm volatile("mrs %0, ccsidr_el1" : "=r" (ccsidr_el1)); /* read the ccsidr_el1 */

	ramindex_dbg_at2("ccsidr_el1: 0x%llx\n", ccsidr_el1);

	ccidx = (id_aa64mmfr2_el1 >> 20) & 0xf;
	if (ccidx) {
		ccsidr->nsets = ((ccsidr_el1 >> 32) & 0xffffff) + 1;
		ccsidr->nways = ((ccsidr_el1 >> 3) & 0x1fffff) + 1;
	} else {
		ccsidr->nsets = ((ccsidr_el1 >> 13) & 0x7fff) + 1;
		ccsidr->nways = ((ccsidr_el1 >> 3) & 0x3ff) + 1;
	}
	ccsidr->linesize = 1 << (((ccsidr_el1 >> 0) & 0x7) + 4);
}

static long ramindex_ioctl_version(void __user *ubuf, size_t size)
{
	struct ramindex_version version;

	if (size != sizeof(struct ramindex_version))
		return -EINVAL;

	memset(&version, 0, sizeof(version));
	version.major = RAMINDEX_VERSION_MAJOR;
	version.minor = RAMINDEX_VERSION_MINOR;
	version.micro = RAMINDEX_VERSION_MICRO;

	if (copy_to_user(ubuf, &version, sizeof(version)))
		return -EFAULT;

	return 0;
}

static long ramindex_ioctl_clid(void __user *ubuf, size_t size)
{
	__u64 clidr_el1 = ramindex_device.clidr_el1;
	struct ramindex_clid clid;
	size_t i;

	if (size != sizeof(struct ramindex_clid))
		return -EINVAL;

	memset(&clid, 0, sizeof(clid));
	for (i = 0; i < ARRAY_SIZE(clid.ctype); i++, clidr_el1 >>= 3)
		clid.ctype[i] = clidr_el1 & 0x7;

	if (clid.ctype[0] != CTYPE_NO_CACHE) {
		ramindex_dbg_at2("Cache hierarchy:\n");
		for (i = 0; i < ARRAY_SIZE(clid.ctype); i++) {
			if (clid.ctype[i] == CTYPE_NO_CACHE)
				break;
			ramindex_dbg_at2("L%zu -> '%s'\n",
				i + 1, ramindex_ctype_to_string(clid.ctype[i]));
		}
	} else
		ramindex_dbg_at2("System has no caches\n");

	if (copy_to_user(ubuf, &clid, sizeof(clid)))
		return -EFAULT;

	return 0;
}

static long ramindex_ioctl_ccsidr(void __user *ubuf, size_t size)
{
	struct ramindex_ccsidr ccsidr;

	if (size != sizeof(struct ramindex_ccsidr))
		return -EINVAL;

	if (copy_from_user(&ccsidr, ubuf, sizeof(ccsidr)))
		return -EFAULT;

	ramindex_get_ccsidr(&ccsidr);

	if (copy_to_user(ubuf, &ccsidr, sizeof(ccsidr)))
		return -EFAULT;

	return 0;
}

static long ramindex_ioctl_dump(void __user *ubuf, size_t size)
{
	long status = -EFAULT;
	__s32 start_set, end_set, set;
	__s32 start_way, end_way, way;
	__u32 nlines = 0;
	struct ramindex_selector selector;
	struct ramindex_ccsidr ccsidr;
	dumpfunction_t df = NULL;

	if (size != sizeof(struct ramindex_selector))
		return -EINVAL;

	if (copy_from_user(&selector, ubuf, sizeof(selector)))
		return -EFAULT;

	switch (selector.level) {
	case 0:
		df = selector.icache ?
			ramindex_device.ops->dump_l1i_cacheline :
			ramindex_device.ops->dump_l1d_cacheline;
		break;
	case 1:
		df = selector.icache ?
			ramindex_device.ops->dump_l2i_cacheline :
			ramindex_device.ops->dump_l2d_cacheline;
		break;
	case 2:
		df = selector.icache ?
			ramindex_device.ops->dump_l3i_cacheline :
			ramindex_device.ops->dump_l3d_cacheline;
		break;
	default:
		df = NULL;
		break;
	}

	/* Validate arguments */
	if (df == NULL) {
		ramindex_dbg_at1(
			"There is no associated operation to dump L%d %s cache\n",
			selector.level + 1, selector.icache ? "instruction" : "data");
		return -EOPNOTSUPP;
	}

	memset(&ccsidr, 0, sizeof(ccsidr));
	ccsidr.level = selector.level;
	ccsidr.icache = selector.icache;
	ramindex_get_ccsidr(&ccsidr);

	if (selector.set >= 0 && selector.set >= ccsidr.nsets) {
		ramindex_dbg_at1(
			"Selected L%d %s cache has %d sets whereas %d set has been requested\n",
			selector.level + 1, selector.icache ? "instruction" : "data",
			ccsidr.nsets, selector.set);
		return -EINVAL;
	}

	if (selector.way >= 0 && selector.way >= ccsidr.nways) {
		ramindex_dbg_at1(
			"Selected L%d %s cache has %d ways whereas %d way has been requested\n",
			selector.level + 1, selector.icache ? "instruction" : "data",
			ccsidr.nways, selector.way);
		return -EINVAL;
	}

	if (selector.set < 0)
		start_set = 0, end_set = ccsidr.nsets;
	else
		start_set = selector.set, end_set = selector.set + 1;

	if (selector.way < 0)
		start_way = 0, end_way = ccsidr.nways;
	else
		start_way = selector.way, end_way = selector.way + 1;

	for (set = start_set; set < end_set; set++)
		for (way = start_way; way < end_way; way++)
			if (nlines < selector.nlines) {
				struct ramindex_cacheline __user *cacheline =
					selector.lines + nlines;
				status = df(set, way, ccsidr.linesize, cacheline);
				if (status)
					goto out;
				nlines++;
			}
			else
				goto out;
out:
	if (status)
		return status;

	put_user(nlines, (__u32 __user *)&(((struct ramindex_selector *)ubuf)->nlines));

	return 0;
}

static long ramindex_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -EFAULT;
	size_t size = _IOC_SIZE(cmd);
	void __user *ubuf = (void __user *)arg;

	ramindex_dbg_at3("%s() cmd: %u '%s'\n",
		__func__, cmd, ramindex_cmd_to_string(cmd));

	switch (cmd) {
	case RAMINDEX_VERSION:
		ret = ramindex_ioctl_version(ubuf, size);
		break;
	case RAMINDEX_CLID:
		ret = ramindex_ioctl_clid(ubuf, size);
		break;
	case RAMINDEX_CCSIDR:
		ret = ramindex_ioctl_ccsidr(ubuf, size);
		break;
	case RAMINDEX_DUMP:
		ret = ramindex_ioctl_dump(ubuf, size);
		break;
	default:
		msleep(1000); /* deliberately sleep for 1 second */
		ret = -EINVAL;
		break;
	}

	return ret;
}

const struct file_operations ramindex_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ramindex_ioctl,
};

static int __init ramindex_init(void)
{
	int status;
	__u64 midr_el1;
	__u64 clidr_el1;

	asm volatile("mrs %0, midr_el1" : "=r" (midr_el1));
	asm volatile("mrs %0, clidr_el1" : "=r" (clidr_el1));

	switch (midr_el1) {
	case 0x410fd083:
		ramindex_device.ops = &ramindex_cortex_a72_ops;
		break;
	case 0x410fd811:
		ramindex_device.ops = &ramindex_cortex_a720_ops;
		break;
	default:
		status = -EOPNOTSUPP;
		break;
	}

	ramindex_device.midr_el1 = midr_el1;
	ramindex_device.clidr_el1 = clidr_el1;

	ramindex_device.miscdev.fops = &ramindex_fops;
	ramindex_device.miscdev.minor = MISC_DYNAMIC_MINOR;
	ramindex_device.miscdev.name = RAMINDEX_DEVICE_NAME;
	status = misc_register(&ramindex_device.miscdev);
	if (status < 0) {
		pr_err("misc_register(%s) failed with code %d\n",
			ramindex_device.miscdev.name, status);
		return status;
	}

	pr_info("module loaded (version: %s, midr_el1: 0x%llx, clidr_el1: 0x%llx)\n",
		RAMINDEX_VERSION_STR, midr_el1, clidr_el1);
	return 0;
}
module_init(ramindex_init);

#ifdef MODULE
static void __exit ramindex_exit(void)
{
	misc_deregister(&ramindex_device.miscdev);
	pr_info("module removed\n");
}
module_exit(ramindex_exit);
#endif

MODULE_DESCRIPTION("ramindex driver");
MODULE_AUTHOR("Lukasz Wiecaszek <lukasz.wiecaszek(at)gmail.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(RAMINDEX_VERSION_STR);
