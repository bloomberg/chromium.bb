/* dristat.c -- 
 * Created: Mon Jan 15 05:05:07 2001 by faith@acm.org
 *
 * Copyright 2000 VA Linux Systems, Inc., Fremont, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 * Authors: Rickard E. (Rik) Faith <faith@valinux.com>
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../../../xf86drm.h"
#include "../xf86drmRandom.c"
#include "../xf86drmHash.c"
#include "../xf86drm.c"

#define DRM_DIR_NAME "/dev/dri"
#define DRM_DEV_NAME "%s/card%d"

#define DRM_VERSION 0x00000001
#define DRM_MEMORY  0x00000002
#define DRM_CLIENTS 0x00000004
#define DRM_STATS   0x00000008

typedef struct drmStatsS {
    unsigned long count;
    struct {
	unsigned long value;
	const char    *long_format;
	const char    *long_name;
	const char    *rate_format;
	const char    *rate_name;
	int           isvalue;
	const char    *mult_names;
	int           mult;
	int           verbose;
    } data[15];
} drmStatsT;

static void getversion(int fd)
{
    drmVersionPtr version;
    
    version = drmGetVersion(fd);
    if (version) {
	printf("  Version information:\n");
	printf("    Name: %s\n", version->name ? version->name : "?");
	printf("    Version: %d.%d.%d\n",
	       version->version_major,
	       version->version_minor,
	       version->version_patchlevel);
	printf("    Date: %s\n", version->date ? version->date : "?");
	printf("    Desc: %s\n", version->desc ? version->desc : "?");
	drmFreeVersion(version);
    } else {
	printf("  No version information available\n");
    }
}

typedef struct {
    unsigned long	offset;	 /* Requested physical address (0 for SAREA)*/
    unsigned long	size;	 /* Requested physical size (bytes)	    */
    drm_map_type_t	type;	 /* Type of memory to map		    */
    drm_map_flags_t flags;	 /* Flags				    */
    void		*handle; /* User-space: "Handle" to pass to mmap    */
    /* Kernel-space: kernel-virtual address    */
    int		mtrr;	 /* MTRR slot used			    */
				 /* Private data			    */
} drmVmRec, *drmVmPtr;

int drmGetMap(int fd, int idx, drmHandle *offset, drmSize *size,
	      drmMapType *type, drmMapFlags *flags, drmHandle *handle,
	      int *mtrr)
{
    drm_map_t map;

    map.offset = idx;
    if (ioctl(fd, DRM_IOCTL_GET_MAP, &map)) return -errno;
    *offset = map.offset;
    *size   = map.size;
    *type   = map.type;
    *flags  = map.flags;
    *handle = (unsigned long)map.handle;
    *mtrr   = map.mtrr;
    return 0;
}

int drmGetClient(int fd, int idx, int *auth, int *pid, int *uid,
		 unsigned long *magic, unsigned long *iocs)
{
    drm_client_t client;

    client.idx = idx;
    if (ioctl(fd, DRM_IOCTL_GET_CLIENT, &client)) return -errno;
    *auth      = client.auth;
    *pid       = client.pid;
    *uid       = client.uid;
    *magic     = client.magic;
    *iocs      = client.iocs;
    return 0;
}

