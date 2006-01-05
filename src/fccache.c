/*
 * $RCSId: xc/lib/fontconfig/src/fccache.c,v 1.12 2002/08/22 07:36:44 keithp Exp $
 *
 * Copyright © 2000 Keith Packard
 * Copyright © 2005 Patrick Lam
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <unistd.h>
#include "fcint.h"
#include <unistd.h>

#define ENDIAN_TEST 0x12345678
#define MACHINE_SIGNATURE_SIZE 9 + 5*20 + 1

static int
FcDirCacheOpen (char * cache_file);

static char *
FcDirCacheHashName (char * cache_file, int collisions);

static off_t
FcCacheSkipToArch (int fd, const char * arch);

static FcBool 
FcCacheCopyOld (int fd, int fd_orig, off_t start);

static void *
FcDirCacheProduce (FcFontSet *set, FcCache * metadata);

static FcBool
FcDirCacheConsume (int fd, const char * dir, FcFontSet *set);

FcBool
FcDirCacheRead (FcFontSet * set, FcStrSet * dirs, const FcChar8 *dir);

static int
FcCacheNextOffset(off_t w);

static char *
FcCacheMachineSignature (void);

static FcBool
FcCacheHaveBank (int bank);

static void
FcCacheAddBankDir (int bank, const char * dir);

struct MD5Context {
        FcChar32 buf[4];
        FcChar32 bits[2];
        unsigned char in[64];
};

static void MD5Init(struct MD5Context *ctx);
static void MD5Update(struct MD5Context *ctx, unsigned char *buf, unsigned len);
static void MD5Final(unsigned char digest[16], struct MD5Context *ctx);
static void MD5Transform(FcChar32 buf[4], FcChar32 in[16]);

#define FC_DBG_CACHE_REF    1024

static char *
FcCacheReadString (int fd, char *dest, int len)
{
    FcChar8	c;
    FcBool	escape;
    int		size;
    int		i;

    if (len == 0)
	return 0;
    
    size = len;
    i = 0;
    escape = FcFalse;
    while (read (fd, &c, 1) == 1)
    {
	if (!escape)
	{
	    switch (c) {
	    case '"':
		c = '\0';
		break;
	    case '\\':
		escape = FcTrue;
		continue;
	    }
	}
	if (i == size)
	{
	    dest[i++] = 0;
	    return dest;
	}
	dest[i++] = c;
	if (c == '\0')
	    return dest;
	escape = FcFalse;
    }
    return 0;
}

static void
FcCacheSkipString (int fd)
{
    FcChar8	c;
    FcBool	escape;

    escape = FcFalse;
    while (read (fd, &c, 1) == 1)
    {
	if (!escape)
	{
	    switch (c) {
	    case '"':
		c = '\0';
		break;
	    case '\\':
		escape = FcTrue;
		continue;
	    }
	}
	if (c == '\0')
	    return;
	escape = FcFalse;
    }
    return;
}

static FcBool
FcCacheWriteString (int fd, const char *chars)
{
    if (write (fd, chars, strlen(chars)+1) != strlen(chars)+1)
	return FcFalse;
    return FcTrue;
}

static void
FcGlobalCacheDirDestroy (FcGlobalCacheDir *d)
{
    FcMemFree (FC_MEM_STRING, strlen (d->name)+1);
    free (d->name);
    FcMemFree (FC_MEM_CACHE, sizeof (FcGlobalCacheDir));
    free (d);
}

FcGlobalCache *
FcGlobalCacheCreate (void)
{
    FcGlobalCache   *cache;

    cache = malloc (sizeof (FcGlobalCache));
    if (!cache)
	return 0;
    FcMemAlloc (FC_MEM_CACHE, sizeof (FcGlobalCache));
    cache->dirs = 0;
    cache->updated = FcFalse;
    cache->fd = -1;
    return cache;
}

void
FcGlobalCacheDestroy (FcGlobalCache *cache)
{
    FcGlobalCacheDir	*d, *next;

    for (d = cache->dirs; d; d = next)
    {
	next = d->next;
	FcGlobalCacheDirDestroy (d);
    }
    FcMemFree (FC_MEM_CACHE, sizeof (FcGlobalCache));
    free (cache);
}

void
FcGlobalCacheLoad (FcGlobalCache    *cache,
                   FcStrSet	    *staleDirs,
		   const FcChar8    *cache_file,
		   FcConfig	    *config)
{
    char		name_buf[FC_MAX_FILE_LEN];
    FcGlobalCacheDir	*d, *next;
    FcFileTime		config_time = FcConfigModifiedTime (config);
    char 		* current_arch_machine_name;
    char 		candidate_arch_machine_name[MACHINE_SIGNATURE_SIZE + 9];
    off_t		current_arch_start;

    struct stat 	cache_stat, dir_stat;

    if (stat ((char *) cache_file, &cache_stat) < 0)
        return;

    cache->fd = open ((char *) cache_file, O_RDONLY);
    if (cache->fd == -1)
	return;

    cache->updated = FcFalse;

    FcCacheReadString (cache->fd, name_buf, sizeof (name_buf));
    if (strcmp (name_buf, FC_GLOBAL_MAGIC_COOKIE) != 0)
	return;

    current_arch_machine_name = FcCacheMachineSignature ();
    current_arch_start = FcCacheSkipToArch(cache->fd, 
					   current_arch_machine_name);
    if (current_arch_start < 0)
        goto bail_and_destroy;

    lseek (cache->fd, current_arch_start, SEEK_SET);
    FcCacheReadString (cache->fd, candidate_arch_machine_name, 
                       sizeof (candidate_arch_machine_name));
    if (strlen(candidate_arch_machine_name) == 0)
	goto bail_and_destroy;

    while (1) 
    {
	off_t targ;

	FcCacheReadString (cache->fd, name_buf, sizeof (name_buf));
	if (!strlen(name_buf))
	    break;

	/* Directory must be older than the global cache file; also
	   cache must be newer than the config file. */
        if (stat ((char *) name_buf, &dir_stat) < 0 || 
            dir_stat.st_mtime > cache_stat.st_mtime ||
	    (config_time.set && cache_stat.st_mtime < config_time.time))
        {
            FcCache md;

            FcStrSetAdd (staleDirs, FcStrCopy ((FcChar8 *)name_buf));
            read (cache->fd, &md, sizeof (FcCache));
            lseek (cache->fd, FcCacheNextOffset (lseek(cache->fd, 0, SEEK_CUR)) + md.count, SEEK_SET);
            continue;
        }

	d = malloc (sizeof (FcGlobalCacheDir));
	if (!d)
	    goto bail1;

	d->next = cache->dirs;
	cache->dirs = d;

	d->name = (char *)FcStrCopy ((FcChar8 *)name_buf);
	d->ent = 0;
	d->offset = lseek (cache->fd, 0, SEEK_CUR);
	if (read (cache->fd, &d->metadata, sizeof (FcCache)) != sizeof (FcCache))
	    goto bail1;
	targ = FcCacheNextOffset (lseek(cache->fd, 0, SEEK_CUR)) + d->metadata.count;
	if (lseek (cache->fd, targ, SEEK_SET) != targ)
	    goto bail1;
    }
    return;

 bail1:
    for (d = cache->dirs; d; d = next)
    {
	next = d->next;
	free (d);
    }
    cache->dirs = 0;

    close (cache->fd);
    cache->fd = -1;
    return;

 bail_and_destroy:
    close (cache->fd);
    cache->fd = -1;

    if (stat ((char *) cache_file, &cache_stat) == 0)
        unlink ((char *)cache_file);

    return;

}

