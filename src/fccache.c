/*
 * $RCSId: xc/lib/fontconfig/src/fccache.c,v 1.12 2002/08/22 07:36:44 keithp Exp $
 *
 * Copyright © 2000 Keith Packard
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
#include <sys/mman.h>
#include <sys/utsname.h>
#include "fcint.h"

#define PAGESIZE 8192

static FcBool force;

static FcChar8 *
FcCacheReadString (int fd, FcChar8 *dest, int len)
{
    FcChar8	c;
    FcBool	escape;
    int		size;
    int		i;

    if (len == 0)
	return FcFalse;
    
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

static FcBool
FcCacheWriteString (int fd, const FcChar8 *chars)
{
    if (write (fd, chars, strlen(chars)+1) != strlen(chars)+1)
	return FcFalse;
    return FcTrue;
}

#if 0
/*
 * Verify the saved timestamp for a file
 */
FcBool
FcGlobalCacheCheckTime (const FcChar8 *file, FcGlobalCacheInfo *info)
{
    struct stat	    statb;

    if (stat ((char *) file, &statb) < 0)
    {
	if (FcDebug () & FC_DBG_CACHE)
	    printf (" file %s missing\n", file);
	return FcFalse;
    }
    if (statb.st_mtime != info->time)
    {
	if (FcDebug () & FC_DBG_CACHE)
	    printf (" timestamp mismatch (was %d is %d)\n",
		    (int) info->time, (int) statb.st_mtime);
	return FcFalse;
    }
    return FcTrue;
}

void
FcGlobalCacheReferenced (FcGlobalCache	    *cache,
			 FcGlobalCacheInfo  *info)
{
    if (!info->referenced)
    {
	info->referenced = FcTrue;
	cache->referenced++;
	if (FcDebug () & FC_DBG_CACHE_REF)
	    printf ("Reference %d %s\n", cache->referenced, info->file);
    }
}

/*
 * Break a path into dir/base elements and compute the base hash
 * and the dir length.  This is shared between the functions
 * which walk the file caches
 */

typedef struct _FcFilePathInfo {
    const FcChar8   *dir;
    int		    dir_len;
    const FcChar8   *base;
    unsigned int    base_hash;
} FcFilePathInfo;

static FcFilePathInfo
FcFilePathInfoGet (const FcChar8    *path)
{
    FcFilePathInfo  i;
    FcChar8	    *slash;

    slash = FcStrLastSlash (path);
    if (slash)
    {
        i.dir = path;
        i.dir_len = slash - path;
	if (!i.dir_len)
	    i.dir_len = 1;
	i.base = slash + 1;
    }
    else
    {
	i.dir = (const FcChar8 *) ".";
	i.dir_len = 1;
	i.base = path;
    }
    i.base_hash = FcCacheHash (i.base, -1);
    return i;
}

FcGlobalCache *
FcGlobalCacheCreate (void)
{
    FcGlobalCache   *cache;
    int		    h;

    cache = malloc (sizeof (FcGlobalCache));
    if (!cache)
	return 0;
    FcMemAlloc (FC_MEM_CACHE, sizeof (FcGlobalCache));
    for (h = 0; h < FC_GLOBAL_CACHE_DIR_HASH_SIZE; h++)
	cache->ents[h] = 0;
    cache->entries = 0;
    cache->referenced = 0;
    cache->updated = FcFalse;
    cache->broken = FcFalse;
    return cache;
}

void
FcGlobalCacheDestroy (FcGlobalCache *cache)
{
    FcGlobalCacheDir	*d, *next;
    int			h;

    for (h = 0; h < FC_GLOBAL_CACHE_DIR_HASH_SIZE; h++)
    {
	for (d = cache->ents[h]; d; d = next)
	{
	    next = d->next;
	    FcGlobalCacheDirDestroy (d);
	}
    }
    FcMemFree (FC_MEM_CACHE, sizeof (FcGlobalCache));
    free (cache);
}

