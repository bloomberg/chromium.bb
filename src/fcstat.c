/*
 * Copyright © 2000 Keith Packard
 * Copyright © 2005 Patrick Lam
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "fcint.h"
#include "fcarch.h"
#include <dirent.h>

#ifdef _WIN32

#include <windows.h>

#ifdef __GNUC__
typedef long long INT64;
#define EPOCH_OFFSET 11644473600ll
#else
#define EPOCH_OFFSET 11644473600i64
typedef __int64 INT64;
#endif

/* Workaround for problems in the stat() in the Microsoft C library:
 *
 * 1) stat() uses FindFirstFile() to get the file
 * attributes. Unfortunately this API doesn't return correct values
 * for modification time of a directory until some time after a file
 * or subdirectory has been added to the directory. (This causes
 * run-test.sh to fail, for instance.) GetFileAttributesEx() is
 * better, it returns the updated timestamp right away.
 *
 * 2) stat() does some strange things related to backward
 * compatibility with the local time timestamps on FAT volumes and
 * daylight saving time. This causes problems after the switches
 * to/from daylight saving time. See
 * http://bugzilla.gnome.org/show_bug.cgi?id=154968 , especially
 * comment #30, and http://www.codeproject.com/datetime/dstbugs.asp .
 * We don't need any of that, FAT and Win9x are as good as dead. So
 * just use the UTC timestamps from NTFS, converted to the Unix epoch.
 */

int
FcStat (const FcChar8 *file, struct stat *statb)
{
    WIN32_FILE_ATTRIBUTE_DATA wfad;
    char full_path_name[MAX_PATH];
    char *basename;
    DWORD rc;

    if (!GetFileAttributesEx (file, GetFileExInfoStandard, &wfad))
	return -1;

    statb->st_dev = 0;

    /* Calculate a pseudo inode number as a hash of the full path name.
     * Call GetLongPathName() to get the spelling of the path name as it
     * is on disk.
     */
    rc = GetFullPathName (file, sizeof (full_path_name), full_path_name, &basename);
    if (rc == 0 || rc > sizeof (full_path_name))
	return -1;

    rc = GetLongPathName (full_path_name, full_path_name, sizeof (full_path_name));
    statb->st_ino = FcStringHash (full_path_name);

    statb->st_mode = _S_IREAD | _S_IWRITE;
    statb->st_mode |= (statb->st_mode >> 3) | (statb->st_mode >> 6);

    if (wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	statb->st_mode |= _S_IFDIR;
    else
	statb->st_mode |= _S_IFREG;

    statb->st_nlink = 1;
    statb->st_uid = statb->st_gid = 0;
    statb->st_rdev = 0;

    if (wfad.nFileSizeHigh > 0)
	return -1;
    statb->st_size = wfad.nFileSizeLow;

    statb->st_atime = (*(INT64 *)&wfad.ftLastAccessTime)/10000000 - EPOCH_OFFSET;
    statb->st_mtime = (*(INT64 *)&wfad.ftLastWriteTime)/10000000 - EPOCH_OFFSET;
    statb->st_ctime = statb->st_mtime;

    return 0;
}

#else

int
FcStat (const FcChar8 *file, struct stat *statb)
{
  return stat ((char *) file, statb);
}

#endif