FcBool
FcGlobalCacheReadDir (FcFontSet *set, FcStrSet *dirs, FcGlobalCache * cache, const char *dir, FcConfig *config)
{
    FcGlobalCacheDir *d;
    FcBool ret = FcFalse;

    if (cache->fd == -1)
	return FcFalse;

    for (d = cache->dirs; d; d = d->next)
    {
	if (strncmp (d->name, dir, strlen(dir)) == 0)
	{
	    lseek (cache->fd, d->offset, SEEK_SET);
	    if (!FcDirCacheConsume (cache->fd, dir, set))
		return FcFalse;
            if (strcmp (d->name, dir) == 0)
		ret = FcTrue;
	}
    }

    return ret;
}

FcBool
FcGlobalCacheUpdate (FcGlobalCache  *cache,
		     const char     *name,
		     FcFontSet	    *set)
{
    FcGlobalCacheDir * d;

    for (d = cache->dirs; d; d = d->next)
    {
	if (strcmp(d->name, name) == 0)
	    break;
    }

    if (!d)
    {
	d = malloc (sizeof (FcGlobalCacheDir));
	if (!d)
	    return FcFalse;
	d->next = cache->dirs;
	cache->dirs = d;
    }

    cache->updated = FcTrue;

    d->name = (char *)FcStrCopy ((FcChar8 *)name);
    d->ent = FcDirCacheProduce (set, &d->metadata);
    d->offset = 0;
    return FcTrue;
}

