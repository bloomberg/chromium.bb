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

#define ENDIAN_TEST 0x12345678
#define MACHINE_SIGNATURE_SIZE 9 + 5*19 + 1

static char *
FcCacheProduceMachineSignature (void);

/*
 * POSIX has broken stdio so that getc must do thread-safe locking,
 * this is a serious performance problem for applications doing large
 * amounts of IO with getc (as is done here).  If available, use
 * the getc_unlocked varient instead.
 */
 
#if defined(getc_unlocked) || defined(_IO_getc_unlocked)
#define GETC(f) getc_unlocked(f)
#define PUTC(c,f) putc_unlocked(c,f)
#else
#define GETC(f) getc(f)
#define PUTC(c,f) putc(c,f)
#endif

#define FC_DBG_CACHE_REF    1024

#define PAGESIZE 8192

static FcBool force;

static FcChar8 *
FcCacheReadString (FILE *f, FcChar8 *dest, int len)
{
    int		c;
    FcBool	escape;
    FcChar8	*d;
    int		size;
    int		i;

    while ((c = GETC (f)) != EOF)
	if (c == '"')
	    break;
    if (c == EOF)
	return FcFalse;
    if (len == 0)
	return FcFalse;
    
    size = len;
    i = 0;
    d = dest;
    escape = FcFalse;
    while ((c = GETC (f)) != EOF)
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
	    FcChar8 *new = malloc (size * 2);	/* freed in caller */
	    if (!new)
		break;
	    memcpy (new, d, size);
	    size *= 2;
	    if (d != dest)
		free (d);
	    d = new;
	}
	d[i++] = c;
	if (c == '\0')
	    return d;
	escape = FcFalse;
    }
    if (d != dest)
	free (d);
    return 0;
}

static FcBool
FcCacheReadUlong (FILE *f, unsigned long *dest)
{
    unsigned long   t;
    int		    c;

    while ((c = GETC (f)) != EOF)
    {
	if (!isspace (c))
	    break;
    }
    if (c == EOF)
	return FcFalse;
    t = 0;
    for (;;)
    {
	if (c == EOF || isspace (c))
	    break;
	if (!isdigit (c))
	    return FcFalse;
	t = t * 10 + (c - '0');
	c = GETC (f);
    }
    *dest = t;
    return FcTrue;
}

static FcBool
FcCacheReadInt (FILE *f, int *dest)
{
    unsigned long   t;
    FcBool	    ret;

    ret = FcCacheReadUlong (f, &t);
    if (ret)
	*dest = (int) t;
    return ret;
}

static FcBool
FcCacheReadTime (FILE *f, time_t *dest)
{
    unsigned long   t;
    FcBool	    ret;

    ret = FcCacheReadUlong (f, &t);
    if (ret)
	*dest = (time_t) t;
    return ret;
}

static FcBool
FcCacheWriteChars (FILE *f, const FcChar8 *chars)
{
    FcChar8    c;
    while ((c = *chars++))
    {
	switch (c) {
	case '"':
	case '\\':
	    if (PUTC ('\\', f) == EOF)
		return FcFalse;
	    /* fall through */
	default:
	    if (PUTC (c, f) == EOF)
		return FcFalse;
	}
    }
    return FcTrue;
}

static FcBool
FcCacheWriteString (FILE *f, const FcChar8 *string)
{

    if (PUTC ('"', f) == EOF)
	return FcFalse;
    if (!FcCacheWriteChars (f, string))
	return FcFalse;
    if (PUTC ('"', f) == EOF)
	return FcFalse;
    return FcTrue;
}