int drmGetStats(int fd, drmStatsT *stats)
{
    drm_stats_t s;
    int         i;

    if (ioctl(fd, DRM_IOCTL_GET_STATS, &s)) return -errno;

    stats->count = 0;
    memset(stats, 0, sizeof(*stats));
    if (s.count > sizeof(stats->data)/sizeof(stats->data[0]))
	return -1;

#define SET_VALUE                              \
    stats->data[i].long_format = "%-20.20s";   \
    stats->data[i].rate_format = "%8.8s";      \
    stats->data[i].isvalue     = 1;            \
    stats->data[i].verbose     = 0

#define SET_COUNT                              \
    stats->data[i].long_format = "%-20.20s";     \
    stats->data[i].rate_format = "%5.5s";      \
    stats->data[i].isvalue     = 0;            \
    stats->data[i].mult_names  = "kgm";        \
    stats->data[i].mult        = 1000;         \
    stats->data[i].verbose     = 0

#define SET_BYTE                             \
    stats->data[i].long_format = "%-9.9s";   \
    stats->data[i].rate_format = "%5.5s";    \
    stats->data[i].isvalue     = 0;          \
    stats->data[i].mult_names  = "KGM";      \
    stats->data[i].mult        = 1024;       \
    stats->data[i].verbose     = 0


    stats->count = s.count;
    for (i = 0; i < s.count; i++) {
	stats->data[i].value = s.data[i].value;
	switch (s.data[i].type) {
	case _DRM_STAT_LOCK:
	    stats->data[i].long_name = "Lock";
	    stats->data[i].rate_name = "Lock";
	    SET_VALUE;
	    break;
	case _DRM_STAT_OPENS:
	    stats->data[i].long_name = "Opens";
	    stats->data[i].rate_name = "O";
	    SET_COUNT;
	    stats->data[i].verbose   = 1;
	    break;
	case _DRM_STAT_CLOSES:
	    stats->data[i].long_name = "Closes";
	    stats->data[i].rate_name = "Lock";
	    SET_COUNT;
	    stats->data[i].verbose   = 1;
	    break;
	case _DRM_STAT_IOCTLS:
	    stats->data[i].long_name = "Ioctls";
	    stats->data[i].rate_name = "Ioc/s";
	    SET_COUNT;
	    break;
	case _DRM_STAT_LOCKS:
	    stats->data[i].long_name = "Locks";
	    stats->data[i].rate_name = "Lck/s";
	    SET_COUNT;
	    break;
	case _DRM_STAT_UNLOCKS:
	    stats->data[i].long_name = "Unlocks";
	    stats->data[i].rate_name = "Unl/s";
	    SET_COUNT;
	    break;
	case _DRM_STAT_IRQ:
	    stats->data[i].long_name = "IRQs";
	    stats->data[i].rate_name = "IRQ/s";
	    SET_COUNT;
	    break;
	case _DRM_STAT_PRIMARY:
	    stats->data[i].long_name = "Primary Bytes";
	    stats->data[i].rate_name = "PB/s";
	    SET_BYTE;
	    break;
	case _DRM_STAT_SECONDARY:
	    stats->data[i].long_name = "Secondary Bytes";
	    stats->data[i].rate_name = "SB/s";
	    SET_BYTE;
	    break;
	case _DRM_STAT_DMA:
	    stats->data[i].long_name = "DMA";
	    stats->data[i].rate_name = "DMA/s";
	    SET_COUNT;
	    break;
	case _DRM_STAT_SPECIAL:
	    stats->data[i].long_name = "Special DMA";
	    stats->data[i].rate_name = "dma/s";
	    SET_COUNT;
	    break;
	case _DRM_STAT_MISSED:
	    stats->data[i].long_name = "Miss";
	    stats->data[i].rate_name = "Ms/s";
	    SET_COUNT;
	    break;
	case _DRM_STAT_VALUE:
	    stats->data[i].long_name = "Value";
	    stats->data[i].rate_name = "Value";
	    SET_VALUE;
	    break;
	case _DRM_STAT_BYTE:
	    stats->data[i].long_name = "Bytes";
	    stats->data[i].rate_name = "B/s";
	    SET_BYTE;
	    break;
	case _DRM_STAT_COUNT:
	default:
	    stats->data[i].long_name = "Count";
	    stats->data[i].rate_name = "Cnt/s";
	    SET_COUNT;
	    break;
	}
    }
    return 0;
}

static void getvm(int fd)
{
    int             i;
    const char      *typename;
    char            flagname[33];
    drmHandle       offset;
    drmSize         size;
    drmMapType      type;
    drmMapFlags     flags;
    drmHandle       handle;
    int             mtrr;

    printf("  VM map information:\n");
    printf("    slot     offset       size type flags    address mtrr\n");

    for (i = 0;
	 !drmGetMap(fd, i, &offset, &size, &type, &flags, &handle, &mtrr);
	 i++) {
	
	switch (type) {
	case DRM_FRAME_BUFFER: typename = "FB";  break;
	case DRM_REGISTERS:    typename = "REG"; break;
	case DRM_SHM:          typename = "SHM"; break;
	case DRM_AGP:          typename = "AGP"; break;
	default:               typename = "???"; break;
	}

	flagname[0] = (flags & DRM_RESTRICTED)      ? 'R' : ' ';
	flagname[1] = (flags & DRM_READ_ONLY)       ? 'r' : 'w';
	flagname[2] = (flags & DRM_LOCKED)          ? 'l' : ' ';
	flagname[3] = (flags & DRM_KERNEL)          ? 'k' : ' ';
	flagname[4] = (flags & DRM_WRITE_COMBINING) ? 'W' : ' ';
	flagname[5] = (flags & DRM_CONTAINS_LOCK)   ? 'L' : ' ';
	flagname[6] = '\0';
	
	printf("    %4d 0x%08lx 0x%08lx %3.3s %6.6s 0x%08lx ",
	       i, offset, (unsigned long)size, typename, flagname, handle);
	if (mtrr < 0) printf("none\n");
	else          printf("%4d\n", mtrr);
    }
}

static void getclients(int fd)
{
    int           i;
    int           auth;
    int           pid;
    int           uid;
    unsigned long magic;
    unsigned long iocs;
    char          buf[64];
    char          cmd[40];
    int           procfd;
    
    printf("  DRI client information:\n");
    printf("    a   pid    uid      magic     ioctls  prog\n");

    for (i = 0; !drmGetClient(fd, i, &auth, &pid, &uid, &magic, &iocs); i++) {
	sprintf(buf, "/proc/%d/cmdline", pid);
	memset(cmd, sizeof(cmd), 0);
	if ((procfd = open(buf, O_RDONLY, 0)) >= 0) {
	    read(procfd, cmd, sizeof(cmd)-1);
	    close(procfd);
	}
	if (*cmd)
	    printf("    %c %5d %5d %10lu %10lu   %s\n",
		   auth ? 'y' : 'n', pid, uid, magic, iocs, cmd);
	else
	    printf("    %c %5d %5d %10lu %10lu\n",
		   auth ? 'y' : 'n', pid, uid, magic, iocs);
    }
}

