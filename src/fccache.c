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
#include <sys/mman.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <unistd.h>
#include "fcint.h"

#define ENDIAN_TEST 0x12345678
#define MACHINE_SIGNATURE_SIZE 9 + 5*19 + 1

static off_t
FcCacheSkipToArch (int fd, const char * arch);

static FcBool 
FcCacheCopyOld (int fd, int fd_orig, off_t start);

static void *
FcDirCacheProduce (FcFontSet *set, FcCache * metadata);

static FcBool
FcDirCacheConsume (int fd, FcFontSet *set);

static FcBool
FcDirCacheRead (FcFontSet * set, FcStrSet * dirs, const FcChar8 *dir);

static int
FcCacheNextOffset(off_t w);

static char *
FcCacheMachineSignature (void);

static FcBool
FcCacheHaveBank (int bank);

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
		   const FcChar8    *cache_file)
{
    char		name_buf[8192];
    FcGlobalCacheDir	*d, *next;
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

    current_arch_machine_name = FcCacheMachineSignature ();
    current_arch_start = FcCacheSkipToArch(cache->fd, 
					   current_arch_machine_name);
    if (current_arch_start < 0)
        goto bail0;

    lseek (cache->fd, current_arch_start, SEEK_SET);
    FcCacheReadString (cache->fd, candidate_arch_machine_name, 
                       sizeof (candidate_arch_machine_name));
    if (strlen(candidate_arch_machine_name) == 0)
	goto bail0;

    while (1) 
    {
	off_t targ;

	FcCacheReadString (cache->fd, name_buf, sizeof (name_buf));
	if (!strlen(name_buf))
	    break;

        if (stat ((char *) name_buf, &dir_stat) < 0 || 
            dir_stat.st_mtime > cache_stat.st_mtime)
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
 bail0:
    close (cache->fd);
    cache->fd = -1;
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
	    if (!FcDirCacheConsume (cache->fd, set))
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

    if (!set->nfont)
	return FcTrue;

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
	current_arch_start = FcCacheNextOffset (lseek(fd, 0, SEEK_END));

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
	truncate_to = FcCacheNextOffset (current_arch_start + truncate_to);
	truncate_to += dir->metadata.count;
    }
    truncate_to -= current_arch_start;

    sprintf (header, "%8x ", (int)truncate_to);
    strcat (header, current_arch_machine_name);
    if (!FcCacheWriteString (fd, header))
	goto bail4;

    for (dir = cache->dirs; dir; dir = dir->next)
    {
        if (dir->ent)
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

#define PAGESIZE 8192
/* 
 * Find the next presumably-mmapable offset after the supplied file
 * position.
 */
static int
FcCacheNextOffset(off_t w)
{
    if (w % PAGESIZE == 0) 
	return w;
    else
	return ((w / PAGESIZE)+1)*PAGESIZE;
}

/* return the address of the segment for the provided arch,
 * or -1 if arch not found */
static off_t
FcCacheSkipToArch (int fd, const char * arch)
{
    char candidate_arch_machine_name_count[MACHINE_SIGNATURE_SIZE + 9];
    char * candidate_arch;
    off_t current_arch_start = 0;

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

FcBool
FcDirCacheValid (const FcChar8 *dir)
{
    FcChar8     *cache_file = FcStrPlus (dir, (FcChar8 *) "/" FC_DIR_CACHE_FILE);
    struct stat file_stat, dir_stat;

    if (stat ((char *) dir, &dir_stat) < 0)
    {
        FcStrFree (cache_file);
        return FcFalse;
    }
    if (stat ((char *) cache_file, &file_stat) < 0)
    {
        FcStrFree (cache_file);
        return FcFalse;
    }
    FcStrFree (cache_file);
    /*
     * If the directory has been modified more recently than
     * the cache file, the cache is not valid
     */
    if (dir_stat.st_mtime - file_stat.st_mtime > 0)
        return FcFalse;
    return FcTrue;
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

/* read serialized state from the cache file */
static FcBool
FcDirCacheRead (FcFontSet * set, FcStrSet * dirs, const FcChar8 *dir)
{
    char *cache_file = (char *)FcStrPlus (dir, (FcChar8 *) "/" FC_DIR_CACHE_FILE);
    int fd;
    char * current_arch_machine_name;
    char candidate_arch_machine_name[9+MACHINE_SIGNATURE_SIZE];
    off_t current_arch_start = 0;
    char subdirName[FC_MAX_FILE_LEN + 1 + 12 + 1];

    if (!cache_file)
        goto bail;

    current_arch_machine_name = FcCacheMachineSignature();
    fd = open(cache_file, O_RDONLY);
    if (fd == -1)
        goto bail;

    current_arch_start = FcCacheSkipToArch(fd, current_arch_machine_name);
    if (current_arch_start < 0)
        goto bail1;

    lseek (fd, current_arch_start, SEEK_SET);
    if (FcCacheReadString (fd, candidate_arch_machine_name, 
			   sizeof (candidate_arch_machine_name)) == 0)
	goto bail1;

    while (strlen(FcCacheReadString (fd, subdirName, sizeof (subdirName))) > 0)
        FcStrSetAdd (dirs, (FcChar8 *)subdirName);

    if (!FcDirCacheConsume (fd, set))
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
FcDirCacheConsume (int fd, FcFontSet *set)
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
    if (current_dir_block == MAP_FAILED)
	return FcFalse;
    
    if (!FcFontSetUnserialize (metadata, set, current_dir_block))
	return FcFalse;

    return FcTrue;
}

static void *
FcDirCacheProduce (FcFontSet *set, FcCache *metadata)
{
    void * current_dir_block, * final_dir_block;
    static unsigned int rand_state = 0;
    int bank;

    if (!rand_state) 
	rand_state = time(0L);
    bank = rand_r(&rand_state);

    while (FcCacheHaveBank(bank))
	bank = rand_r(&rand_state);

    memset (metadata, 0, sizeof(FcCache));
    FcFontSetNewBank();
    metadata->count = FcFontSetNeededBytes (set);
    metadata->magic = FC_CACHE_MAGIC;
    metadata->bank = bank;

    if (!metadata->count) /* not a failure, no fonts to write */
	return 0;

    current_dir_block = malloc (metadata->count);
    if (!current_dir_block)
	goto bail;
    final_dir_block = FcFontSetDistributeBytes (metadata, current_dir_block);

    if ((char *)current_dir_block + metadata->count != final_dir_block)
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
    FcChar8         *cache_file = FcStrPlus (dir, (FcChar8 *) "/" FC_DIR_CACHE_FILE);
    int 	    fd, fd_orig, i, dirs_count;
    FcAtomic 	    *atomic;
    FcCache 	    metadata;
    off_t 	    current_arch_start = 0, truncate_to;

    char            *current_arch_machine_name, * header;
    void 	    *current_dir_block;

    if (!cache_file)
        goto bail;

    current_dir_block = FcDirCacheProduce (set, &metadata);

    if (metadata.count && !current_dir_block)
	goto bail0;

    if (FcDebug () & FC_DBG_CACHE)
        printf ("FcDirCacheWriteDir cache_file \"%s\"\n", cache_file);

    atomic = FcAtomicCreate (cache_file);
    if (!atomic)
        goto bail0;

    if (!FcAtomicLock (atomic))
        goto bail1;

    fd_orig = open((char *)FcAtomicOrigFile (atomic), O_RDONLY, 0666);

    fd = open((char *)FcAtomicNewFile (atomic), O_RDWR | O_CREAT, 0666);
    if (fd == -1)
        goto bail2;

    current_arch_machine_name = FcCacheMachineSignature ();
    current_arch_start = 0;

    if (fd_orig != -1)
        current_arch_start = 
            FcCacheSkipToArch(fd_orig, current_arch_machine_name);

    if (current_arch_start < 0)
	current_arch_start = FcCacheNextOffset (lseek(fd, 0, SEEK_END));

    if (fd_orig != -1 && !FcCacheCopyOld(fd, fd_orig, current_arch_start))
	goto bail3;

    if (fd_orig != -1)
        close (fd_orig);

    current_arch_start = lseek(fd, 0, SEEK_CUR);
    if (ftruncate (fd, current_arch_start) == -1)
	goto bail3;

    /* allocate space for subdir names in this block */
    dirs_count = 0;
    for (i = 0; i < dirs->size; i++)
        dirs_count += strlen((char *)dirs->strs[i]) + 1;
    dirs_count ++;

    /* now write the address of the next offset */
    truncate_to = FcCacheNextOffset (FcCacheNextOffset (current_arch_start + sizeof (FcCache) + dirs_count) + metadata.count) - current_arch_start;
    header = malloc (10 + strlen (current_arch_machine_name));
    if (!header)
	goto bail3;
    sprintf (header, "%8x ", (int)truncate_to);
    strcat (header, current_arch_machine_name);
    if (!FcCacheWriteString (fd, header))
	goto bail4;

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
	goto bail4;

    close(fd);
    if (!FcAtomicReplaceOrig(atomic))
        goto bail4;
    FcAtomicUnlock (atomic);
    FcAtomicDestroy (atomic);
    return FcTrue;

 bail4:
    free (header);
 bail3:
    close (fd);
 bail2:
    FcAtomicUnlock (atomic);
 bail1:
    FcAtomicDestroy (atomic);
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
    int magic = ENDIAN_TEST;
    char * m = (char *)&magic;

    sprintf (buf, "%2x%2x%2x%2x "
	     "%4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x %4x "
	     "%4x %4x %4x %4x %4x %4x %4x\n", 
	     m[0], m[1], m[2], m[3],
	     sizeof (char),
	     sizeof (char *),
	     sizeof (int),
	     sizeof (FcPattern),
	     sizeof (FcPatternEltPtr),
	     sizeof (struct _FcPatternElt *),
	     sizeof (FcPatternElt),
	     sizeof (FcObjectPtr),
	     sizeof (FcValueListPtr),
	     sizeof (FcValue),
	     sizeof (FcValueBinding),
	     sizeof (struct _FcValueList *),
	     sizeof (FcCharSet),
	     sizeof (FcCharLeaf **),
	     sizeof (FcChar16 *),
	     sizeof (FcChar16),
	     sizeof (FcCharLeaf),
	     sizeof (FcChar32),
	     sizeof (FcCache));

    return buf;
}

static int banks_ptr = 0, banks_alloc = 0;
static int * bankId = 0;

static FcBool
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

    if (banks_ptr >= banks_alloc)
    {
	b = realloc (bankId, (banks_alloc + 4) * sizeof(int));
	if (!b)
	    return -1;

	bankId = b;
	banks_alloc += 4;
    }

    i = banks_ptr++;
    bankId[i] = bank;
    return i;
}