static FcBool
FcCacheWritePath (FILE *f, const FcChar8 *dir, const FcChar8 *file)
{
    if (PUTC ('"', f) == EOF)
	return FcFalse;
    if (dir)
	if (!FcCacheWriteChars (f, dir))
	    return FcFalse;
#ifdef _WIN32
    if (dir &&
	dir[strlen((const char *) dir) - 1] != '/' &&
	dir[strlen((const char *) dir) - 1] != '\\')
    {
	if (!FcCacheWriteChars (f, "\\"))
	    return FcFalse;
    }
#else
    if (dir && dir[strlen((const char *) dir) - 1] != '/')
	if (PUTC ('/', f) == EOF)
	    return FcFalse;
#endif
    if (!FcCacheWriteChars (f, file))
	return FcFalse;
    if (PUTC ('"', f) == EOF)
	return FcFalse;
    return FcTrue;
}

static FcBool
FcCacheWriteUlong (FILE *f, unsigned long t)
{
    int	    pow;
    unsigned long   temp, digit;

    temp = t;
    pow = 1;
    while (temp >= 10)
    {
	temp /= 10;
	pow *= 10;
    }
    temp = t;
    while (pow)
    {
	digit = temp / pow;
	if (PUTC ((char) digit + '0', f) == EOF)
	    return FcFalse;
	temp = temp - pow * digit;
	pow = pow / 10;
    }
    return FcTrue;
}

static FcBool
FcCacheWriteInt (FILE *f, int i)
{
    return FcCacheWriteUlong (f, (unsigned long) i);
}

static FcBool
FcCacheWriteTime (FILE *f, time_t t)
{
    return FcCacheWriteUlong (f, (unsigned long) t);
}

static FcChar8 *
FcCacheReadString2 (int fd, FcChar8 *dest, int len)
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
FcCacheWriteString2 (int fd, const FcChar8 *chars)
{
    if (write (fd, chars, strlen(chars)+1) != strlen(chars)+1)
	return FcFalse;
    return FcTrue;
}

static FcBool
FcCacheFontSetAdd (FcFontSet	    *set,
		   FcStrSet	    *dirs,
		   const FcChar8    *dir,
		   int		    dir_len,
		   const FcChar8    *file,
		   const FcChar8    *name,
		   FcConfig	    *config)
{
    FcChar8	path_buf[8192], *path;
    int		len;
    FcBool	ret = FcFalse;
    FcPattern	*font;
    FcPattern	*frozen;

    path = path_buf;
    len = (dir_len + 1 + strlen ((const char *) file) + 1);
    if (len > sizeof (path_buf))
    {
	path = malloc (len);	/* freed down below */
	if (!path)
	    return FcFalse;
    }
    strncpy ((char *) path, (const char *) dir, dir_len);
#ifdef _WIN32
    if (dir[dir_len - 1] != '/' && dir[dir_len - 1] != '\\' )
	path[dir_len++] = '\\';
#else
    if (dir[dir_len - 1] != '/')
	path[dir_len++] = '/';
#endif
    strcpy ((char *) path + dir_len, (const char *) file);
    if (config && !FcConfigAcceptFilename (config, path))
	ret = FcTrue;
    else if (!FcStrCmp (name, FC_FONT_FILE_DIR))
    {
	if (FcDebug () & FC_DBG_CACHEV)
	    printf (" dir cache dir \"%s\"\n", path);
	ret = FcStrSetAdd (dirs, path);
    }
    else if (!FcStrCmp (name, FC_FONT_FILE_INVALID))
    {
	ret = FcTrue;
    }
    else
    {
	font = FcNameParse (name);
	if (font)
	{
	    FcChar8 *family;
	    
	    if (FcDebug () & FC_DBG_CACHEV)
		printf (" dir cache file \"%s\"\n", file);
	    ret = FcPatternAddString (font, FC_FILE, path);
	    /*
	     * Make sure the pattern has the file name as well as
	     * already containing at least one family name.
	     */
	    if (ret && 
		FcPatternGetString (font, FC_FAMILY, 0, &family) == FcResultMatch &&
		(!config || FcConfigAcceptFont (config, font)))
	    {
		frozen = FcPatternFreeze (font);
		ret = (frozen != 0);
		if (ret)
		   ret = FcFontSetAdd (set, frozen);
	    }
	    FcPatternDestroy (font);
	}
    }
    if (path != path_buf) free (path);
    return ret;
    
}