FcBool
FcGlobalCacheSave (FcGlobalCache    *cache,
		   const FcChar8    *cache_file)
{
    int			fd, fd_orig;
    FcGlobalCacheDir	*dir;
    FcAtomic		*atomic;
    off_t 		current_arch_start = 0, truncate_to;
    char 		* current_arch_machine_name, * header;

    if (!cache->updated)
	return FcTrue;
    
#if defined (HAVE_GETUID) && defined (HAVE_GETEUID)
    /* Set-UID programs can't safely update the cache */
    if (getuid () != geteuid ())
	return FcFalse;
#endif
    
    atomic = FcAtomicCreate (cache_file);
    if (!atomic)
	return FcFalse;

    if (!FcAtomicLock (atomic))
	goto bail1;
    fd = open ((char *) FcAtomicNewFile(atomic), O_RDWR | O_CREAT, 
	       S_IRUSR | S_IWUSR);
    if (fd == -1)
	goto bail2;

    fd_orig = open ((char *) FcAtomicOrigFile(atomic), O_RDONLY);

    current_arch_machine_name = FcCacheMachineSignature ();
    if (fd_orig == -1)
        current_arch_start = 0;
    else
        current_arch_start = FcCacheSkipToArch (fd_orig, 
                                                current_arch_machine_name);

    if (current_arch_start < 0)
	current_arch_start = FcCacheNextOffset (lseek(fd_orig, 0, SEEK_END));

    if (!FcCacheCopyOld(fd, fd_orig, current_arch_start))
	goto bail3;

    close (fd_orig);
    fd_orig = -1;

    current_arch_start = lseek(fd, 0, SEEK_CUR);
    if (ftruncate (fd, current_arch_start) == -1)
	goto bail3;

    header = malloc (10 + strlen (current_arch_machine_name));
    if (!header)
	goto bail3;

    truncate_to = current_arch_start + strlen(current_arch_machine_name) + 11;
    for (dir = cache->dirs; dir; dir = dir->next)
    {
	truncate_to += strlen(dir->name) + 1;
	truncate_to += sizeof (FcCache);
	truncate_to = FcCacheNextOffset (truncate_to);
	truncate_to += dir->metadata.count;
    }
    truncate_to -= current_arch_start;

    FcCacheWriteString (fd, FC_GLOBAL_MAGIC_COOKIE);
    sprintf (header, "%8x ", (int)truncate_to);
    strcat (header, current_arch_machine_name);
    if (!FcCacheWriteString (fd, header))
	goto bail4;

    for (dir = cache->dirs; dir; dir = dir->next)
    {
        if (dir->name)
        {
            FcCacheWriteString (fd, dir->name);
            write (fd, &dir->metadata, sizeof(FcCache));
            lseek (fd, FcCacheNextOffset (lseek(fd, 0, SEEK_CUR)), SEEK_SET);
            write (fd, dir->ent, dir->metadata.count);
            free (dir->ent);
        }
    }
    FcCacheWriteString (fd, "");

    if (close (fd) == -1)
	goto bail25;
    
    if (!FcAtomicReplaceOrig (atomic))
	goto bail25;
    
    FcAtomicUnlock (atomic);
    FcAtomicDestroy (atomic);

    cache->updated = FcFalse;
    return FcTrue;

 bail4:
    free (header);
 bail3:
    if (fd_orig != -1)
        close (fd_orig);

    close (fd);
 bail25:
    FcAtomicDeleteNew (atomic);
 bail2:
    FcAtomicUnlock (atomic);
 bail1:
    FcAtomicDestroy (atomic);
    return FcFalse;
}

/* 
 * Find the next presumably-mmapable offset after the supplied file
 * position.
 */
static int
FcCacheNextOffset(off_t w)
{
    static long pagesize = -1;
    if (pagesize == -1)
	pagesize = sysconf(_SC_PAGESIZE);
    if (w % pagesize == 0) 
	return w;
    else
	return ((w / pagesize)+1)*pagesize;
}

/* return the address of the segment for the provided arch,
 * or -1 if arch not found */
static off_t
FcCacheSkipToArch (int fd, const char * arch)
{
    char candidate_arch_machine_name_count[MACHINE_SIGNATURE_SIZE + 9];
    char * candidate_arch;
    off_t current_arch_start = 0;

    lseek (fd, 0, SEEK_SET);
    FcCacheSkipString (fd);
    current_arch_start = lseek (fd, 0, SEEK_CUR);

    /* skip arches that are not the current arch */
    while (1)
    {
	long bs;

	if (lseek (fd, current_arch_start, SEEK_SET) != current_arch_start)
            return -1;

	if (FcCacheReadString (fd, candidate_arch_machine_name_count, 
				sizeof (candidate_arch_machine_name_count)) == 0)
            return -1;
	if (!strlen(candidate_arch_machine_name_count))
	    return -1;
	bs = strtol(candidate_arch_machine_name_count, &candidate_arch, 16);

	// count = 0 should probably be distinguished from the !bs condition
	if (!bs || bs < strlen (candidate_arch_machine_name_count))
	    return -1;

	candidate_arch++; /* skip leading space */

	if (strcmp (candidate_arch, arch)==0)
	    return current_arch_start;
	current_arch_start += bs;
    }

    return -1;
}

/* Cuts out the segment at the file pointer (moves everything else
 * down to cover it), and leaves the file pointer at the end of the
 * file. */
static FcBool 
FcCacheCopyOld (int fd, int fd_orig, off_t start)
{
    char * buf = malloc (8192);
    char candidate_arch_machine_name[MACHINE_SIGNATURE_SIZE + 9];
    long bs;
    int c, bytes_skipped;
    off_t loc;

    if (!buf)
	return FcFalse;

    loc = 0;
    lseek (fd, 0, SEEK_SET); lseek (fd_orig, 0, SEEK_SET);
    do
    {
        int b = 8192;
        if (loc + b > start)
            b = start - loc;

	if ((c = read (fd_orig, buf, b)) <= 0)
	    break;
	if (write (fd, buf, c) < 0)
	    goto bail;

        loc += c;
    }
    while (c > 0);

    lseek (fd, start, SEEK_SET);
    if (FcCacheReadString (fd, candidate_arch_machine_name, 
			   sizeof (candidate_arch_machine_name)) == 0)
	goto done;
    if (!strlen(candidate_arch_machine_name))
	goto done;

    bs = strtol(candidate_arch_machine_name, 0, 16);
    if (bs == 0)
	goto done;

    bytes_skipped = 0;
    do
    {
	lseek (fd, start+bs+bytes_skipped, SEEK_SET);
	if ((c = read (fd, buf, 8192)) <= 0)
	    break;
	lseek (fd, start+bytes_skipped, SEEK_SET);
	if (write (fd, buf, c) < 0)
	    goto bail;
	bytes_skipped += c;
    }
    while (c > 0);
    lseek (fd, start+bytes_skipped, SEEK_SET);

 done:
    free (buf);
    return FcTrue;

 bail:
    free (buf);
    return FcFalse;
}