/*
 * Cache file syntax is quite simple:
 *
 * "file_name" id time "font_name" \n
 */
 
void
FcGlobalCacheLoad (FcGlobalCache    *cache,
		   const FcChar8    *cache_file)
{
    FILE		*f;
    FcChar8		file_buf[8192], *file;
    int			id;
    time_t		time;
    FcChar8		name_buf[8192], *name;
    FcGlobalCacheInfo	*info;

    f = fopen ((char *) cache_file, "r");
    if (!f)
	return;

    cache->updated = FcFalse;
    file = 0;
    name = 0;
    while ((file = FcCacheReadString (f, file_buf, sizeof (file_buf))) &&
	   FcCacheReadInt (f, &id) &&
	   FcCacheReadTime (f, &time) &&
	   (name = FcCacheReadString (f, name_buf, sizeof (name_buf))))
    {
	if (FcDebug () & FC_DBG_CACHEV)
	    printf ("FcGlobalCacheLoad \"%s\" \"%20.20s\"\n", file, name);
	if (!FcStrCmp (name, FC_FONT_FILE_DIR))
	    info = FcGlobalCacheDirAdd (cache, file, time, FcFalse, FcTrue);
	else
	    info = FcGlobalCacheFileAdd (cache, file, id, time, name, FcFalse);
	if (!info)
	    cache->broken = FcTrue;
	else
	    cache->entries++;
	if (FcDebug () & FC_DBG_CACHE_REF)
	    printf ("FcGlobalCacheLoad entry %d %s\n",
		    cache->entries, file);
	if (file != file_buf)
	    free (file);
	if (name != name_buf)
	    free (name);
	file = 0;
	name = 0;
    }
    if (file && file != file_buf)
	free (file);
    if (name && name != name_buf)
	free (name);
    fclose (f);
}

FcBool
FcGlobalCacheUpdate (FcGlobalCache  *cache,
		     const FcChar8  *file,
		     int	    id,
		     const FcChar8  *name)
{
    const FcChar8	*match;
    struct stat		statb;
    FcGlobalCacheInfo	*info;

    match = file;

    if (stat ((char *) file, &statb) < 0)
	return FcFalse;
    if (S_ISDIR (statb.st_mode))
	info = FcGlobalCacheDirAdd (cache, file, statb.st_mtime, 
				    FcTrue, FcTrue);
    else
	info = FcGlobalCacheFileAdd (cache, file, id, statb.st_mtime, 
				    name, FcTrue);
    if (info)
    {
	FcGlobalCacheReferenced (cache, info);
	cache->updated = FcTrue;
    }
    else
	cache->broken = FcTrue;
    return info != 0;
}

