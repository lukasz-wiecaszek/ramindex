/* SPDX-License-Identifier: MIT */
/**
 * @file ramindex.c
 *
 * Userspace side of the ramindex kernel driver.
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 */

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/ioctl.h>

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include <version.h>
#include "../ramindex.h"

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/
#define RAMINDEX_DEVICENAME "/dev/ramindex"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/*===========================================================================*\
 * local types definitions
\*===========================================================================*/

/*===========================================================================*\
 * local (internal linkage) objects definitions
\*===========================================================================*/

/*===========================================================================*\
 * global (external linkage) objects definitions
\*===========================================================================*/

/*===========================================================================*\
 * local (internal linkage) functions definitions
\*===========================================================================*/
static void ramindex_print_usage(const char* progname)
{
    fprintf(stdout, "%s: [ OPTIONS ]\n", progname);
    fprintf(stdout, "\t-h, --help     this message\n");
    fprintf(stdout, "\t-v, --version  output version information\n");
    fprintf(stdout, "\t-l, --level    select cache level (default: 1)\n");
    fprintf(stdout, "\t-t, --type     select cache type (1 for instruction cache,\n");
    fprintf(stdout, "\t                 0 for data and unified caches, default: 0)\n");
    fprintf(stdout, "\t-s, --set      select cache set (default: -1, all sets)\n");
    fprintf(stdout, "\t-w, --way      select cache way (default: -1, all ways)\n");
}