/* Does not check that the cache has the appropriate arch section. */
/* Also, this can be fooled if the original location has a stale
 * cache, and the hashed location has an up-to-date cache.  Oh well,
 * sucks to be you in that case! */
FcBool
FcDirCacheValid (const FcChar8 *dir)
{
    FcChar8	*cache_file;
    struct stat file_stat, dir_stat;
    int 	fd;

    if (stat ((char *) dir, &dir_stat) < 0)
        return FcFalse;

    cache_file = FcStrPlus (dir, (FcChar8 *) "/" FC_DIR_CACHE_FILE);
    if (!cache_file)
	return FcFalse;

    fd = FcDirCacheOpen ((char *)cache_file);

    if (fd < 0)
	goto bail;
    if (fstat (fd, &file_stat) < 0)
	goto bail1;

    close (fd);
    FcStrFree (cache_file);

    /*
     * If the directory has been modified more recently than
     * the cache file, the cache is not valid
     */
    if (dir_stat.st_mtime > file_stat.st_mtime)
        return FcFalse;

    return FcTrue;

 bail1:
    close (fd);
 bail:
    FcStrFree (cache_file);
    return FcFalse;
}

/* Assumes that the cache file in 'dir' exists.
 * Checks that the cache has the appropriate arch section. */
FcBool
FcDirCacheHasCurrentArch (const FcChar8 *dir)
{
    char	*cache_file;
    int 	fd;
    off_t	current_arch_start;
    char 	*current_arch_machine_name;

    cache_file = (char *)FcStrPlus (dir, (FcChar8 *) "/" FC_DIR_CACHE_FILE);
    if (!cache_file)
	return FcFalse;

    fd = FcDirCacheOpen (cache_file);
    if (fd < 0)
	goto bail;

    current_arch_machine_name = FcCacheMachineSignature();
    current_arch_start = FcCacheSkipToArch(fd, current_arch_machine_name);
    close (fd);

    if (current_arch_start < 0)
        return FcFalse;
    
    return FcTrue;

 bail:
    free (cache_file);
    return FcFalse;
}

FcBool
FcDirCacheUnlink (const FcChar8 *dir)
{
    char	*cache_file = (char *)FcStrPlus (dir, (FcChar8 *) "/" FC_DIR_CACHE_FILE);
    char	*cache_hashed;
    int		fd, collisions;
    struct stat	cache_stat;
    char	name_buf[FC_MAX_FILE_LEN];

    /* First remove normal cache file. */
    if (stat ((char *) cache_file, &cache_stat) == 0 &&
	unlink ((char *)cache_file) != 0)
	goto bail;

    /* Next remove any applicable hashed files. */
    fd = -1; collisions = 0;
    do
    {
	cache_hashed = FcDirCacheHashName (cache_file, collisions++);
	if (!cache_hashed)
	    goto bail;

	if (fd > 0)
	    close (fd);
	fd = open(cache_hashed, O_RDONLY);
	if (fd == -1)
	{
	    FcStrFree ((FcChar8 *)cache_file);
	    return FcTrue;
	}

	FcCacheReadString (fd, name_buf, sizeof (name_buf));
	if (!strlen(name_buf))
	    goto bail;
    } while (strcmp (name_buf, cache_file) != 0);

    FcStrFree ((FcChar8 *)cache_file);
    close (fd);

    if (stat ((char *) cache_hashed, &cache_stat) == 0 &&
	unlink ((char *)cache_hashed) != 0)
    {
	FcStrFree ((FcChar8 *)cache_hashed);
	goto bail;
    }

    FcStrFree ((FcChar8 *)cache_hashed);
    return FcTrue;

 bail:
    FcStrFree ((FcChar8 *)cache_file);
    return FcFalse;
}

