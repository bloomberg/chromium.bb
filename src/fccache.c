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
#include <dirent.h>
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
static int
FcCacheNextOffset(off_t w)
{
    if (w % PAGESIZE == 0) 
	return w;
    else
	return ((w / PAGESIZE)+1)*PAGESIZE;
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
    long bs;
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

static int
FcCacheReadDirs (FcStrList *list, FcFontSet * set)
{
    DIR			*d;
    struct dirent	*e;
    int			ret = 0;
    FcChar8		*dir;
    FcChar8		*file, *base;
    FcStrSet		*subdirs;
    FcStrList		*sublist;
    struct stat		statb;

    /*
     * Now scan all of the directories into separate databases
     * and write out the results
     */
    while ((dir = FcStrListNext (list)))
    {
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
	d = opendir ((char *) dir);
	if (!d)
	{
	    FcStrSetDestroy (subdirs);
	    free (file);
	    continue;
	}
	while ((e = readdir (d)))
	{
	    if (e->d_name[0] != '.' && strlen (e->d_name) < FC_MAX_FILE_LEN)
	    {
		strcpy ((char *) base, (char *) e->d_name);
		if (FcFileIsDir (file) && !FcStrSetAdd (subdirs, file))
		    ret++;
	    }
	}
	closedir (d);
	if (1 || FcDirCacheValid (dir))
	{
	    FcDirCacheRead (set, dir);
	}
	else
	{
	    ret++;
#if 0 // (implement per-dir loading)
	    if (verbose)
		printf ("caching, %d fonts, %d dirs\n", 
			set->nfont, nsubdirs (subdirs));

	    if (!FcDirSave (set, dir))
	    {
		fprintf (stderr, "Can't save cache in \"%s\"\n", dir);
		ret++;
	    }
#endif
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
	ret += FcCacheReadDirs (sublist, set);
	free (file);
    }
    FcStrListDone (list);
    return ret;
}

FcFontSet *
FcCacheRead (FcConfig *config)
{
    FcFontSet * s = FcFontSetCreate();
    if (!s) 
	return 0;

    if (force)
	goto bail;

    if (FcCacheReadDirs (FcConfigGetConfigDirs (config), s))
	goto bail;

    return s;

 bail:
    FcFontSetDestroy (s);
    return 0;
}

/* read serialized state from the cache file */
FcBool
FcDirCacheRead (FcFontSet * set, const FcChar8 *dir)
{
    FcChar8         *cache_file = FcStrPlus (dir, (FcChar8 *) "/" FC_DIR_CACHE_FILE);
    int fd;
    FcCache metadata;
    void * current_dir_block;
    char * current_arch_machine_name;
    char candidate_arch_machine_name[64], bytes_in_block[7];
    off_t current_arch_start = 0;

    if (force)
	goto bail;
    if (!cache_file)
        goto bail;

    current_arch_machine_name = FcCacheGetCurrentArch();
    fd = open(cache_file, O_RDONLY);
    if (fd == -1)
        goto bail0;

    current_arch_start = FcCacheSkipToArch(fd, current_arch_machine_name);
    if (current_arch_start < 0)
        goto bail1;

    lseek (fd, current_arch_start, SEEK_SET);
    if (FcCacheReadString (fd, candidate_arch_machine_name, 
			   sizeof (candidate_arch_machine_name)) == 0)
	goto bail1;
    if (FcCacheReadString (fd, bytes_in_block, 7) == 0)
	goto bail1;

    // sanity check for endianness issues
    read(fd, &metadata, sizeof(FcCache));
    if (metadata.magic != FC_CACHE_MAGIC)
        goto bail1;

    if (metadata.count)
    {
	off_t pos = FcCacheNextOffset (lseek(fd, 0, SEEK_CUR));
	current_dir_block = mmap (0, metadata.count, 
				  PROT_READ, MAP_SHARED, fd, pos);
	if (current_dir_block == MAP_FAILED)
	    perror("");

	if (!FcFontSetUnserialize (metadata, set, current_dir_block))
	    goto bail1;
    }
	
    close(fd);
    free (current_arch_machine_name);
    free (cache_file);
    return FcTrue;

 bail1:
    close(fd);
 bail0:
    free (current_arch_machine_name);
 bail:
    free (cache_file);
    return FcFalse;
}

/* write serialized state to the cache file */
FcBool
FcDirCacheWrite (int bank, FcFontSet *set, const FcChar8 *dir)
{
    FcChar8         *cache_file = FcStrPlus (dir, (FcChar8 *) "/" FC_DIR_CACHE_FILE);
    int fd, bytes_to_write, metadata_bytes;
    FcCache metadata;
    off_t current_arch_start = 0, truncate_to;
    char * current_arch_machine_name, bytes_written[7] = "dedbef";
    void * current_dir_block, *final_dir_block;

    if (!cache_file)
        goto bail;

    FcFontSetNewBank();
    bytes_to_write = FcFontSetNeededBytes (set);
    metadata_bytes = FcCacheNextOffset (sizeof (FcCache));

    if (!bytes_to_write)
    {
	unlink (cache_file);
	free (cache_file);
	return FcTrue;
    }

    current_dir_block = malloc (bytes_to_write);
    memset (&metadata, 0, sizeof(FcCache));
    metadata.count = bytes_to_write;
    metadata.bank = bank;
    if (!current_dir_block)
	goto bail;
    final_dir_block = FcFontSetDistributeBytes (&metadata, current_dir_block);
			      
    if (!FcFontSetSerialize (bank, set))
	return FcFalse;

    if (FcDebug () & FC_DBG_CACHE)
        printf ("FcDirCacheWriteDir cache_file \"%s\"\n", cache_file);

    fd = open(cache_file, O_RDWR | O_CREAT, 0666);
    if (fd == -1)
        return FcFalse;

    current_arch_machine_name = FcCacheGetCurrentArch();
    current_arch_start = FcCacheSkipToArch(fd, current_arch_machine_name);
    if (current_arch_start < 0)
	current_arch_start = FcCacheNextOffset (lseek(fd, 0, SEEK_END));

    if (!FcCacheMoveDown(fd, current_arch_start))
	goto bail1;

    current_arch_start = lseek(fd, 0, SEEK_CUR);
    if (ftruncate (fd, current_arch_start) == -1)
	goto bail1;

    /* reserve space for arch, count & metadata */
    if (!FcCacheWriteString (fd, current_arch_machine_name))
	goto bail1;

    /* now write the address of the next offset */
    truncate_to = FcCacheNextOffset(current_arch_start + bytes_to_write + metadata_bytes) -
	current_arch_start;
    strcpy (bytes_written, l64a(truncate_to));
    if (!FcCacheWriteString (fd, bytes_written))
	goto bail1;

    metadata.magic = FC_CACHE_MAGIC;
    write (fd, &metadata, sizeof(FcCache));
    lseek (fd, FcCacheNextOffset (lseek(fd, 0, SEEK_END)), SEEK_SET);
    write (fd, current_dir_block, bytes_to_write);

    /* this actually serves to pad out the cache file */
    if (ftruncate (fd, current_arch_start + truncate_to) == -1)
	goto bail1;

    close(fd);
    return FcTrue;

 bail1:
    free (current_dir_block);
    free (current_arch_machine_name);
 bail:
    unlink (cache_file);
    free (cache_file);
    return FcFalse;
}

/* if true, ignore the cache file */
void
FcCacheForce (FcBool f)
{
    force = f;
}

static int banks_ptr = 0, banks_alloc = 0;
static int * bankId = 0;

int
FcCacheBankCount (void)
{
    return banks_ptr;
}

FcBool
FcCacheHaveBank (int bank)
{
    int i;

    if (bank < FC_BANK_FIRST)
	return FcTrue;

    for (i = 0; i < banks_ptr; i++)
	if (bankId[i] == bank)
	    return FcTrue;

    return FcFalse;
}

int
FcCacheBankToIndex (int bank)
{
    static int lastBank = FC_BANK_DYNAMIC, lastIndex = -1;
    int i;
    int * b;

    if (bank == lastBank)
	return lastIndex;

    for (i = 0; i < banks_ptr; i++)
	if (bankId[i] == bank)
	    return i;

    if (banks_ptr <= banks_alloc)
    {
	b = realloc (bankId, banks_alloc + 4);
	if (!b)
	    return -1;

	bankId = b;
	banks_alloc += 4;
    }

    i = banks_ptr++;
    bankId[i] = bank;
    return i;
}
