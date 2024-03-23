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
	struct arm_smccc_1_2_regs in;
	struct arm_smccc_1_2_regs out;

	in.a0 = CPU_SVC_GET_L1I_CACHELINE;
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

static int ramindex_cortex_a720_dump_l1d_cacheline(__s32 set, __s32 way, __u32 lsize, struct ramindex_cacheline __user *l)
{
	struct arm_smccc_1_2_regs in;
	struct arm_smccc_1_2_regs out;

	in.a0 = CPU_SVC_GET_L1D_CACHELINE;
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