static int
FcCacheReadDirs (FcConfig * config, FcGlobalCache * cache, 
		 FcStrList *list, FcFontSet * set)
{
    int			ret = 0;
    FcChar8		*dir;
    FcChar8		*file, *base;
    FcStrSet		*subdirs;
    FcStrList		*sublist;
    struct stat		statb;

    /*
     * Read in the results from 'list'.
     */
    while ((dir = FcStrListNext (list)))
    {
	if (!FcConfigAcceptFilename (config, dir))
	    continue;

	/* freed below */
	file = (FcChar8 *) malloc (strlen ((char *) dir) + 1 + FC_MAX_FILE_LEN + 1);
	if (!file)
	    return FcFalse;

	strcpy ((char *) file, (char *) dir);
	strcat ((char *) file, "/");
	base = file + strlen ((char *) file);

	subdirs = FcStrSetCreate ();
	if (!subdirs)
	{
	    fprintf (stderr, "Can't create directory set\n");
	    ret++;
	    free (file);
	    continue;
	}
	
	if (access ((char *) dir, X_OK) < 0)
	{
	    switch (errno) {
	    case ENOENT:
	    case ENOTDIR:
	    case EACCES:
		break;
	    default:
		fprintf (stderr, "\"%s\": ", dir);
		perror ("");
		ret++;
	    }
	    FcStrSetDestroy (subdirs);
	    free (file);
	    continue;
	}
	if (stat ((char *) dir, &statb) == -1)
	{
	    fprintf (stderr, "\"%s\": ", dir);
	    perror ("");
	    FcStrSetDestroy (subdirs);
	    ret++;
	    free (file);
	    continue;
	}
	if (!S_ISDIR (statb.st_mode))
	{
	    fprintf (stderr, "\"%s\": not a directory, skipping\n", dir);
	    FcStrSetDestroy (subdirs);
	    free (file);
	    continue;
	}
	if (!FcDirCacheValid (dir) || !FcDirCacheRead (set, subdirs, dir))
	{
	    if (FcDebug () & FC_DBG_FONTSET)
		printf ("cache scan dir %s\n", dir);

	    FcDirScanConfig (set, subdirs, cache, 
			     config->blanks, dir, FcFalse, config);
	}
	sublist = FcStrListCreate (subdirs);
	FcStrSetDestroy (subdirs);
	if (!sublist)
	{
	    fprintf (stderr, "Can't create subdir list in \"%s\"\n", dir);
	    ret++;
	    free (file);
	    continue;
	}
	ret += FcCacheReadDirs (config, cache, sublist, set);
	free (file);
    }
    FcStrListDone (list);
    return ret;
}

FcFontSet *
FcCacheRead (FcConfig *config, FcGlobalCache * cache)
{
    FcFontSet * s = FcFontSetCreate();
    if (!s) 
	return 0;

    if (FcCacheReadDirs (config, cache, FcConfigGetConfigDirs (config), s))
	goto bail;

    return s;

 bail:
    FcFontSetDestroy (s);
    return 0;
}

static const char bin2hex[] = { '0', '1', '2', '3',
				'4', '5', '6', '7',
				'8', '9', 'a', 'b',
				'c', 'd', 'e', 'f' };

static char *
FcDirCacheHashName (char * cache_file, int collisions)
{
    unsigned char 	hash[16], hex_hash[33];
    char 		*cache_hashed;
    unsigned char	uscore = '_';
    int			cnt, i;
    FcChar8 		*tmp;
    struct MD5Context 	ctx;

    MD5Init (&ctx);
    MD5Update (&ctx, (unsigned char *)cache_file, strlen (cache_file));

    for (i = 0; i < collisions; i++)
	MD5Update (&ctx, &uscore, 1);

    MD5Final (hash, &ctx);

    for (cnt = 0; cnt < 16; ++cnt)
    {
	hex_hash[2*cnt] = bin2hex[hash[cnt] >> 4];
	hex_hash[2*cnt+1] = bin2hex[hash[cnt] & 0xf];
    }
    hex_hash[32] = 0;

    tmp = FcStrPlus ((FcChar8 *)hex_hash, (FcChar8 *)FC_CACHE_SUFFIX);
    if (!tmp)
	return 0;

    cache_hashed = (char *)FcStrPlus ((FcChar8 *)PKGCACHEDIR"/", tmp);
    free (tmp);

    return cache_hashed;
}

/* Opens the hashed name for cache_file.
 * This would fail in the unlikely event of a collision and subsequent
 * removal of the file which originally caused the collision. */
static int
FcDirCacheOpen (char *cache_file)
{
    int		fd = -1, collisions = 0;
    char	*cache_hashed;
    char	name_buf[FC_MAX_FILE_LEN];

    fd = open(cache_file, O_RDONLY);
    if (fd != -1)
	return fd;

    do
    {
	cache_hashed = FcDirCacheHashName (cache_file, collisions++);
	if (!cache_hashed)
	    return -1;

	if (fd > 0)
	    close (fd);
	fd = open(cache_hashed, O_RDONLY);
	FcStrFree ((FcChar8 *)cache_hashed);

	if (fd == -1)
	    return -1;
	FcCacheReadString (fd, name_buf, sizeof (name_buf));
	if (!strlen(name_buf))
	    goto bail;
    } while (strcmp (name_buf, cache_file) != 0);
    return fd;

 bail:
    close (fd);
    return -1;
}

/* read serialized state from the cache file */
FcBool
FcDirCacheRead (FcFontSet * set, FcStrSet * dirs, const FcChar8 *dir)
{
    char 	*cache_file;
    int 	fd;
    char 	*current_arch_machine_name;
    char 	candidate_arch_machine_name[9+MACHINE_SIGNATURE_SIZE];
    off_t 	current_arch_start = 0;
    char 	subdirName[FC_MAX_FILE_LEN + 1 + 12 + 1];

    cache_file = (char *)FcStrPlus (dir, (FcChar8 *) "/" FC_DIR_CACHE_FILE);
    if (!cache_file)
	return FcFalse;

    fd = FcDirCacheOpen (cache_file);
    if (fd < 0)
	goto bail;

    current_arch_machine_name = FcCacheMachineSignature();
    current_arch_start = FcCacheSkipToArch(fd, 
					   current_arch_machine_name);
    if (current_arch_start < 0)
        goto bail1;

    lseek (fd, current_arch_start, SEEK_SET);
    if (FcCacheReadString (fd, candidate_arch_machine_name, 
			   sizeof (candidate_arch_machine_name)) == 0)
	goto bail1;

    while (strlen(FcCacheReadString (fd, subdirName, sizeof (subdirName))) > 0)
        FcStrSetAdd (dirs, (FcChar8 *)subdirName);

    if (!FcDirCacheConsume (fd, (const char *)dir, set))
	goto bail1;
	
    close(fd);
    free (cache_file);
    return FcTrue;

 bail1:
    close (fd);
 bail:
    free (cache_file);
    return FcFalse;
}

