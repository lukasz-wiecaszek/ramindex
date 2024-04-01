/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ramindex-cortex-a720.c
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
 *
 * The Cortex-A720 allows for access to caches' internal memory only in EL3.
 * Thus SMC calls to some Secure Monitor are needed to handle such accesses.
 * This module uses Arm Trusted Firmware software which implements Secure Monitor
 * and provides means to deliver your own services.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/arm-smccc.h>

#include "ramindex-ops.h"

#define CPU_SVC_GET_L1I_CACHELINE \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, ARM_SMCCC_OWNER_CPU, 0x0001)

#define CPU_SVC_GET_L1D_CACHELINE \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, ARM_SMCCC_OWNER_CPU, 0x0002)

#define CPU_SVC_GET_L2U_CACHELINE \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, ARM_SMCCC_OWNER_CPU, 0x0003)

#define CPU_SVC_GET_L3U_CACHELINE \
	ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, ARM_SMCCC_OWNER_CPU, 0x0004)

static int ramindex_cortex_a720_dump_l1i_cacheline(__s32 set, __s32 way, __u32 lsize, struct ramindex_cacheline __user *l)
{
	__u8 valid;
	__u8 dirty;
	__u8 ns;
	__u64 tag;
	__u32 linesize;
	__u64 *linedata;
	struct arm_smccc_1_2_regs in;
	struct arm_smccc_1_2_regs out;
	int ret = 0;

	ret |= get_user(linesize, (__u32 __user *)&l->linesize);
	ret |= get_user(linedata, (__u64* __user *)&l->linedata);

	if (ret)
		return ret;

	if (lsize < linesize)
		linesize = lsize;

	in.a0 = CPU_SVC_GET_L1I_CACHELINE;
	in.a1 = set;
	in.a2 = way;
	arm_smccc_1_2_smc(&in, &out);

	/* Secure Monitor returns SMC_OK on success, and SMC_UNK on error */
	if (out.a0)
		return -EFAULT;

	/* out.a1 contains IMP_ISIDE_DATA0_EL3 for L1 instruction cache tag */
	valid = (out.a1 >> 29) & 0x1;
	dirty = 0; /* dirty bit is not present in instruction cache */
	ns = (out.a1 >> 28) & 0x1;
	tag = ((out.a1 & 0x0fffffff) << 12) | ((set & 0x3f) << 6);

	ret |= put_user(set, (__s32 __user *)&l->set);
	ret |= put_user(way, (__s32 __user *)&l->way);
	ret |= put_user(valid, (__u8 __user *)&l->valid);
	ret |= put_user(dirty, (__u8 __user *)&l->dirty);
	ret |= put_user(ns, (__u8 __user *)&l->ns);
	ret |= put_user(tag, (__u64 __user *)&l->tag);
	ret |= put_user(linesize, (__u32 __user *)&l->linesize);

	if (ret)
		return ret;

#define COPY_TO_USER(value, index)                                                        \
	if (linesize >= 8) {                                                              \
		ret |= put_user(value, (__u32 __user *)(linedata + index));               \
		linesize -= 8;                                                            \
	} else {                                                                          \
		ret |= copy_to_user((void __user *)(linedata + index), &value, linesize); \
		break;                                                                    \
	}
	/* out.a2 till out.a9 contain cache line data */
	do {
		COPY_TO_USER(out.a2, 0);
		COPY_TO_USER(out.a3, 1);
		COPY_TO_USER(out.a4, 2);
		COPY_TO_USER(out.a5, 3);
		COPY_TO_USER(out.a6, 4);
		COPY_TO_USER(out.a7, 5);
		COPY_TO_USER(out.a8, 6);
		COPY_TO_USER(out.a9, 7);
	} while (0);
#undef COPY_TO_USER

	return ret;
}

