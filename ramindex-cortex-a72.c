/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ramindex-cortex-a72.c
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

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/uaccess.h>

#include "ramindex-ops.h"

static int ramindex_cortex_a72_dump_l1i_cacheline(__s32 set, __s32 way, __u32 lsize, struct ramindex_cacheline __user *l)
{
	__u8 valid;
	__u8 dirty;
	__u8 ns;
	__u64 tag;
	__u32 ls;
	__u32 linesize;
	__u32 *linedata;
	__u32 selector;
	__u32 r0, r1;
	int ret = 0;

	ret |= get_user(linesize, (__u32 __user *)&l->linesize);
	ret |= get_user(linedata, (__u32* __user *)&l->linedata);

	if (ret)
		return ret;

	if (lsize < linesize)
		linesize = lsize;

	/*
	* RAMINDEX bit assignments
	* When AArch64-RAMINDEX.ID == 0x00 and 48KiB of L1 I$
	*
	* [31:24] RAMID		ID of the selected memory (L1 I$ Tag)
	* [23:20] Reserved
	* [19:18] Way
	* [17:14] Reserved
	* [13:6] Set		Virtual Address bits [13:6]
	* [5:0] Reserved
	*/
	selector = 0x00000000; /* this selects l1 instruction cache tag (ramid = 0x00) */
	selector |= (way & 0x3) << 18;
	selector |= (set & 0xff) << 6;

	asm volatile("sys #0, c15, c4, #0, %0" : : "r" (selector));
	asm volatile("dsb sy");
	asm volatile("isb");
	asm volatile("mrs %0, s3_0_c15_c0_0" : "=r" (r0));
	asm volatile("mrs %0, s3_0_c15_c0_1" : "=r" (r1));

	valid = (r1 >> 1) & 0x1;
	dirty = 0; /* dirty bit is not present in instruction cache */
	ns = (r1 >> 0) & 0x1;
	tag = r0 << 12 | (set & 0x3f) << 6;

	ret |= put_user(set, (__s32 __user *)&l->set);
	ret |= put_user(way, (__s32 __user *)&l->way);
	ret |= put_user(valid, (__u8 __user *)&l->valid);
	ret |= put_user(dirty, (__u8 __user *)&l->dirty);
	ret |= put_user(ns, (__u8 __user *)&l->ns);
	ret |= put_user(tag, (__u64 __user *)&l->tag);
	ret |= put_user(linesize, (__u32 __user *)&l->linesize);

	if (ret)
		return ret;

	/*
	* RAMINDEX bit assignments
	* When AArch64-RAMINDEX.ID == 0x01 and 48KiB of L1 I$
	*
	* [31:24] RAMID		ID of the selected memory (L1 I$ Data)
	* [23:20] Reserved
	* [19:18] Way
	* [17:14] Reserved
	* [13:6] Set		Virtual Address bits [13:6]/Index
	* [5:4] Bank select
	* [3] Upper or lower doubleword within the quadword
	* [2:0] Reserved
	*/
	for (ls = 0; !ret && ls < linesize; ls+=sizeof(*linedata)*2, linedata+=2) {
		selector = 0x01000000; /* this selects l1 instruction cache data (ramid = 0x01) */
		selector |= (way & 0x3) << 18;
		selector |= (set & 0xff) << 6;
		selector |= ls;

		asm volatile("sys #0, c15, c4, #0, %0" : : "r" (selector));
		asm volatile("dsb sy");
		asm volatile("isb");
		asm volatile("mrs %0, s3_0_c15_c0_0" : "=r" (r0));
		asm volatile("mrs %0, s3_0_c15_c0_1" : "=r" (r1));

		if (ls + sizeof(*linedata)*2 < linesize) {
			ret |= put_user(r0, (__u32 __user *)(linedata + 0));
			ret |= put_user(r1, (__u32 __user *)(linedata + 1));
		} else if (ls + sizeof(*linedata) < linesize) {
			ret |= put_user(r0, (__u32 __user *)(linedata + 0));
			ret |= copy_to_user((void __user *)(linedata + 1), &r1, linesize - ls - sizeof(*linedata));
		} else {
			ret |= copy_to_user((void __user *)(linedata + 0), &r0, linesize - ls);
		}
	}

	return ret;
}

