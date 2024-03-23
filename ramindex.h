/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ramindex.h
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

#ifndef _UAPI_LINUX_RAMINDEX_H_
#define _UAPI_LINUX_RAMINDEX_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define RAMINDEX_VERSION_MAJOR 0
#define RAMINDEX_VERSION_MINOR 0
#define RAMINDEX_VERSION_MICRO 7

/**
 * struct ramindex_version - used by RAMINDEX_VERSION ioctl
 * @major:	major version of the driver
 * @minor:	minor version of the driver
 * @micro:	micro version of the driver
 *
 * The structure is used to query current version of the ramindex driver.
 * Major version will change with every incompatible API change.
 * Minor version will change with compatible API changes.
 * Micro version is for micro changes ;-).
 */
struct ramindex_version {
	__s32 major;
	__s32 minor;
	__s32 micro;
};

/**
 * enum ramindex_ctype - indicates possible cache types
 */
enum ramindex_ctype {
	CTYPE_NO_CACHE = 0,
	CTYPE_UNIFIED_CACHE = 0x4,
	CTYPE_SEPARATE_I_AND_D_CACHES = 0x3
};

static inline const char *ramindex_ctype_to_string(enum ramindex_ctype ctype)
{
	switch (ctype) {
	case CTYPE_NO_CACHE:
		return "No cache";
	case CTYPE_UNIFIED_CACHE:
		return "Unified cache";
	case CTYPE_SEPARATE_I_AND_D_CACHES:
		return "Separate instruction and data caches";
	default:
		return "Undefined cache type";
	}
}

/**
 * struct ramindex_clid - identifies the type of caches
 *                        implemented at each level
 *                        (up to a maximum of seven levels).
 * @ctype:	an array mapping cache level with cache type
 * 		(index 0 -> level 1 cache type),
 * 		(index 1 -> level 2 cache type),
 * 		and so on.
 */
struct ramindex_clid {
	enum ramindex_ctype ctype[7];
};

/**
 * struct ramindex_ccsidr - provides information about the architecture/geometry
 *                          of the selected cache
 * @level:	selected cache level
 * @icache:	non-zero if the selected cache is an instruction cache, zero otherwise
 * @nsets:	number of cache sets (filled on return)
 * @nways:	number of cache ways (filled on return)
 * @linesize:	cache line size (filled on return)
 */
struct ramindex_ccsidr {
	__s32 level;
	__s32 icache;
	__s32 nsets;
	__s32 nways;
	__s32 linesize;
};

/**
 * struct ramindex_cacheline - describes one cache line
 * @set:	the set (index within a way) of a cacheline
 * @way:	the way requested cacheline belongs to
 * @valid:	valid bit
 * @dirty:	dirty bit (valid only for data caches)
 * @ns:		non-secure identifier for physical address (tag)
 * @tag:	physical address tag
 * @linesize:	size of the @linedata buffer
 * @linedata:	starting address of a buffer to hold content of a queried line
 *
 * On entry @linesize depicts the size of the @linedata buffer.
 * On return @linesize contains the actual number of bytes
 * copied to @linedata (which is the min(@linesize, actual line size)).
 */
struct ramindex_cacheline {
	__s32 set;
	__s32 way;
	__u8 valid;
	__u8 dirty;
	__u8 ns;
	__u64 tag;
	__u32 linesize;
	void *linedata;
};

/**
 * struct ramindex_selector - used by ioctls to select requested line(s)
 * @level:	selected cache level
 * @icache:	non-zero if the selected cache is an instruction cache, zero otherwise
 * @set:	cache set to be selected (-1 for all sets)
 * @way:	cache way to be selected (-1 for all ways)
 * @nlines:	number of entries in @lines array
 * @lines:	array of @ramindex_cacheline elements
 *
 * The structure is used to locate, select, and copy
 * the requested cache lines to an array of @ramindex_cacheline elements.
 * It may be a single line, whole way, whole set or a whole cache.
 * The passed array shall be large enough to store all the requested lines.
 * If it is not, then of course max @nlines entries/lines will be copied.
 */
struct ramindex_selector {
	__s32 level;
	__s32 icache;
	__s32 set;
	__s32 way;
	__u32 nlines;
	struct ramindex_cacheline *lines;
};

#define RAMINDEX_MAGIC 'r'
#define RAMINDEX_IO(nr)		_IO(RAMINDEX_MAGIC, nr)
#define RAMINDEX_IOR(nr, type)	_IOR(RAMINDEX_MAGIC, nr, type)
#define RAMINDEX_IOW(nr, type)	_IOW(RAMINDEX_MAGIC, nr, type)
#define RAMINDEX_IOWR(nr, type)	_IOWR(RAMINDEX_MAGIC, nr, type)

#define RAMINDEX_VERSION	RAMINDEX_IOR (42, struct ramindex_version)
#define RAMINDEX_CLID		RAMINDEX_IOR (43, struct ramindex_clid)
#define RAMINDEX_CCSIDR		RAMINDEX_IOWR(44, struct ramindex_ccsidr)
#define RAMINDEX_DUMP		RAMINDEX_IOWR(45, struct ramindex_selector)

static inline const char *ramindex_cmd_to_string(size_t cmd)
{
	switch (cmd) {
	case RAMINDEX_VERSION:
		return "RAMINDEX_VERSION";
	case RAMINDEX_CLID:
		return "RAMINDEX_CLID";
	case RAMINDEX_CCSIDR:
		return "RAMINDEX_CCSIDR";
	case RAMINDEX_DUMP:
		return "RAMINDEX_DUMP";
	default:
		return "RAMINDEX_UNRECOGNIZED_COMMAND";
	}
}

#endif /* _UAPI_LINUX_RAMINDEX_H_ */