static unsigned int
FcCacheHash (const FcChar8 *string, int len)
{
    unsigned int    h = 0;
    FcChar8	    c;

    while (len-- && (c = *string++))
	h = (h << 1) ^ c;
    return h;
}

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

FcGlobalCacheDir *
FcGlobalCacheDirGet (FcGlobalCache  *cache,
		     const FcChar8  *dir,
		     int	    len,
		     FcBool	    create_missing)
{
    unsigned int	hash = FcCacheHash (dir, len);
    FcGlobalCacheDir	*d, **prev;

    for (prev = &cache->ents[hash % FC_GLOBAL_CACHE_DIR_HASH_SIZE];
	 (d = *prev);
	 prev = &(*prev)->next)
    {
	if (d->info.hash == hash && d->len == len &&
	    !strncmp ((const char *) d->info.file,
		      (const char *) dir, len))
	    break;
    }
    if (!(d = *prev))
    {
	int	i;
	if (!create_missing)
	    return 0;
	d = malloc (sizeof (FcGlobalCacheDir) + len + 1);
	if (!d)
	    return 0;
	FcMemAlloc (FC_MEM_CACHE, sizeof (FcGlobalCacheDir) + len + 1);
	d->next = *prev;
	*prev = d;
	d->info.hash = hash;
	d->info.file = (FcChar8 *) (d + 1);
	strncpy ((char *) d->info.file, (const char *) dir, len);
	d->info.file[len] = '\0';
	d->info.time = 0;
	d->info.referenced = FcFalse;
	d->len = len;
	for (i = 0; i < FC_GLOBAL_CACHE_FILE_HASH_SIZE; i++)
	    d->ents[i] = 0;
	d->subdirs = 0;
    }
    return d;
}

static FcGlobalCacheInfo *
FcGlobalCacheDirAdd (FcGlobalCache  *cache,
		     const FcChar8  *dir,
		     time_t	    time,
		     FcBool	    replace,
		     FcBool	    create_missing)
{
    FcGlobalCacheDir	*d;
    FcFilePathInfo	i;
    FcGlobalCacheSubdir	*subdir;
    FcGlobalCacheDir	*parent;

    i = FcFilePathInfoGet (dir);
    parent = FcGlobalCacheDirGet (cache, i.dir, i.dir_len, create_missing);
    /*
     * Tricky here -- directories containing fonts.cache-1 files
     * need entries only when the parent doesn't have a cache file.
     * That is, when the parent already exists in the cache, is
     * referenced and has a "real" timestamp.  The time of 0 is
     * special and marks directories which got stuck in the
     * global cache for this very reason.  Yes, it could
     * use a separate boolean field, and probably should.
     */
    if (!parent || (!create_missing && 
		    (!parent->info.referenced ||
		    (parent->info.time == 0))))
	return 0;
    /*
     * Add this directory to the cache
     */
    d = FcGlobalCacheDirGet (cache, dir, strlen ((const char *) dir), FcTrue);
    if (!d)
	return 0;
    d->info.time = time;
    /*
     * Add this directory to the subdirectory list of the parent
     */
    subdir = malloc (sizeof (FcGlobalCacheSubdir));
    if (!subdir)
	return 0;
    FcMemAlloc (FC_MEM_CACHE, sizeof (FcGlobalCacheSubdir));
    subdir->ent = d;
    subdir->next = parent->subdirs;
    parent->subdirs = subdir;
    return &d->info;
}

