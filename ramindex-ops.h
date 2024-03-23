/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ramindex-ops.h
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

#ifndef _RAMINDEX_OPS_H_
#define _RAMINDEX_OPS_H_

#include <linux/types.h>
#include "ramindex.h"

typedef int (*dumpfunction_t)(__s32 set, __s32 way, __u32 lsize, struct ramindex_cacheline __user *l);

/**
 * struct ramindex_ops - ramindex operations
 */
struct ramindex_ops {
	dumpfunction_t dump_l1i_cacheline;
	dumpfunction_t dump_l1d_cacheline;

	dumpfunction_t dump_l2i_cacheline;
	dumpfunction_t dump_l2d_cacheline;

	dumpfunction_t dump_l3i_cacheline;
	dumpfunction_t dump_l3d_cacheline;
};

#endif /* _RAMINDEX_OPS_H_ */