static void ramindex_print_versions(void)
{
    int fd;
    int status;
    struct ramindex_version version;

    fprintf(stdout, "ramindex (this program) version: %s\n", PROJECT_VER);

    fd = open(RAMINDEX_DEVICENAME, O_RDWR);
    assert(fd >= -1);
    if (fd == -1) {
        fprintf(stderr, "cannot open '%s': %s\n",
            RAMINDEX_DEVICENAME, strerror(errno));
        exit(EXIT_FAILURE);
    }

    memset(&version, 0, sizeof(version));
    status = ioctl(fd, RAMINDEX_VERSION, &version);
    if (status < 0) {
        fprintf(stderr, "ioctl(RAMINDEX_VERSION) failed with code %d : %s\n",
            errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "kernel module version: %d.%d.%d\n", version.major, version.minor, version.micro);

    if (version.major != RAMINDEX_VERSION_MAJOR) {
        fprintf(stderr, "incompatible kernel module/header major version (%d/%d)\n",
            version.major, RAMINDEX_VERSION_MAJOR);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

static int ramindex_get_ccsidr(int fd, int level, int icache, struct ramindex_ccsidr* ccsidr)
{
    int status;

    ccsidr->level = level - 1;
    ccsidr->icache = icache;

    status = ioctl(fd, RAMINDEX_CCSIDR, ccsidr);
    if (status < 0) {
        fprintf(stderr, "ioctl(RAMINDEX_CCSIDR) failed with code %d : %s\n",
            errno, strerror(errno));
        return -1;
    }

    return 0;
}

static void ramindex_print_ccsidr(int fd, int level, int icache)
{
    int status;
    struct ramindex_ccsidr ccsidr;

    memset(&ccsidr, 0, sizeof(ccsidr));
    status = ramindex_get_ccsidr(fd, level, icache, &ccsidr);
    if (status < 0) {
        fprintf(stderr, "Cannot read ccsidr\n");
        return;
    }

    fprintf(stdout, "%s$ (%u KiB):\n", icache ? "I" : "D",
        (ccsidr.linesize * ccsidr.nways * ccsidr.nsets) / 1024);
    fprintf(stdout, "\tLine size: %d\n", ccsidr.linesize);
    fprintf(stdout, "\tNumber of ways: %d\n", ccsidr.nways);
    fprintf(stdout, "\tNumber of sets: %d\n", ccsidr.nsets);
}

static int ramindex_get_clid(int fd, struct ramindex_clid* clid)
{
    int status;
    int level;
    size_t n = 0;

    status = ioctl(fd, RAMINDEX_CLID, clid);
    if (status < 0) {
        fprintf(stderr, "ioctl(RAMINDEX_CLID) failed with code %d : %s\n",
            errno, strerror(errno));
        return -1;
    }

    if (clid->ctype[0] != CTYPE_NO_CACHE) {
        fprintf(stdout, "Cache hierarchy:\n");
        for (; n < ARRAY_SIZE(clid->ctype); n++) {
            if (clid->ctype[n] == CTYPE_NO_CACHE)
                break;
            level = n + 1;
            fprintf(stdout, "L%d -> '%s'\n",
                level, ramindex_ctype_to_string(clid->ctype[n]));
            switch (clid->ctype[n]) {
                case CTYPE_UNIFIED_CACHE:
                    ramindex_print_ccsidr(fd, level, 0);
                    break;
                case CTYPE_SEPARATE_I_AND_D_CACHES:
                    ramindex_print_ccsidr(fd, level, 0);
                    ramindex_print_ccsidr(fd, level, 1);
                    break;
                default:
                    fprintf(stderr, "Detected invalid (%d) cache type\n", clid->ctype[n]);
                    break;
            }
        }
        fprintf(stdout, "\n");
    } else
        fprintf(stdout, "System has no caches\n");

    return n;
}

/*===========================================================================*\
 * global (external linkage) functions definitions
\*===========================================================================*/
int main(int argc, char *argv[])
{
    int i;
    int fd;
    int c;
    int status;
    int ncachelines;
    unsigned n, m;
    char *buf;
    struct ramindex_clid clid;
    struct ramindex_ccsidr ccsidr;
    struct ramindex_cacheline *cachelines;
    struct ramindex_selector selector;
    // cmdline options
    int level = 1;
    int type = 0;
    int set = -1;
    int way = -1;

    static struct option long_options[] = {
        {"help",    no_argument,       0, 'h'},
        {"version", no_argument,       0, 'v'},
        {"level",   required_argument, 0, 'l'},
        {"type",    required_argument, 0, 't'},
        {"set",     required_argument, 0, 's'},
        {"way",     required_argument, 0, 'w'},
        {0, 0, 0, 0}
    };

    for (;;) {
        c = getopt_long(argc, argv, "hvl:t:s:w:", long_options, 0);
        if (c == -1)
            break;

        switch (c) {
            case 'h':
                ramindex_print_usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;

            case 'v':
                ramindex_print_versions();
                exit(EXIT_SUCCESS);
                break;

            case 'l':
                level = atoi(optarg);
                break;

            case 't':
                type = atoi(optarg);
                break;

            case 's':
                set = atoi(optarg);
                break;

            case 'w':
                way = atoi(optarg);
                break;
        }
    }

    fd = open(RAMINDEX_DEVICENAME, O_RDWR);
    assert(fd >= -1);
    if (fd == -1) {
        fprintf(stderr, "Cannot open '%s': %s\n",
            RAMINDEX_DEVICENAME, strerror(errno));
        ramindex_print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    status = ramindex_get_clid(fd, &clid);
    if (status <= 0)
        exit(EXIT_FAILURE);

    if ((level <= 0) || (level > status)) {
        fprintf(stderr, "Cache at level %d is not implemented\n", level);
        exit(EXIT_FAILURE);
    }

    if ((type != 0) && (type != 1)) {
        fprintf(stderr, "Only 0 (data or unified cache) or 1 (instruction cache) cache type args (-t) are recognized\n");
        exit(EXIT_FAILURE);
    }

    if ((clid.ctype[level - 1] == CTYPE_UNIFIED_CACHE) && (type == 1)) {
        fprintf(stderr, "Cache type argument (-t) must be set to 0 for unified caches\n");
        exit(EXIT_FAILURE);
    }

    memset(&ccsidr, 0, sizeof(ccsidr));
    status = ramindex_get_ccsidr(fd, level, type, &ccsidr);
    if (status < 0) {
        fprintf(stderr, "Cannot read ccsidr\n");
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, "Selected cache: L%d '%s' cache\n",
        level, type ? "instruction" : "data/unified");

    ncachelines = ccsidr.nways * ccsidr.nsets;

    buf = malloc(ncachelines * ccsidr.linesize);
    if (buf == NULL) {
        fprintf(stderr, "malloc(%d) failed\n", ncachelines * ccsidr.linesize);
        exit(EXIT_FAILURE);
    }

    cachelines = calloc(ncachelines, sizeof(*cachelines));
    if (cachelines == NULL) {
        fprintf(stderr, "calloc(%d, %zu) failed\n",
            ncachelines, sizeof(*cachelines));
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < ncachelines; i++) {
        cachelines[i].linesize = ccsidr.linesize;
        cachelines[i].linedata = buf + i * ccsidr.linesize;
    }

    memset(&selector, 0, sizeof(selector));
    selector.level = level - 1;
    selector.icache = type;
    selector.set = set;
    selector.way = way;
    selector.nlines = ncachelines;
    selector.lines = cachelines;

    status = ioctl(fd, RAMINDEX_DUMP, &selector);
    if (status < 0) {
        fprintf(stderr, "ioctl(RAMINDEX_DUMP) failed with code %d : %s\n",
            errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    for (n = 0; n < selector.nlines; n++) {
        const struct ramindex_cacheline *l = &selector.lines[n];
        const char *ld = (const char *)l->linedata;
        fprintf(stdout, "SET:%04d WAY:%02d V:%d D:%d NS:%d TAG:%012llx DATA[0:%u] ",
            l->set, l->way, l->valid, l->dirty, l->ns, l->tag, l->linesize - 1);
        for (m = 0; m < l->linesize; m++) {
            fprintf(stdout, "%02x", ld[m]);
            if ((m + 1) % 4 == 0)
                fprintf(stdout, " ");
        }
        fprintf(stdout, "\n");
    }

    free(buf);
    free(cachelines);
    close(fd);

    return 0;
}