static void
FcGlobalCacheDirDestroy (FcGlobalCacheDir *d)
{
    FcGlobalCacheFile	*f, *next;
    int			h;
    FcGlobalCacheSubdir	*s, *nexts;

    for (h = 0; h < FC_GLOBAL_CACHE_FILE_HASH_SIZE; h++)
	for (f = d->ents[h]; f; f = next)
	{
	    next = f->next;
	    FcMemFree (FC_MEM_CACHE, sizeof (FcGlobalCacheFile) +
		       strlen ((char *) f->info.file) + 1 +
		       strlen ((char *) f->name) + 1);
	    free (f);
	}
    for (s = d->subdirs; s; s = nexts)
    {
	nexts = s->next;
	FcMemFree (FC_MEM_CACHE, sizeof (FcGlobalCacheSubdir));
	free (s);
    }
    FcMemFree (FC_MEM_CACHE, sizeof (FcGlobalCacheDir) + d->len + 1);
    free (d);
}

/*
 * If the parent is in the global cache and referenced, add
 * an entry for 'dir' to the global cache.  This is used
 * for directories with fonts.cache files
 */

void
FcGlobalCacheReferenceSubdir (FcGlobalCache *cache,
			      const FcChar8 *dir)
{
    FcGlobalCacheInfo	*info;
    info = FcGlobalCacheDirAdd (cache, dir, 0, FcFalse, FcFalse);
    if (info && !info->referenced)
    {
	info->referenced = FcTrue;
	cache->referenced++;
    }
}

/*
 * Check to see if the global cache contains valid data for 'dir'.
 * If so, scan the global cache for files and directories in 'dir'.
 * else, return False.
 */
FcBool
FcGlobalCacheScanDir (FcFontSet		*set,
		      FcStrSet		*dirs,
		      FcGlobalCache	*cache,
		      const FcChar8	*dir,
		      FcConfig		*config)
{
    FcGlobalCacheDir	*d = FcGlobalCacheDirGet (cache, dir,
						  strlen ((const char *) dir),
						  FcFalse);
    FcGlobalCacheFile	*f;
    int			h;
    int			dir_len;
    FcGlobalCacheSubdir	*subdir;
    FcBool		any_in_cache = FcFalse;

    if (FcDebug() & FC_DBG_CACHE)
	printf ("FcGlobalCacheScanDir %s\n", dir);
    
    if (!d)
    {
	if (FcDebug () & FC_DBG_CACHE)
	    printf ("\tNo dir cache entry\n");
	return FcFalse;
    }

    /*
     * See if the timestamp recorded in the global cache
     * matches the directory time, if not, return False
     */
    if (!FcGlobalCacheCheckTime (d->info.file, &d->info))
    {
	if (FcDebug () & FC_DBG_CACHE)
	    printf ("\tdir cache entry time mismatch\n");
	return FcFalse;
    }

    /*
     * Add files from 'dir' to the fontset
     */
    dir_len = strlen ((const char *) dir);
    for (h = 0; h < FC_GLOBAL_CACHE_FILE_HASH_SIZE; h++)
	for (f = d->ents[h]; f; f = f->next)
	{
	    if (FcDebug() & FC_DBG_CACHEV)
		printf ("FcGlobalCacheScanDir add file %s\n", f->info.file);
	    any_in_cache = FcTrue;
	    if (!FcCacheFontSetAdd (set, dirs, dir, dir_len,
				    f->info.file, f->name, config))
	    {
		cache->broken = FcTrue;
		return FcFalse;
	    }
	    FcGlobalCacheReferenced (cache, &f->info);
	}
    /*
     * Add directories in 'dir' to 'dirs'
     */
    for (subdir = d->subdirs; subdir; subdir = subdir->next)
    {
	FcFilePathInfo	info = FcFilePathInfoGet (subdir->ent->info.file);
	
        any_in_cache = FcTrue;
	if (!FcCacheFontSetAdd (set, dirs, dir, dir_len,
				info.base, FC_FONT_FILE_DIR, config))
	{
	    cache->broken = FcTrue;
	    return FcFalse;
	}
	FcGlobalCacheReferenced (cache, &subdir->ent->info);
    }
    
    FcGlobalCacheReferenced (cache, &d->info);

    /*
     * To recover from a bug in previous versions of fontconfig,
     * return FcFalse if no entries in the cache were found
     * for this directory.  This will cause any empty directories
     * to get rescanned every time fontconfig is initialized.  This
     * might get removed at some point when the older cache files are
     * presumably fixed.
     */
    return any_in_cache;
}