static int ramindex_cortex_a72_dump_l1d_cacheline(__s32 set, __s32 way, __u32 lsize, struct ramindex_cacheline __user *l)
{
	__u8 valid;
	__u8 dirty;
	__u8 ns;
	__u64 tag;
	__u32 ls;
	__u32 linesize;
	__u32 *linedata;
	__u32 selector;
	__u32 r0, r1;
	int ret = 0;

	ret |= get_user(linesize, (__u32 __user *)&l->linesize);
	ret |= get_user(linedata, (__u32* __user *)&l->linedata);

	if (ret)
		return ret;

	if (lsize < linesize)
		linesize = lsize;

	/*
	* RAMINDEX bit assignments
	* When AArch64-RAMINDEX.ID == 0x08 and 32KB of L1 D$
	*
	* [31:24] RAMID		ID of the selected memory (L1 D$ Tag)
	* [23:19] Reserved
	* [18] Way
	* [17:14] Reserved
	* [13:6] Set		Virtual Address bits [13:6]
	* [5:0] Reserved
	*/
	selector = 0x08000000; /* this selects l1 data cache tag (ramid = 0x08) */
	selector |= (way & 0x1) << 18;
	selector |= (set & 0xff) << 6;

	asm volatile("sys #0, c15, c4, #0, %0" : : "r" (selector));
	asm volatile("dsb sy");
	asm volatile("isb");
	asm volatile("mrs %0, s3_0_c15_c1_0" : "=r" (r0));
	asm volatile("mrs %0, s3_0_c15_c1_1" : "=r" (r1));

	valid = (r1 & 0x3) != 0;
	dirty = (r1 & 0x3) == 0x3;
	ns = (r0 >> 30) & 0x1;
	tag = (r0 & 0x3fffffff) << 14 | (set & 0xff) << 6;

	ret |= put_user(set, (__s32 __user *)&l->set);
	ret |= put_user(way, (__s32 __user *)&l->way);
	ret |= put_user(valid, (__u8 __user *)&l->valid);
	ret |= put_user(dirty, (__u8 __user *)&l->dirty);
	ret |= put_user(ns, (__u8 __user *)&l->ns);
	ret |= put_user(tag, (__u64 __user *)&l->tag);
	ret |= put_user(linesize, (__u32 __user *)&l->linesize);

	if (ret)
		return ret;

	/*
	* RAMINDEX bit assignments
	* When AArch64-RAMINDEX.ID == 0x09 and 32KB of L1 D$
	*
	* [31:24] RAMID		ID of the selected memory (L1 D$ Data)
	* [23:19] Reserved
	* [18] Way
	* [17:14] Reserved
	* [13:6] Set		Virtual Address bits [13:6]
	* [5:4] Bank select
	* [3] Upper or lower doubleword within the quadword
	* [2:0] Reserved
	*/
	for (ls = 0; !ret && ls < linesize; ls+=sizeof(*linedata)*2, linedata+=2) {
		selector = 0x09000000; /* this selects l1 data cache data (ramid = 0x09) */
		selector |= (way & 0x1) << 18;
		selector |= (set & 0xff) << 6;
		selector |= ls;

		asm volatile("sys #0, c15, c4, #0, %0" : : "r" (selector));
		asm volatile("dsb sy");
		asm volatile("isb");
		asm volatile("mrs %0, s3_0_c15_c1_0" : "=r" (r0));
		asm volatile("mrs %0, s3_0_c15_c1_1" : "=r" (r1));

		if (ls + sizeof(*linedata)*2 < linesize) {
			ret |= put_user(r0, (__u32 __user *)(linedata + 0));
			ret |= put_user(r1, (__u32 __user *)(linedata + 1));
		} else if (ls + sizeof(*linedata) < linesize) {
			ret |= put_user(r0, (__u32 __user *)(linedata + 0));
			ret |= copy_to_user((void __user *)(linedata + 1), &r1, linesize - ls - sizeof(*linedata));
		} else {
			ret |= copy_to_user((void __user *)(linedata + 0), &r0, linesize - ls);
		}
	}

	return ret;
}

const struct ramindex_ops ramindex_cortex_a72_ops = {
	.dump_l1i_cacheline = ramindex_cortex_a72_dump_l1i_cacheline,
	.dump_l1d_cacheline = ramindex_cortex_a72_dump_l1d_cacheline,
};
