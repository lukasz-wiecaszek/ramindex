/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * ramindex-cortex-a720.c
 *
 * Copyright (c) 2024 Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <common/debug.h>
#include <common/runtime_svc.h>
#include <smccc_helpers.h>

#define CPU_SVC_GET_L1I_CACHELINE	0x81000001
#define CPU_SVC_GET_L1D_CACHELINE	0x81000002
#define CPU_SVC_GET_L2U_CACHELINE	0x81000003
#define CPU_SVC_GET_L3U_CACHELINE	0x81000004

static u_register_t cortex_a720_get_l1i_cacheline(void *handle, u_register_t set, u_register_t way)
{
	uint64_t selector;
	uint64_t r0, r1;
	int i;

	/*
	* RAMINDEX bit assignments
	* When AArch64-RAMINDEX.ID == 0x00 and 32KiB of L1 I$
	*
	* [63:32] Reserved
	* [31:24] RAMID		ID of the selected memory (L1 I$ Tag)
	* [23:20] Reserved
	* [19:18] Way
	* [17:13] Reserved
	* [12:6] Set		Virtual Address bits [12:6]
	* [5:0] Reserved
	*/
	selector = 0x0000000000000000ULL; /* this selects l1 instruction cache tag (ramid = 0x00) */
	selector |= (way & 0x3) << 18;
	selector |= (set & 0x7f) << 6;

	asm volatile("sys #6, c15, c0, #0, %0" : : "r" (selector));
	asm volatile("dsb sy");
	asm volatile("isb");
	asm volatile("mrs %0, s3_6_c15_c0_0" : "=r" (r0));

	write_ctx_reg((get_gpregs_ctx(handle)), (CTX_GPREG_X1), r0);

	/*
	* RAMINDEX bit assignments
	* When AArch64-RAMINDEX.ID == 0x01 and 32KiB of L1 I$
	*
	* [63:32] Reserved
	* [31:24] RAMID		ID of the selected memory (L1 I$ Data)
	* [23:20] Reserved
	* [19:18] Way
	* [17] Reserved
	* [16:14] VA[5:3]	Virtual Address bits[5:3]
	* [13] Reserved
	* [12:6] Set		Virtual Address bits [12:6]
	* [5:0] Reserved
	*/
	for (i = 0; i < 8; i++) {
		selector = 0x0000000001000000ULL; /* this selects l1 instruction cache data (ramid = 0x01) */
		selector |= (way & 0x3) << 18;
		selector |= (set & 0x7f) << 6;
		selector |= (i & 0x7) << 14; /* this selects bytes [0+i*8:7+i*8] from cacheline */
		asm volatile("sys #6, c15, c0, #0, %0" : : "r" (selector));
		asm volatile("dsb sy");
		asm volatile("isb");
		asm volatile("mrs %0, s3_6_c15_c0_0" : "=r" (r0));
		asm volatile("mrs %0, s3_6_c15_c0_1" : "=r" (r1));

		write_ctx_reg((get_gpregs_ctx(handle)), (CTX_GPREG_X2 + i * sizeof(u_register_t)),
			(r1 & 0xffffffff) << 32 | (r0 & 0xffffffff));
	}

	return SMC_OK;
}