static FcBool
FcDirCacheConsume (int fd, const char * dir, FcFontSet *set)
{
    FcCache metadata;
    void * current_dir_block;
    off_t pos;

    read(fd, &metadata, sizeof(FcCache));
    if (metadata.magic != FC_CACHE_MAGIC)
        return FcFalse;

    if (!metadata.count)
	return FcTrue;

    pos = FcCacheNextOffset (lseek(fd, 0, SEEK_CUR));
    current_dir_block = mmap (0, metadata.count, 
			      PROT_READ, MAP_SHARED, fd, pos);
    lseek (fd, pos+metadata.count, SEEK_SET);
    if (current_dir_block == MAP_FAILED)
	return FcFalse;

    FcCacheAddBankDir (metadata.bank, dir);

    if (!FcFontSetUnserialize (&metadata, set, current_dir_block))
	return FcFalse;

    return FcTrue;
}

static void *
FcDirCacheProduce (FcFontSet *set, FcCache *metadata)
{
    void * current_dir_block, * final_dir_block;
    static unsigned int rand_state = 0;
    int bank, needed_bytes_no_align;

    if (!rand_state) 
	rand_state = time(0L);
    bank = rand_r(&rand_state);

    while (FcCacheHaveBank(bank))
	bank = rand_r(&rand_state);

    memset (metadata, 0, sizeof(FcCache));
    FcFontSetNewBank();
    needed_bytes_no_align = FcFontSetNeededBytes (set);
    metadata->count = needed_bytes_no_align + 
	FcFontSetNeededBytesAlign ();
    metadata->magic = FC_CACHE_MAGIC;
    metadata->bank = bank;

    if (!needed_bytes_no_align) /* not a failure, no fonts to write */
    {
	/* no, you don't really need to write any bytes at all. */
	metadata->count = 0;
	return 0;
    }

    current_dir_block = malloc (metadata->count);
    if (!current_dir_block)
	goto bail;
    // shut up valgrind
    memset (current_dir_block, 0, metadata->count);
    final_dir_block = FcFontSetDistributeBytes (metadata, current_dir_block);

    if ((void *)((char *)current_dir_block+metadata->count) < final_dir_block)
	goto bail;
			      
    if (!FcFontSetSerialize (bank, set))
	goto bail;

    return current_dir_block;

 bail:
    free (current_dir_block);
    return 0;
}