FcBool
FcGlobalCacheSave (FcGlobalCache    *cache,
		   const FcChar8    *cache_file)
{
    FILE		*f;
    int			dir_hash, file_hash;
    FcGlobalCacheDir	*dir;
    FcGlobalCacheFile	*file;
    FcAtomic		*atomic;

    if (!cache->updated && cache->referenced == cache->entries)
	return FcTrue;
    
    if (cache->broken)
	return FcFalse;

#if defined (HAVE_GETUID) && defined (HAVE_GETEUID)
    /* Set-UID programs can't safely update the cache */
    if (getuid () != geteuid ())
	return FcFalse;
#endif
    
    atomic = FcAtomicCreate (cache_file);
    if (!atomic)
	goto bail0;
    if (!FcAtomicLock (atomic))
	goto bail1;
    f = fopen ((char *) FcAtomicNewFile(atomic), "w");
    if (!f)
	goto bail2;

    for (dir_hash = 0; dir_hash < FC_GLOBAL_CACHE_DIR_HASH_SIZE; dir_hash++)
    {
	for (dir = cache->ents[dir_hash]; dir; dir = dir->next)
	{
	    if (!dir->info.referenced)
		continue;
	    if (!FcCacheWriteString (f, dir->info.file))
		goto bail4;
	    if (PUTC (' ', f) == EOF)
		goto bail4;
	    if (!FcCacheWriteInt (f, 0))
		goto bail4;
	    if (PUTC (' ', f) == EOF)
		goto bail4;
	    if (!FcCacheWriteTime (f, dir->info.time))
		goto bail4;
	    if (PUTC (' ', f) == EOF)
		goto bail4;
	    if (!FcCacheWriteString (f, (FcChar8 *) FC_FONT_FILE_DIR))
		goto bail4;
	    if (PUTC ('\n', f) == EOF)
		goto bail4;
	    
	    for (file_hash = 0; file_hash < FC_GLOBAL_CACHE_FILE_HASH_SIZE; file_hash++)
	    {
		for (file = dir->ents[file_hash]; file; file = file->next)
		{
		    if (!file->info.referenced)
			continue;
		    if (!FcCacheWritePath (f, dir->info.file, file->info.file))
			goto bail4;
		    if (PUTC (' ', f) == EOF)
			goto bail4;
		    if (!FcCacheWriteInt (f, file->id < 0 ? 0 : file->id))
			goto bail4;
		    if (PUTC (' ', f) == EOF)
			goto bail4;
		    if (!FcCacheWriteTime (f, file->info.time))
			goto bail4;
		    if (PUTC (' ', f) == EOF)
			goto bail4;
		    if (!FcCacheWriteString (f, file->name))
			goto bail4;
		    if (PUTC ('\n', f) == EOF)
			goto bail4;
		}
	    }
	}
    }

    if (fclose (f) == EOF)
	goto bail3;
    
    if (!FcAtomicReplaceOrig (atomic))
	goto bail3;
    
    FcAtomicUnlock (atomic);
    FcAtomicDestroy (atomic);

    cache->updated = FcFalse;
    return FcTrue;

bail4:
    fclose (f);
bail3:
    FcAtomicDeleteNew (atomic);
bail2:
    FcAtomicUnlock (atomic);
bail1:
    FcAtomicDestroy (atomic);
bail0:
    return FcFalse;
}
#endif

/* 
 * Find the next presumably-mmapable offset after the current file
 * pointer.
 */
int
FcCacheNextOffset(int fd)
{
    off_t w;
    w = lseek(fd, 0, SEEK_END);

    if (w % PAGESIZE == 0) 
	return w;
    else
	return ((w / PAGESIZE)+1)*PAGESIZE;
}

/* will go away once we use config->cache */
#define CACHE_DEFAULT_TMPDIR "/tmp"
#define CACHE_DEFAULT_NAME "/fontconfig-mmap"
static char *
FcCacheFilename(void)
{
    struct utsname b;
    static char * name = 0;

    if (name)
        return name;

    if (uname(&b) == -1)
        name = CACHE_DEFAULT_NAME;
    else
    {
        char * tmpname = getenv("TMPDIR");
        char * logname = getenv("LOGNAME");
        if (!tmpname)
            tmpname = CACHE_DEFAULT_TMPDIR;

        name = malloc(strlen(CACHE_DEFAULT_NAME) +
                      strlen(tmpname) +
                      (logname ? strlen(logname) : 0) + 5);
        strcpy(name, tmpname);
        strcat(name, CACHE_DEFAULT_NAME);
        strcat(name, "-");
        strcat(name, logname ? logname : "");
    }
    return name;
}

/* 
 * Wipe out static state.
 */
void
FcCacheClearStatic()
{
    FcFontSetClearStatic();
    FcPatternClearStatic();
    FcValueListClearStatic();
    FcObjectClearStatic();
    FcMatrixClearStatic();
    FcCharSetClearStatic();
    FcLangSetClearStatic();
}

/*
 * Trigger the counting phase: this tells us how much to allocate.
 */
FcBool
FcCachePrepareSerialize (FcConfig * config)
{
    int i;
    for (i = FcSetSystem; i <= FcSetApplication; i++)
	if (config->fonts[i] && !FcFontSetPrepareSerialize(config->fonts[i]))
	    return FcFalse;
    return FcTrue;
}