static u_register_t cortex_a720_get_l1d_cacheline(void *handle, u_register_t set, u_register_t way)
{
	uint64_t selector;
	uint64_t r0, r1;
	int i;

	/*
	* RAMINDEX bit assignments
	* When AArch64-RAMINDEX.ID == 0x08 and 32KiB of L1 D$
	*
	* [63:32] Reserved
	* [31:24] RAMID		ID of the selected memory (L1 D$ Tag)
	* [23:20] Reserved
	* [19:18] Way
	* [17:16] BANK		0b00 Tag RAM 0, 0b01 Tag RAM 1, 0b10 Tag RAM 2
	* [15:13] Reserved
	* [12:6] Set		Virtual Address bits [12:6]
	* [5:0] Reserved
	*/
	selector = 0x0000000008000000ULL; /* this selects l1 data cache tag (ramid = 0x08) */
	selector |= (way & 0x3) << 18;
	selector |= (set & 0x7f) << 6;

	asm volatile("sys #6, c15, c0, #0, %0" : : "r" (selector));
	asm volatile("dsb sy");
	asm volatile("isb");
	asm volatile("mrs %0, s3_6_c15_c1_0" : "=r" (r0));

	write_ctx_reg((get_gpregs_ctx(handle)), (CTX_GPREG_X1), r0);

	/*
	* RAMINDEX bit assignments
	* When AArch64-RAMINDEX.ID == 0x09 and 32KiB of L1 D$
	*
	* [63:32] Reserved
	* [31:24] RAMID		ID of the selected memory (L1 D$ Data)
	* [23:20] Reserved
	* [19:18] Way
	* [17:16] VA[5:4]	Virtual Address bits[5:4]
	* [15:13] Reserved
	* [12:6] Set		Virtual Address bits [12:6]
	* [5:0] Reserved
	*/

	for (i = 0; i < 4; i++) {
		selector = 0x0000000009000000ULL; /* this selects l1 data cache data (ramid = 0x09) */
		selector |= (way & 0x3) << 18;
		selector |= (set & 0x7f) << 6;
		selector |= (i & 0x3) << 16; /* this selects bytes [0+i*16:15+i*16] from cacheline */
		asm volatile("sys #6, c15, c0, #0, %0" : : "r" (selector));
		asm volatile("dsb sy");
		asm volatile("isb");
		asm volatile("mrs %0, s3_6_c15_c1_0" : "=r" (r0));
		asm volatile("mrs %0, s3_6_c15_c1_1" : "=r" (r1));

		write_ctx_reg((get_gpregs_ctx(handle)), (CTX_GPREG_X2 + (i * 2 + 0) * sizeof(u_register_t)), r0);
		write_ctx_reg((get_gpregs_ctx(handle)), (CTX_GPREG_X2 + (i * 2 + 1) * sizeof(u_register_t)), r1);
	}

	return SMC_OK;
}

static u_register_t cortex_a720_get_l2u_cacheline(void *handle, u_register_t set, u_register_t way)
{
	/* To be implemented yet */
	return SMC_UNK;
}

static u_register_t cortex_a720_get_l3u_cacheline(void *handle, u_register_t set, u_register_t way)
{
	/* To be implemented yet */
	return SMC_UNK;
}

uintptr_t cortex_a720_smc_handler(uint32_t smc_fid,
				u_register_t x1,
				u_register_t x2,
				u_register_t x3,
				u_register_t x4,
				void *cookie,
				void *handle,
				u_register_t flags)
{
	u_register_t ret;

	switch (smc_fid) {
	case CPU_SVC_GET_L1I_CACHELINE:
		ret = cortex_a720_get_l1i_cacheline(handle, x1, x2);
		SMC_RET1(handle, ret);

	case CPU_SVC_GET_L1D_CACHELINE:
		ret = cortex_a720_get_l1d_cacheline(handle, x1, x2);
		SMC_RET1(handle, ret);

	case CPU_SVC_GET_L2U_CACHELINE:
		ret = cortex_a720_get_l2u_cacheline(handle, x1, x2);
		SMC_RET1(handle, ret);

	case CPU_SVC_GET_L3U_CACHELINE:
		ret = cortex_a720_get_l3u_cacheline(handle, x1, x2);
		SMC_RET1(handle, ret);

	default:
		ERROR("%s: unhandled SMC (0x%x)\n", __func__, smc_fid);
		SMC_RET1(handle, SMC_UNK);
	}
}

/* Define a runtime service descriptor for fast SMC calls */
DECLARE_RT_SVC(
	cortex_a720_cpu_svc,
	OEN_CPU_START,
	OEN_CPU_END,
	SMC_TYPE_FAST,
	cortex_a720_smc_handler
);