/*
 * Locate the cache entry for a particular file
 */
FcGlobalCacheFile *
FcGlobalCacheFileGet (FcGlobalCache *cache,
		      const FcChar8 *file,
		      int	    id,
		      int	    *count)
{
    FcFilePathInfo	i = FcFilePathInfoGet (file);
    FcGlobalCacheDir	*d = FcGlobalCacheDirGet (cache, i.dir, 
						  i.dir_len, FcFalse);
    FcGlobalCacheFile	*f, *match = 0;
    int			max = -1;

    if (!d)
	return 0;
    for (f = d->ents[i.base_hash % FC_GLOBAL_CACHE_FILE_HASH_SIZE]; f; f = f->next)
    {
	if (f->info.hash == i.base_hash &&
	    !strcmp ((const char *) f->info.file, (const char *) i.base))
	{
	    if (f->id == id)
		match = f;
	    if (f->id > max)
		max = f->id;
	}
    }
    if (count)
	*count = max + 1;
    return match;
}
    
/*
 * Add a file entry to the cache
 */
static FcGlobalCacheInfo *
FcGlobalCacheFileAdd (FcGlobalCache *cache,
		      const FcChar8 *path,
		      int	    id,
		      time_t	    time,
		      const FcChar8 *name,
		      FcBool	    replace)
{
    FcFilePathInfo	i = FcFilePathInfoGet (path);
    FcGlobalCacheDir	*d = FcGlobalCacheDirGet (cache, i.dir, 
						  i.dir_len, FcTrue);
    FcGlobalCacheFile	*f, **prev;
    int			size;

    if (!d)
	return 0;
    for (prev = &d->ents[i.base_hash % FC_GLOBAL_CACHE_FILE_HASH_SIZE];
	 (f = *prev);
	 prev = &(*prev)->next)
    {
	if (f->info.hash == i.base_hash && 
	    f->id == id &&
	    !strcmp ((const char *) f->info.file, (const char *) i.base))
	{
	    break;
	}
    }
    if (*prev)
    {
	if (!replace)
	    return 0;

	f = *prev;
	if (f->info.referenced)
	    cache->referenced--;
	*prev = f->next;
	FcMemFree (FC_MEM_CACHE, sizeof (FcGlobalCacheFile) +
		   strlen ((char *) f->info.file) + 1 +
		   strlen ((char *) f->name) + 1);
	free (f);
    }
    size = (sizeof (FcGlobalCacheFile) +
	    strlen ((char *) i.base) + 1 +
	    strlen ((char *) name) + 1);
    f = malloc (size);
    if (!f)
	return 0;
    FcMemAlloc (FC_MEM_CACHE, size);
    f->next = *prev;
    *prev = f;
    f->info.hash = i.base_hash;
    f->info.file = (FcChar8 *) (f + 1);
    f->info.time = time;
    f->info.referenced = FcFalse;
    f->id = id;
    f->name = f->info.file + strlen ((char *) i.base) + 1;
    strcpy ((char *) f->info.file, (const char *) i.base);
    strcpy ((char *) f->name, (const char *) name);
    return &f->info;
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

	lseek (fd, current_arch_start, SEEK_SET);
	if (FcCacheReadString2 (fd, candidate_arch_machine_name_count, 
				sizeof (candidate_arch_machine_name_count)) == 0)
	    break;
	if (!strlen(candidate_arch_machine_name_count))
	    return -1;
	bs = strtol(candidate_arch_machine_name_count, &candidate_arch, 16);
	candidate_arch++; /* skip leading space */

	if (strcmp (candidate_arch, arch)==0)
	    break;
	current_arch_start += bs;
    }

    if (strcmp (candidate_arch, arch)!=0)
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
    char candidate_arch_machine_name[MACHINE_SIGNATURE_SIZE + 9];
    long bs;
    int c, bytes_skipped;

    if (!buf)
	return FcFalse;

    lseek (fd, start, SEEK_SET);
    if (FcCacheReadString2 (fd, candidate_arch_machine_name, 
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
	if (!FcDirCacheValid (dir) || !FcDirCacheRead (set, dir))
	{
	    if (FcDebug () & FC_DBG_FONTSET)
		printf ("scan dir %s\n", dir);
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

    if (force)
	goto bail;

    if (FcCacheReadDirs (config, cache, FcConfigGetConfigDirs (config), s))
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
    char candidate_arch_machine_name[9+MACHINE_SIGNATURE_SIZE];
    off_t current_arch_start = 0;

    if (force)
	goto bail;
    if (!cache_file)
        goto bail;

    current_arch_machine_name = FcCacheProduceMachineSignature();
    fd = open(cache_file, O_RDONLY);
    if (fd == -1)
        goto bail;

    current_arch_start = FcCacheSkipToArch(fd, current_arch_machine_name);
    if (current_arch_start < 0)
        goto bail1;

    lseek (fd, current_arch_start, SEEK_SET);
    if (FcCacheReadString2 (fd, candidate_arch_machine_name, 
			   sizeof (candidate_arch_machine_name)) == 0)
	goto bail1;

    // sanity check for endianness issues
    read(fd, &metadata, sizeof(FcCache));
    if (metadata.magic != FC_CACHE_MAGIC)
        goto bail1;

    if (!metadata.count)
	goto bail1;

    off_t pos = FcCacheNextOffset (lseek(fd, 0, SEEK_CUR));
    current_dir_block = mmap (0, metadata.count, 
			      PROT_READ, MAP_SHARED, fd, pos);
    if (current_dir_block == MAP_FAILED)
	perror("");
    
    if (!FcFontSetUnserialize (metadata, set, current_dir_block))
	goto bail1;
	
    close(fd);
    free (cache_file);
    return FcTrue;

 bail1:
    close(fd);
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
    char * current_arch_machine_name, * header;
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

    if ((char *)current_dir_block + bytes_to_write != final_dir_block)
	goto bail;
			      
    if (!FcFontSetSerialize (bank, set))
	goto bail;

    if (FcDebug () & FC_DBG_CACHE)
        printf ("FcDirCacheWriteDir cache_file \"%s\"\n", cache_file);

    fd = open(cache_file, O_RDWR | O_CREAT, 0666);
    if (fd == -1)
        goto bail;

    current_arch_machine_name = FcCacheProduceMachineSignature ();
    current_arch_start = FcCacheSkipToArch(fd, current_arch_machine_name);
    if (current_arch_start < 0)
	current_arch_start = FcCacheNextOffset (lseek(fd, 0, SEEK_END));

    if (!FcCacheMoveDown(fd, current_arch_start))
	goto bail1;

    current_arch_start = lseek(fd, 0, SEEK_CUR);
    if (ftruncate (fd, current_arch_start) == -1)
	goto bail1;

    /* now write the address of the next offset */
    truncate_to = FcCacheNextOffset (FcCacheNextOffset (current_arch_start + metadata_bytes) + bytes_to_write) - current_arch_start;

    header = malloc (10 + strlen (current_arch_machine_name));
    sprintf (header, "%8x ", (int)truncate_to);
    strcat (header, current_arch_machine_name);
    if (!FcCacheWriteString2 (fd, header))
	goto bail1;

    metadata.magic = FC_CACHE_MAGIC;
    write (fd, &metadata, sizeof(FcCache));
    lseek (fd, FcCacheNextOffset (lseek(fd, 0, SEEK_END)), SEEK_SET);
    write (fd, current_dir_block, bytes_to_write);

    /* this actually serves to pad out the cache file, if needed */
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

static char *
FcCacheProduceMachineSignature ()
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