/* allocate and populate static structures */
FcBool
FcCacheSerialize (FcConfig * config)
{
    int i;
    for (i = FcSetSystem; i <= FcSetApplication; i++)
	if (config->fonts[i] && !FcFontSetSerialize(config->fonts[i]))
	    return FcFalse;
    return FcTrue;
}

/* get the current arch name */
/* caller is responsible for freeing returned pointer */
static char *
FcCacheGetCurrentArch (void)
{
    struct utsname b;
    char * current_arch_machine_name;

    if (uname(&b) == -1)
	return FcFalse;
    current_arch_machine_name = strdup(b.machine);
    /* if (getenv ("FAKE_ARCH")) // testing purposes
       current_arch_machine_name = strdup(getenv("FAKE_ARCH")); */
    return current_arch_machine_name;
}

/* return the address of the segment for the provided arch,
 * or -1 if arch not found */
static off_t
FcCacheSkipToArch (int fd, const char * arch)
{
    char candidate_arch_machine_name[64], bytes_to_skip[7];
    off_t current_arch_start = 0;

    /* skip arches that are not the current arch */
    while (1)
    {
	long bs;

	lseek (fd, current_arch_start, SEEK_SET);
	if (FcCacheReadString (fd, candidate_arch_machine_name, 
			       sizeof (candidate_arch_machine_name)) == 0)
	    break;
	if (FcCacheReadString (fd, bytes_to_skip, 7) == 0)
	    break;
	bs = a64l(bytes_to_skip);
	if (bs == 0)
	    break;

	if (strcmp (candidate_arch_machine_name, arch)==0)
	    break;
	current_arch_start += bs;
    }

    if (strcmp (candidate_arch_machine_name, arch)!=0)
	return -1;

    return current_arch_start;
}

/* Cuts out the segment at the file pointer (moves everything else
 * down to cover it), and leaves the file pointer at the end of the
 * file. */
#define BUF_SIZE 8192