/* write serialized state to the cache file */
FcBool
FcDirCacheWrite (FcFontSet *set, FcStrSet *dirs, const FcChar8 *dir)
{
    char	    *cache_file;
    char	    *cache_hashed;
    int 	    fd, fd_orig, i, dirs_count;
    FcAtomic 	    *atomic;
    FcCache 	    metadata;
    off_t 	    current_arch_start = 0, truncate_to;
    char	    name_buf[FC_MAX_FILE_LEN];
    int		    collisions;

    char            *current_arch_machine_name, * header;
    void 	    *current_dir_block = 0;

    cache_file = (char *)FcStrPlus (dir, (FcChar8 *) "/" FC_DIR_CACHE_FILE);
    if (!cache_file)
        goto bail;

    /* Ensure that we're not trampling a cache for some other dir. */
    /* This is slightly different from FcDirCacheOpen, since it 
     * needs the filename, not the file descriptor. */
    fd = -1; collisions = 0;
    do
    {
	cache_hashed = FcDirCacheHashName (cache_file, collisions++);
	if (!cache_hashed)
	    goto bail0;

	if (fd > 0)
	    close (fd);
	fd = open(cache_hashed, O_RDONLY);
	if (fd == -1)
	    break;
	FcCacheReadString (fd, name_buf, sizeof (name_buf));
	close (fd);

	if (!strlen(name_buf))
	    break;

	if (strcmp (name_buf, cache_file) != 0)
	    continue;
    } while (0);

    current_dir_block = FcDirCacheProduce (set, &metadata);

    if (metadata.count && !current_dir_block)
	goto bail1;

    if (FcDebug () & FC_DBG_CACHE)
        printf ("FcDirCacheWriteDir cache_file \"%s\"\n", cache_file);

    atomic = FcAtomicCreate ((FcChar8 *)cache_hashed);
    if (!atomic)
	goto bail1;

    if (!FcAtomicLock (atomic))
    {
	/* Now try rewriting the original version of the file. */
	FcAtomicDestroy (atomic);

	atomic = FcAtomicCreate ((FcChar8 *)cache_file);
	fd_orig = open (cache_file, O_RDONLY);
	if (fd_orig == -1)
	    fd_orig = open((char *)FcAtomicOrigFile (atomic), O_RDONLY);

	fd = open((char *)FcAtomicNewFile (atomic), O_RDWR | O_CREAT, 0666);
	if (fd == -1)
	    goto bail2;
    }

    /* In all cases, try opening the real location of the cache file first. */
    /* (even if that's not atomic.) */
    fd_orig = open (cache_file, O_RDONLY);
    if (fd_orig == -1)
	fd_orig = open((char *)FcAtomicOrigFile (atomic), O_RDONLY);

    fd = open((char *)FcAtomicNewFile (atomic), O_RDWR | O_CREAT, 0666);
    if (fd == -1)
	goto bail3;

    FcCacheWriteString (fd, cache_file);

    current_arch_machine_name = FcCacheMachineSignature ();
    current_arch_start = 0;

    if (fd_orig != -1)
        current_arch_start = 
            FcCacheSkipToArch(fd_orig, current_arch_machine_name);

    if (current_arch_start < 0)
	current_arch_start = FcCacheNextOffset (lseek(fd_orig, 0, SEEK_END));

    if (fd_orig != -1 && !FcCacheCopyOld(fd, fd_orig, current_arch_start))
	goto bail4;

    if (fd_orig != -1)
        close (fd_orig);

    current_arch_start = lseek(fd, 0, SEEK_CUR);
    if (ftruncate (fd, current_arch_start) == -1)
	goto bail4;

    /* allocate space for subdir names in this block */
    dirs_count = 0;
    for (i = 0; i < dirs->size; i++)
        dirs_count += strlen((char *)dirs->strs[i]) + 1;
    dirs_count ++;

    /* now write the address of the next offset */
    truncate_to = FcCacheNextOffset (FcCacheNextOffset (current_arch_start + sizeof (FcCache) + dirs_count) + metadata.count) - current_arch_start;
    header = malloc (10 + strlen (current_arch_machine_name));
    if (!header)
	goto bail4;
    sprintf (header, "%8x ", (int)truncate_to);
    strcat (header, current_arch_machine_name);
    if (!FcCacheWriteString (fd, header))
	goto bail5;

    for (i = 0; i < dirs->size; i++)
        FcCacheWriteString (fd, (char *)dirs->strs[i]);
    FcCacheWriteString (fd, "");

    write (fd, &metadata, sizeof(FcCache));
    if (metadata.count)
    {
	lseek (fd, FcCacheNextOffset (lseek(fd, 0, SEEK_END)), SEEK_SET);
	write (fd, current_dir_block, metadata.count);
	free (current_dir_block);
    }

    /* this actually serves to pad out the cache file, if needed */
    if (ftruncate (fd, current_arch_start + truncate_to) == -1)
	goto bail5;

    close(fd);
    if (!FcAtomicReplaceOrig(atomic))
        goto bail5;
    FcStrFree ((FcChar8 *)cache_hashed);
    FcAtomicUnlock (atomic);
    FcAtomicDestroy (atomic);
    return FcTrue;

 bail5:
    free (header);
 bail4:
    close (fd);
 bail3:
    FcAtomicUnlock (atomic);
 bail2:
    FcAtomicDestroy (atomic);
 bail1:
    FcStrFree ((FcChar8 *)cache_hashed);
 bail0:
    unlink ((char *)cache_file);
    free (cache_file);
    if (current_dir_block)
        free (current_dir_block);
 bail:
    return FcFalse;
}

static char *
FcCacheMachineSignature ()
{
    static char buf[MACHINE_SIGNATURE_SIZE];
    int32_t magic = ENDIAN_TEST;
    char * m = (char *)&magic;

    sprintf (buf, "%2x%2x%2x%2x "
	     "%4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x "
	     "%4x %4x %4x %4x %4x %4x %4x %4x\n", 
	     m[0], m[1], m[2], m[3],
	     (unsigned int)sizeof (char),
	     (unsigned int)sizeof (char *),
	     (unsigned int)sizeof (int),
	     (unsigned int)sizeof (FcPattern),
	     (unsigned int)sizeof (FcPatternEltPtr),
	     (unsigned int)sizeof (struct _FcPatternElt *),
	     (unsigned int)sizeof (FcPatternElt),
	     (unsigned int)sizeof (FcObjectPtr),
	     (unsigned int)sizeof (FcValueListPtr),
	     (unsigned int)sizeof (FcValue),
	     (unsigned int)sizeof (FcValueBinding),
	     (unsigned int)sizeof (struct _FcValueList *),
	     (unsigned int)sizeof (FcCharSet),
	     (unsigned int)sizeof (FcCharLeaf **),
	     (unsigned int)sizeof (FcChar16 *),
	     (unsigned int)sizeof (FcChar16),
	     (unsigned int)sizeof (FcCharLeaf),
	     (unsigned int)sizeof (FcChar32),
	     (unsigned int)sizeof (FcCache),
	     (unsigned int)sysconf(_SC_PAGESIZE));

    return buf;
}

static int banks_ptr = 0, banks_alloc = 0;
int * _fcBankId = 0, * _fcBankIdx = 0;
static const char ** bankDirs = 0;

static FcBool
FcCacheHaveBank (int bank)
{
    int i;

    if (bank < FC_BANK_FIRST)
	return FcTrue;

    for (i = 0; i < banks_ptr; i++)
	if (_fcBankId[i] == bank)
	    return FcTrue;

    return FcFalse;
}

