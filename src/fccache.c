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

#include "fcint.h"

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

FcBool
FcDirCacheValid (const FcChar8 *dir)
{
    FcChar8	*cache_file = FcStrPlus (dir, (FcChar8 *) "/" FC_DIR_CACHE_FILE);
    struct stat	file_stat, dir_stat;

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

FcBool
FcDirCacheReadDir (FcFontSet *set, FcStrSet *dirs, const FcChar8 *dir, FcConfig *config)
{
    FcChar8	    *cache_file = FcStrPlus (dir, (FcChar8 *) "/" FC_DIR_CACHE_FILE);
    FILE	    *f;
    FcChar8	    *base;
    int		    id;
    int		    dir_len;
    FcChar8	    file_buf[8192], *file;
    FcChar8	    name_buf[8192], *name;
    FcBool	    ret = FcFalse;

    if (!cache_file)
	goto bail0;
    
    if (FcDebug () & FC_DBG_CACHE)
	printf ("FcDirCacheReadDir cache_file \"%s\"\n", cache_file);
    
    f = fopen ((char *) cache_file, "r");
    if (!f)
    {
	if (FcDebug () & FC_DBG_CACHE)
	    printf (" no cache file\n");
	goto bail1;
    }

    if (!FcDirCacheValid (dir))
    {
	if (FcDebug () & FC_DBG_CACHE)
	    printf (" cache file older than directory\n");
	goto bail2;
    }
    
    base = (FcChar8 *) strrchr ((char *) cache_file, '/');
    if (!base)
	goto bail2;
    base++;
    dir_len = base - cache_file;
    
    file = 0;
    name = 0;
    while ((file = FcCacheReadString (f, file_buf, sizeof (file_buf))) &&
	   FcCacheReadInt (f, &id) &&
	   (name = FcCacheReadString (f, name_buf, sizeof (name_buf))))
    {
	if (!FcCacheFontSetAdd (set, dirs, cache_file, dir_len,
				file, name, config))
	    goto bail3;
	if (file != file_buf)
	    free (file);
	if (name != name_buf)
	    free (name);
	file = name = 0;
    }
    if (FcDebug () & FC_DBG_CACHE)
	printf (" cache loaded\n");
    
    ret = FcTrue;
bail3:
    if (file && file != file_buf)
	free (file);
    if (name && name != name_buf)
	free (name);
bail2:
    fclose (f);
bail1:
    FcStrFree (cache_file);
bail0:
    return ret;
}

/*
 * return the path from the directory containing 'cache' to 'file'
 */

static const FcChar8 *
FcFileBaseName (const FcChar8 *cache, const FcChar8 *file)
{
    const FcChar8   *cache_slash;

    cache_slash = FcStrLastSlash (cache);
    if (cache_slash && !strncmp ((const char *) cache, (const char *) file,
				 (cache_slash + 1) - cache))
	return file + ((cache_slash + 1) - cache);
    return file;
}

FcBool
FcDirCacheWriteDir (FcFontSet *set, FcStrSet *dirs, const FcChar8 *dir)
{
    FcChar8	    *cache_file = FcStrPlus (dir, (FcChar8 *) "/" FC_DIR_CACHE_FILE);
    FcPattern	    *font;
    FILE	    *f;
    FcChar8	    *name;
    const FcChar8   *file, *base;
    int		    n;
    int		    id;
    FcBool	    ret;
    FcStrList	    *list;

    if (!cache_file)
	goto bail0;
    if (FcDebug () & FC_DBG_CACHE)
	printf ("FcDirCacheWriteDir cache_file \"%s\"\n", cache_file);
    
    f = fopen ((char *) cache_file, "w");
    if (!f)
    {
	if (FcDebug () & FC_DBG_CACHE)
	    printf (" can't create \"%s\"\n", cache_file);
	goto bail1;
    }
    
    list = FcStrListCreate (dirs);
    if (!list)
	goto bail2;
    
    while ((dir = FcStrListNext (list)))
    {
	base = FcFileBaseName (cache_file, dir);
	if (!FcCacheWriteString (f, base))
	    goto bail3;
	if (PUTC (' ', f) == EOF)
	    goto bail3;
	if (!FcCacheWriteInt (f, 0))
	    goto bail3;
        if (PUTC (' ', f) == EOF)
	    goto bail3;
	if (!FcCacheWriteString (f, FC_FONT_FILE_DIR))
	    goto bail3;
	if (PUTC ('\n', f) == EOF)
	    goto bail3;
    }
    
    for (n = 0; n < set->nfont; n++)
    {
	font = set->fonts[n];
	if (FcPatternGetString (font, FC_FILE, 0, (FcChar8 **) &file) != FcResultMatch)
	    goto bail3;
	base = FcFileBaseName (cache_file, file);
	if (FcPatternGetInteger (font, FC_INDEX, 0, &id) != FcResultMatch)
	    goto bail3;
	if (FcDebug () & FC_DBG_CACHEV)
	    printf (" write file \"%s\"\n", base);
	if (!FcCacheWriteString (f, base))
	    goto bail3;
	if (PUTC (' ', f) == EOF)
	    goto bail3;
	if (!FcCacheWriteInt (f, id))
	    goto bail3;
        if (PUTC (' ', f) == EOF)
	    goto bail3;
	name = FcNameUnparse (font);
	if (!name)
	    goto bail3;
	ret = FcCacheWriteString (f, name);
	FcStrFree (name);
	if (!ret)
	    goto bail3;
	if (PUTC ('\n', f) == EOF)
	    goto bail3;
    }
    
    FcStrListDone (list);

    if (fclose (f) == EOF)
	goto bail1;
    
    FcStrFree (cache_file);

    if (FcDebug () & FC_DBG_CACHE)
	printf (" cache written\n");
    return FcTrue;
    
bail3:
    FcStrListDone (list);
bail2:
    fclose (f);
bail1:
    unlink ((char *) cache_file);
    FcStrFree (cache_file);
bail0:
    return FcFalse;
}