static void printhuman(unsigned long value, const char *name, int mult)
{
    const char *p;
    double     f;
				/* Print width 5 number in width 6 space */
    if (value < 100000) {
	printf(" %5lu", value);
	return;
    }

    p = name;
    f = (double)value / (double)mult;
    if (f < 10.0) {
	printf(" %4.2f%c", f, *p);
	return;
    }

    p++;
    f = (double)value / (double)mult;
    if (f < 10.0) {
	printf(" %4.2f%c", f, *p);
	return;
    }
    
    p++;
    f = (double)value / (double)mult;
    if (f < 10.0) {
	printf(" %4.2f%c", f, *p);
	return;
    }
}

static void getstats(int fd, int i)
{
    drmStatsT prev, curr;
    int       j;
    double    rate;
    
    printf("  System statistics:\n");

    if (drmGetStats(fd, &prev)) return;
    if (!i) {
	for (j = 0; j < prev.count; j++) {
	    printf("    ");
	    printf(prev.data[j].long_format, prev.data[j].long_name);
	    if (prev.data[j].isvalue) printf(" 0x%08lx\n", prev.data[j].value);
	    else                      printf(" %10lu\n", prev.data[j].value);
	}
	return;
    }

    printf("    ");
    for (j = 0; j < prev.count; j++)
	if (!prev.data[j].verbose) {
	    printf(" ");
	    printf(prev.data[j].rate_format, prev.data[j].rate_name);
	}
    printf("\n");
    
    for (;;) {
	sleep(i);
	if (drmGetStats(fd, &curr)) return;
	printf("    ");
	for (j = 0; j < curr.count; j++) {
	    if (curr.data[j].verbose) continue;
	    if (curr.data[j].isvalue) {
		printf(" %08lx", curr.data[j].value);
	    } else {
		rate = (curr.data[j].value - prev.data[j].value) / (double)i;
		printhuman(rate, curr.data[j].mult_names, curr.data[j].mult);
	    }
	}
	printf("\n");
	memcpy(&prev, &curr, sizeof(prev));
    }
    
}

static int drmOpenMinor(int minor, uid_t user, gid_t group,
			mode_t dirmode, mode_t devmode, int force)
{
    struct stat st;
    char        buf[64];
    long        dev    = makedev(DRM_MAJOR, minor);
    int         setdir = 0;
    int         setdev = 0;
    int         fd;

    if (stat(DRM_DIR_NAME, &st) || !S_ISDIR(st.st_mode)) {
	remove(DRM_DIR_NAME);
	mkdir(DRM_DIR_NAME, dirmode);
	++setdir;
    }

    if (force || setdir) {
	chown(DRM_DIR_NAME, user, group);
	chmod(DRM_DIR_NAME, dirmode);
    }

    sprintf(buf, DRM_DEV_NAME, DRM_DIR_NAME, minor);
    if (stat(buf, &st) || st.st_rdev != dev) {
	remove(buf);
	mknod(buf, S_IFCHR, dev);
	++setdev;
    }

    if (force || setdev) {
	chown(buf, user, group);
	chmod(buf, devmode);
    }

    if ((fd = open(buf, O_RDWR, 0)) >= 0) return fd;
    if (setdev) remove(buf);
    return -errno;
}
	
int main(int argc, char **argv)
{
    int  c;
    int  mask     = 0;
    int  minor    = 0;
    int  interval = 0;
    int  fd;
    char buf[64];
    int  i;

    while ((c = getopt(argc, argv, "avmcsM:i:")) != EOF)
	switch (c) {
	case 'a': mask = ~0;                          break;
	case 'v': mask |= DRM_VERSION;                break;
	case 'm': mask |= DRM_MEMORY;                 break;
	case 'c': mask |= DRM_CLIENTS;                break;
	case 's': mask |= DRM_STATS;                  break;
	case 'i': interval = strtol(optarg, NULL, 0); break;
	case 'M': minor = strtol(optarg, NULL, 0);    break;
	default:
	    fprintf( stderr, "Usage: dristat [options]\n" );
	    return 1;
	}

    for (i = 0; i < 16; i++) if (!minor || i == minor) {
	sprintf(buf, DRM_DEV_NAME, DRM_DIR_NAME, i);
	fd = drmOpenMinor(i, 0, 0, 0700, 0600, 0);
	if (fd >= 0) {
	    printf("%s\n", buf);
	    if (mask & DRM_VERSION) getversion(fd);
	    if (mask & DRM_MEMORY)  getvm(fd);
	    if (mask & DRM_CLIENTS) getclients(fd);
	    if (mask & DRM_STATS)   getstats(fd, interval);
	    close(fd);
	}
    }

    return 0; 
}