static int ramindex_cortex_a720_dump_l1d_cacheline(__s32 set, __s32 way, __u32 lsize, struct ramindex_cacheline __user *l)
{
	__u8 valid;
	__u8 dirty;
	__u8 ns;
	__u64 tag;
	__u32 linesize;
	__u64 *linedata;
	struct arm_smccc_1_2_regs in;
	struct arm_smccc_1_2_regs out;
	int ret = 0;

	ret |= get_user(linesize, (__u32 __user *)&l->linesize);
	ret |= get_user(linedata, (__u64* __user *)&l->linedata);

	if (ret)
		return ret;

	if (lsize < linesize)
		linesize = lsize;

	in.a0 = CPU_SVC_GET_L1D_CACHELINE;
	in.a1 = set;
	in.a2 = way;
	arm_smccc_1_2_smc(&in, &out);

	/* Secure Monitor returns SMC_OK on success, and SMC_UNK on error */
	if (out.a0)
		return -EFAULT;

	/* out.a1 contains IMP_DSIDE_DATA0_EL3 for L1 data cache tag */
	valid = (out.a1 & 0x3) != 0;
	dirty = (out.a1 & 0x3) == 0x2;
	ns = (out.a1 >> 30) & 0x1;
	tag = (((out.a1 >> 2) & 0x0fffffff) << 12) | ((set & 0x3f) << 6);

	ret |= put_user(set, (__s32 __user *)&l->set);
	ret |= put_user(way, (__s32 __user *)&l->way);
	ret |= put_user(valid, (__u8 __user *)&l->valid);
	ret |= put_user(dirty, (__u8 __user *)&l->dirty);
	ret |= put_user(ns, (__u8 __user *)&l->ns);
	ret |= put_user(tag, (__u64 __user *)&l->tag);
	ret |= put_user(linesize, (__u32 __user *)&l->linesize);

	if (ret)
		return ret;

#define COPY_TO_USER(value, index)                                                        \
	if (linesize >= 8) {                                                              \
		ret |= put_user(value, (__u32 __user *)(linedata + index));               \
		linesize -= 8;                                                            \
	} else {                                                                          \
		ret |= copy_to_user((void __user *)(linedata + index), &value, linesize); \
		break;                                                                    \
	}
	/* out.a2 till out.a9 contain cache line data */
	do {
		COPY_TO_USER(out.a2, 0);
		COPY_TO_USER(out.a3, 1);
		COPY_TO_USER(out.a4, 2);
		COPY_TO_USER(out.a5, 3);
		COPY_TO_USER(out.a6, 4);
		COPY_TO_USER(out.a7, 5);
		COPY_TO_USER(out.a8, 6);
		COPY_TO_USER(out.a9, 7);
	} while (0);
#undef COPY_TO_USER

	return ret;
}

static int ramindex_cortex_a720_dump_l2u_cacheline(__s32 set, __s32 way, __u32 lsize, struct ramindex_cacheline __user *l)
{
	struct arm_smccc_1_2_regs in;
	struct arm_smccc_1_2_regs out;

	in.a0 = CPU_SVC_GET_L2U_CACHELINE;
	in.a1 = set;
	in.a2 = way;
	arm_smccc_1_2_smc(&in, &out);

	pr_info("%s: %lu, %lu, %lu %lu, %lu, %lu, %lu %lu\n",
		__func__, out.a0, out.a1, out.a2, out.a3, out.a4, out.a5, out.a6, out.a7);

	if (out.a0)
		return -EFAULT;

	//TODO: Parse response from ATF

	return 0;
}

static int ramindex_cortex_a720_dump_l3u_cacheline(__s32 set, __s32 way, __u32 lsize, struct ramindex_cacheline __user *l)
{
	struct arm_smccc_1_2_regs in;
	struct arm_smccc_1_2_regs out;

	in.a0 = CPU_SVC_GET_L3U_CACHELINE;
	in.a1 = set;
	in.a2 = way;
	arm_smccc_1_2_smc(&in, &out);

	pr_info("%s: %lu, %lu, %lu %lu, %lu, %lu, %lu %lu\n",
		__func__, out.a0, out.a1, out.a2, out.a3, out.a4, out.a5, out.a6, out.a7);

	if (out.a0)
		return -EFAULT;

	//TODO: Parse response from ATF

	return 0;
}

const struct ramindex_ops ramindex_cortex_a720_ops = {
	.dump_l1i_cacheline = ramindex_cortex_a720_dump_l1i_cacheline,
	.dump_l1d_cacheline = ramindex_cortex_a720_dump_l1d_cacheline,
	.dump_l2d_cacheline = ramindex_cortex_a720_dump_l2u_cacheline,
	.dump_l3d_cacheline = ramindex_cortex_a720_dump_l3u_cacheline,
};