int
FcCacheBankToIndexMTF (int bank)
{
    int i, j;

    for (i = 0; i < banks_ptr; i++)
	if (_fcBankId[_fcBankIdx[i]] == bank)
	{
	    int t = _fcBankIdx[i];

	    for (j = i; j > 0; j--)
		_fcBankIdx[j] = _fcBankIdx[j-1];
	    _fcBankIdx[0] = t;
	    return t;
	}

    if (banks_ptr >= banks_alloc)
    {
	int * b, * bidx;
	const char ** bds;

	b = realloc (_fcBankId, (banks_alloc + 4) * sizeof(int));
	if (!b)
	    return -1;
	_fcBankId = b;

	bidx = realloc (_fcBankIdx, (banks_alloc + 4) * sizeof(int));
	if (!bidx)
	    return -1;
	_fcBankIdx = bidx;

	bds = realloc (bankDirs, (banks_alloc + 4) * sizeof (char *));
	if (!bds)
	    return -1;
	bankDirs = bds;

	banks_alloc += 4;
    }

    i = banks_ptr++;
    _fcBankId[i] = bank;
    _fcBankIdx[i] = i;
    return i;
}

static void
FcCacheAddBankDir (int bank, const char * dir)
{
    int bi = FcCacheBankToIndexMTF (bank);

    if (bi < 0)
	return;

    bankDirs[bi] = (const char *)FcStrCopy ((FcChar8 *)dir);
}

const char *
FcCacheFindBankDir (int bank)
{
    int bi = FcCacheBankToIndex (bank);
    return bankDirs[bi];
}

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.	This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

#ifndef HIGHFIRST
#define byteReverse(buf, len)	/* Nothing */
#else
/*
 * Note: this code is harmless on little-endian machines.
 */
void byteReverse(unsigned char *buf, unsigned longs)
{
    FcChar32 t;
    do {
	t = (FcChar32) ((unsigned) buf[3] << 8 | buf[2]) << 16 |
	    ((unsigned) buf[1] << 8 | buf[0]);
	*(FcChar32 *) buf = t;
	buf += 4;
    } while (--longs);
}
#endif

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
static void MD5Init(struct MD5Context *ctx)
{
    ctx->buf[0] = 0x67452301;
    ctx->buf[1] = 0xefcdab89;
    ctx->buf[2] = 0x98badcfe;
    ctx->buf[3] = 0x10325476;

    ctx->bits[0] = 0;
    ctx->bits[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void MD5Update(struct MD5Context *ctx, unsigned char *buf, unsigned len)
{
    FcChar32 t;

    /* Update bitcount */

    t = ctx->bits[0];
    if ((ctx->bits[0] = t + ((FcChar32) len << 3)) < t)
	ctx->bits[1]++; 	/* Carry from low to high */
    ctx->bits[1] += len >> 29;

    t = (t >> 3) & 0x3f;	/* Bytes already in shsInfo->data */

    /* Handle any leading odd-sized chunks */

    if (t) {
	unsigned char *p = (unsigned char *) ctx->in + t;

	t = 64 - t;
	if (len < t) {
	    memcpy(p, buf, len);
	    return;
	}
	memcpy(p, buf, t);
	byteReverse(ctx->in, 16);
	MD5Transform(ctx->buf, (FcChar32 *) ctx->in);
	buf += t;
	len -= t;
    }
    /* Process data in 64-byte chunks */

    while (len >= 64) {
	memcpy(ctx->in, buf, 64);
	byteReverse(ctx->in, 16);
	MD5Transform(ctx->buf, (FcChar32 *) ctx->in);
	buf += 64;
	len -= 64;
    }

    /* Handle any remaining bytes of data. */

    memcpy(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
static void MD5Final(unsigned char digest[16], struct MD5Context *ctx)
{
    unsigned count;
    unsigned char *p;

    /* Compute number of bytes mod 64 */
    count = (ctx->bits[0] >> 3) & 0x3F;

    /* Set the first char of padding to 0x80.  This is safe since there is
       always at least one byte free */
    p = ctx->in + count;
    *p++ = 0x80;

    /* Bytes of padding needed to make 64 bytes */
    count = 64 - 1 - count;

    /* Pad out to 56 mod 64 */
    if (count < 8) {
	/* Two lots of padding:  Pad the first block to 64 bytes */
	memset(p, 0, count);
	byteReverse(ctx->in, 16);
	MD5Transform(ctx->buf, (FcChar32 *) ctx->in);

	/* Now fill the next block with 56 bytes */
	memset(ctx->in, 0, 56);
    } else {
	/* Pad block to 56 bytes */
	memset(p, 0, count - 8);
    }
    byteReverse(ctx->in, 14);

    /* Append length in bits and transform */
    ((FcChar32 *) ctx->in)[14] = ctx->bits[0];
    ((FcChar32 *) ctx->in)[15] = ctx->bits[1];

    MD5Transform(ctx->buf, (FcChar32 *) ctx->in);
    byteReverse((unsigned char *) ctx->buf, 4);
    memcpy(digest, ctx->buf, 16);
    memset(ctx, 0, sizeof(ctx));        /* In case it's sensitive */
}


/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f, w, x, y, z, data, s) \
	( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void MD5Transform(FcChar32 buf[4], FcChar32 in[16])
{
    register FcChar32 a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
    MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}