static FcBool 
FcCacheMoveDown (int fd, off_t start)
{
    char * buf = malloc (BUF_SIZE);
    char candidate_arch_machine_name[64], bytes_to_skip[7];
    long bs; off_t pos;
    int c, bytes_skipped;

    if (!buf)
	return FcFalse;

    lseek (fd, start, SEEK_SET);
    if (FcCacheReadString (fd, candidate_arch_machine_name, 
			   sizeof (candidate_arch_machine_name)) == 0)
	goto done;
    if (FcCacheReadString (fd, bytes_to_skip, 7) == 0)
	goto done;

    bs = a64l(bytes_to_skip);
    if (bs == 0)
	goto done;

    bytes_skipped = 0;
    do
    {
	lseek (fd, start+bs+bytes_skipped, SEEK_SET);
	if ((c = read (fd, buf, BUF_SIZE)) <= 0)
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

/* read serialized state from the cache file */
FcBool
FcCacheRead (FcConfig *config)
{
    int fd, i;
    FcCache metadata;
    char * current_arch_machine_name;
    char candidate_arch_machine_name[64], bytes_in_block[7];
    off_t current_arch_start = 0;

    if (force)
	return FcFalse;

    fd = open(FcCacheFilename(), O_RDONLY);
    if (fd == -1)
        return FcFalse;

    current_arch_machine_name = FcCacheGetCurrentArch();
    current_arch_start = FcCacheSkipToArch(fd, current_arch_machine_name);
    if (current_arch_start < 0)
        goto bail;

    lseek (fd, current_arch_start, SEEK_SET);
    if (FcCacheReadString (fd, candidate_arch_machine_name, 
			   sizeof (candidate_arch_machine_name)) == 0)
	goto bail;
    if (FcCacheReadString (fd, bytes_in_block, 7) == 0)
	goto bail;

    // sanity check for endianness issues
    read(fd, &metadata, sizeof(FcCache));
    if (metadata.magic != FC_CACHE_MAGIC)
        goto bail;

    if (!FcObjectRead(fd, metadata)) goto bail1;
    if (!FcStrSetRead(fd, metadata)) goto bail1;
    if (!FcCharSetRead(fd, metadata)) goto bail1;
    if (!FcMatrixRead(fd, metadata)) goto bail1;
    if (!FcLangSetRead(fd, metadata)) goto bail1;
    if (!FcValueListRead(fd, metadata)) goto bail1;
    if (!FcPatternEltRead(fd, metadata)) goto bail1;
    if (!FcPatternRead(fd, metadata)) goto bail1;
    if (!FcFontSetRead(fd, config, metadata)) goto bail1;
    close(fd);
    free (current_arch_machine_name);
    return FcTrue;

 bail1:
    for (i = FcSetSystem; i <= FcSetApplication; i++)
        config->fonts[i] = 0;
    close(fd);
 bail:
    free (current_arch_machine_name);
    return FcFalse;
}

/* write serialized state to the cache file */
FcBool
FcCacheWrite (FcConfig * config)
{
    int fd;
    FcCache metadata;
    off_t current_arch_start = 0, truncate_to;
    char * current_arch_machine_name, bytes_written[7] = "dedbef";

    if (!FcCachePrepareSerialize (config))
	return FcFalse;

    if (!FcCacheSerialize (config))
	return FcFalse;

    fd = open(FcCacheFilename(), O_RDWR | O_CREAT, 0666);
    if (fd == -1)
        return FcFalse;

    current_arch_machine_name = FcCacheGetCurrentArch();
    current_arch_start = FcCacheSkipToArch(fd, current_arch_machine_name);
    if (current_arch_start < 0)
	current_arch_start = FcCacheNextOffset (fd);

    if (!FcCacheMoveDown(fd, current_arch_start))
	goto bail;

    current_arch_start = lseek(fd, 0, SEEK_CUR);
    if (ftruncate (fd, current_arch_start) == -1)
	goto bail;

    /* reserve space for arch, count & metadata */
    if (!FcCacheWriteString (fd, current_arch_machine_name))
	goto bail;
    if (!FcCacheWriteString (fd, bytes_written))
	goto bail;
    memset (&metadata, 0, sizeof(FcCache));
    metadata.magic = FC_CACHE_MAGIC;
    write(fd, &metadata, sizeof(FcCache));

    if (!FcFontSetWrite(fd, config, &metadata)) goto bail;
    if (!FcPatternWrite(fd, &metadata)) goto bail;
    if (!FcPatternEltWrite(fd, &metadata)) goto bail;
    if (!FcValueListWrite(fd, &metadata)) goto bail;
    if (!FcLangSetWrite(fd, &metadata)) goto bail;
    if (!FcCharSetWrite(fd, &metadata)) goto bail;
    if (!FcMatrixWrite(fd, &metadata)) goto bail;
    if (!FcStrSetWrite(fd, &metadata)) goto bail;
    if (!FcObjectWrite(fd, &metadata)) goto bail;

    /* now write the address of the next offset */
    truncate_to = FcCacheNextOffset(fd) - current_arch_start;
    lseek(fd, current_arch_start + strlen(current_arch_machine_name)+1, 
	  SEEK_SET);
    strcpy (bytes_written, l64a(truncate_to));
    if (!FcCacheWriteString (fd, bytes_written))
	goto bail;

    /* now rewrite metadata & truncate file */
    if (write(fd, &metadata, sizeof(FcCache)) != sizeof (FcCache)) 
	goto bail;
    if (ftruncate (fd, current_arch_start + truncate_to) == -1)
	goto bail;

    close(fd);
    return FcTrue;

 bail:
    free (current_arch_machine_name);
    return FcFalse;
}

/* if true, ignore the cache file */
void
FcCacheForce (FcBool f)
{
    force = f;
}
