/*
 * $XFree86: xc/lib/fontconfig/src/fccache.c,v 1.7 2002/05/21 17:06:22 keithp Exp $
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
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

static unsigned int
FcFileCacheHash (const FcChar8	*string)
{
    unsigned int    h = 0;
    FcChar8	    c;

    while ((c = *string++))
	h = (h << 1) ^ c;
    return h;
}

FcChar8 *
FcFileCacheFind (FcFileCache	*cache,
		 const FcChar8	*file,
		 int		id,
		 int		*count)
{
    unsigned int    hash;
    const FcChar8   *match;
    FcFileCacheEnt  *c, *name;
    int		    maxid;
    struct stat	    statb;
    
    match = file;
    
    hash = FcFileCacheHash (match);
    name = 0;
    maxid = -1;
    for (c = cache->ents[hash % FC_FILE_CACHE_HASH_SIZE]; c; c = c->next)
    {
	if (c->hash == hash && !strcmp ((const char *) match, (const char *) c->file))
	{
	    if (c->id > maxid)
		maxid = c->id;
	    if (c->id == id)
	    {
		if (stat ((char *) file, &statb) < 0)
		{
		    if (FcDebug () & FC_DBG_CACHE)
			printf (" file missing\n");
		    return 0;
		}
		if (statb.st_mtime != c->time)
		{
		    if (FcDebug () & FC_DBG_CACHE)
			printf (" timestamp mismatch (was %d is %d)\n",
				(int) c->time, (int) statb.st_mtime);
		    return 0;
		}
		if (!c->referenced)
		{
		    cache->referenced++;
		    c->referenced = FcTrue;
		}
		name = c;
	    }
	}
    }
    if (!name)
	return 0;
    *count = maxid + 1;
    return name->name;
}

/*
 * Cache file syntax is quite simple:
 *
 * "file_name" id time "font_name" \n
 */
 
static FcChar8 *
FcFileCacheReadString (FILE *f, FcChar8 *dest, int len)
{
    int		c;
    FcBool	escape;
    FcChar8	*d;
    int		size;
    int		i;

    while ((c = getc (f)) != EOF)
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
    while ((c = getc (f)) != EOF)
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
	    FcChar8 *new = malloc (size * 2);
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
FcFileCacheReadUlong (FILE *f, unsigned long *dest)
{
    unsigned long   t;
    int		    c;

    while ((c = getc (f)) != EOF)
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
	c = getc (f);
    }
    *dest = t;
    return FcTrue;
}

static FcBool
FcFileCacheReadInt (FILE *f, int *dest)
{
    unsigned long   t;
    FcBool	    ret;

    ret = FcFileCacheReadUlong (f, &t);
    if (ret)
	*dest = (int) t;
    return ret;
}

static FcBool
FcFileCacheReadTime (FILE *f, time_t *dest)
{
    unsigned long   t;
    FcBool	    ret;

    ret = FcFileCacheReadUlong (f, &t);
    if (ret)
	*dest = (time_t) t;
    return ret;
}

static FcBool
FcFileCacheAdd (FcFileCache	*cache,
		 const FcChar8	*file,
		 int		id,
		 time_t		time,
		 const FcChar8	*name,
		 FcBool		replace)
{
    FcFileCacheEnt    *c;
    FcFileCacheEnt    **prev, *old;
    unsigned int    hash;

    if (FcDebug () & FC_DBG_CACHE)
    {
	printf ("%s face %s/%d as %s\n", replace ? "Replace" : "Add",
		file, id, name);
    }
    hash = FcFileCacheHash (file);
    for (prev = &cache->ents[hash % FC_FILE_CACHE_HASH_SIZE]; 
	 (old = *prev);
	 prev = &(*prev)->next)
    {
	if (old->hash == hash && old->id == id && !strcmp ((const char *) old->file,
							   (const char *) file))
	    break;
    }
    if (*prev)
    {
	if (!replace)
	    return FcFalse;

	old = *prev;
	if (old->referenced)
	    cache->referenced--;
	*prev = old->next;
	free (old);
	cache->entries--;
    }
	
    c = malloc (sizeof (FcFileCacheEnt) +
		strlen ((char *) file) + 1 +
		strlen ((char *) name) + 1);
    if (!c)
	return FcFalse;
    c->next = *prev;
    *prev = c;
    c->hash = hash;
    c->file = (FcChar8 *) (c + 1);
    c->id = id;
    c->name = c->file + strlen ((char *) file) + 1;
    strcpy ((char *) c->file, (const char *) file);
    c->time = time;
    c->referenced = replace;
    strcpy ((char *) c->name, (const char *) name);
    cache->entries++;
    return FcTrue;
}

FcFileCache *
FcFileCacheCreate (void)
{
    FcFileCache	*cache;
    int		h;

    cache = malloc (sizeof (FcFileCache));
    if (!cache)
	return 0;
    for (h = 0; h < FC_FILE_CACHE_HASH_SIZE; h++)
	cache->ents[h] = 0;
    cache->entries = 0;
    cache->referenced = 0;
    cache->updated = FcFalse;
    return cache;
}

void
FcFileCacheDestroy (FcFileCache *cache)
{
    FcFileCacheEnt *c, *next;
    int		    h;

    for (h = 0; h < FC_FILE_CACHE_HASH_SIZE; h++)
    {
	for (c = cache->ents[h]; c; c = next)
	{
	    next = c->next;
	    free (c);
	}
    }
    free (cache);
}

void
FcFileCacheLoad (FcFileCache	*cache,
		 const FcChar8	*cache_file)
{
    FILE	    *f;
    FcChar8	    file_buf[8192], *file;
    int		    id;
    time_t	    time;
    FcChar8	    name_buf[8192], *name;

    f = fopen ((char *) cache_file, "r");
    if (!f)
	return;

    cache->updated = FcFalse;
    file = 0;
    name = 0;
    while ((file = FcFileCacheReadString (f, file_buf, sizeof (file_buf))) &&
	   FcFileCacheReadInt (f, &id) &&
	   FcFileCacheReadTime (f, &time) &&
	   (name = FcFileCacheReadString (f, name_buf, sizeof (name_buf))))
    {
	(void) FcFileCacheAdd (cache, file, id, time, name, FcFalse);
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
FcFileCacheUpdate (FcFileCache	    *cache,
		   const FcChar8    *file,
		   int		    id,
		   const FcChar8    *name)
{
    const FcChar8   *match;
    struct stat	    statb;
    FcBool	    ret;

    match = file;

    if (stat ((char *) file, &statb) < 0)
	return FcFalse;
    ret = FcFileCacheAdd (cache, match, id, 
			    statb.st_mtime, name, FcTrue);
    if (ret)
	cache->updated = FcTrue;
    return ret;
}

static FcBool
FcFileCacheWriteString (FILE *f, const FcChar8 *string)
{
    char    c;

    if (putc ('"', f) == EOF)
	return FcFalse;
    while ((c = *string++))
    {
	switch (c) {
	case '"':
	case '\\':
	    if (putc ('\\', f) == EOF)
		return FcFalse;
	    /* fall through */
	default:
	    if (putc (c, f) == EOF)
		return FcFalse;
	}
    }
    if (putc ('"', f) == EOF)
	return FcFalse;
    return FcTrue;
}

static FcBool
FcFileCacheWriteUlong (FILE *f, unsigned long t)
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
	if (putc ((char) digit + '0', f) == EOF)
	    return FcFalse;
	temp = temp - pow * digit;
	pow = pow / 10;
    }
    return FcTrue;
}

static FcBool
FcFileCacheWriteInt (FILE *f, int i)
{
    return FcFileCacheWriteUlong (f, (unsigned long) i);
}

static FcBool
FcFileCacheWriteTime (FILE *f, time_t t)
{
    return FcFileCacheWriteUlong (f, (unsigned long) t);
}

FcBool
FcFileCacheSave (FcFileCache	*cache,
		 const FcChar8	*cache_file)
{
    FILE	    *f;
    int		    h;
    FcFileCacheEnt  *c;
    FcAtomic	    *atomic;

    if (!cache->updated && cache->referenced == cache->entries)
	return FcTrue;
    
    /* Set-UID programs can't safely update the cache */
    if (getuid () != geteuid ())
	return FcFalse;
    
    atomic = FcAtomicCreate (cache_file);
    if (!atomic)
	goto bail0;
    if (!FcAtomicLock (atomic))
	goto bail1;
    f = fopen ((char *) FcAtomicNewFile(atomic), "w");
    if (!f)
	goto bail2;

    for (h = 0; h < FC_FILE_CACHE_HASH_SIZE; h++)
    {
	for (c = cache->ents[h]; c; c = c->next)
	{
	    if (!c->referenced)
		continue;
	    if (!FcFileCacheWriteString (f, c->file))
		goto bail4;
	    if (putc (' ', f) == EOF)
		goto bail4;
	    if (!FcFileCacheWriteInt (f, c->id))
		goto bail4;
	    if (putc (' ', f) == EOF)
		goto bail4;
	    if (!FcFileCacheWriteTime (f, c->time))
		goto bail4;
	    if (putc (' ', f) == EOF)
		goto bail4;
	    if (!FcFileCacheWriteString (f, c->name))
		goto bail4;
	    if (putc ('\n', f) == EOF)
		goto bail4;
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
FcFileCacheValid (const FcChar8 *cache_file)
{
    FcChar8	*dir = FcStrDirname (cache_file);
    struct stat	file_stat, dir_stat;

    if (!dir)
	return FcFalse;
    if (stat ((char *) dir, &dir_stat) < 0)
    {
	FcStrFree (dir);
	return FcFalse;
    }
    FcStrFree (dir);
    if (stat ((char *) cache_file, &file_stat) < 0)
	return FcFalse;
    /*
     * If the directory has been modified more recently than
     * the cache file, the cache is not valid
     */
    if (dir_stat.st_mtime - file_stat.st_mtime > 0)
	return FcFalse;
    return FcTrue;
}

FcBool
FcFileCacheReadDir (FcFontSet *set, FcStrSet *dirs, const FcChar8 *cache_file)
{
    FcPattern	    *font;
    FILE	    *f;
    FcChar8	    *base;
    int		    id;
    int		    dir_len;
    int		    file_len;
    FcChar8	    file_buf[8192], *file;
    FcChar8	    name_buf[8192], *name;
    FcChar8	    path_buf[8192], *path;
    FcBool	    ret = FcFalse;

    if (FcDebug () & FC_DBG_CACHE)
    {
	printf ("FcFileCacheReadDir cache_file \"%s\"\n", cache_file);
    }
    
    f = fopen ((char *) cache_file, "r");
    if (!f)
    {
	if (FcDebug () & FC_DBG_CACHE)
	{
	    printf (" no cache file\n");
	}
	goto bail0;
    }

    if (!FcFileCacheValid (cache_file))
    {
	if (FcDebug () & FC_DBG_CACHE)
	{
	    printf (" cache file older than directory\n");
	}
	goto bail1;
    }
    
    base = (FcChar8 *) strrchr ((char *) cache_file, '/');
    if (!base)
	goto bail1;
    base++;
    dir_len = base - cache_file;
    if (dir_len < sizeof (path_buf))
	strncpy ((char *) path_buf, (const char *) cache_file, dir_len);
    
    file = 0;
    name = 0;
    path = 0;
    while ((file = FcFileCacheReadString (f, file_buf, sizeof (file_buf))) &&
	   FcFileCacheReadInt (f, &id) &&
	   (name = FcFileCacheReadString (f, name_buf, sizeof (name_buf))))
    {
	file_len = strlen ((const char *) file);
	if (dir_len + file_len + 1 > sizeof (path_buf))
	{
	    path = malloc (dir_len + file_len + 1);
	    if (!path)
		goto bail2;
	    strncpy ((char *) path, (const char *) cache_file, dir_len);
	}
	else
	    path = path_buf;
	
    	strcpy ((char *) path + dir_len, (const char *) file);
	if (!FcStrCmp (name, FC_FONT_FILE_DIR))
	{
	    if (FcDebug () & FC_DBG_CACHEV)
	    {
		printf (" dir cache dir \"%s\"\n", path);
	    }
	    if (!FcStrSetAdd (dirs, path))
		goto bail2;
	}
	else
	{
	    font = FcNameParse (name);
	    if (font)
	    {
		if (FcDebug () & FC_DBG_CACHEV)
		{
		    printf (" dir cache file \"%s\"\n", file);
		}
		if (!FcPatternAddString (font, FC_FILE, path))
		{
		    FcPatternDestroy (font);
		    goto bail2;
		}
		if (!FcFontSetAdd (set, font))
		{
		    FcPatternDestroy (font);
		    goto bail2;
		}
	    }
	}
	if (path != path_buf)
	    free (path);
	if (file != file_buf)
	    free (file);
	if (name != name_buf)
	    free (name);
	path = file = name = 0;
    }
    if (FcDebug () & FC_DBG_CACHE)
    {
	printf (" cache loaded\n");
    }
    
    ret = FcTrue;
bail2:
    if (path && path != path_buf)
	free (path);
    if (file && file != file_buf)
	free (file);
    if (name && name != name_buf)
	free (name);
bail1:
    fclose (f);
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

    cache_slash = (const FcChar8 *) strrchr ((const char *) cache, '/');
    if (cache_slash && !strncmp ((const char *) cache, (const char *) file,
				 (cache_slash + 1) - cache))
	return file + ((cache_slash + 1) - cache);
    return file;
}

FcBool
FcFileCacheWriteDir (FcFontSet *set, FcStrSet *dirs, const FcChar8 *cache_file)
{
    FcPattern	    *font;
    FILE	    *f;
    FcChar8	    *name;
    const FcChar8   *file, *base;
    int		    n;
    int		    id;
    FcBool	    ret;
    FcStrList	    *list;
    FcChar8	    *dir;

    if (FcDebug () & FC_DBG_CACHE)
	printf ("FcFileCacheWriteDir cache_file \"%s\"\n", cache_file);
    
    f = fopen ((char *) cache_file, "w");
    if (!f)
    {
	if (FcDebug () & FC_DBG_CACHE)
	    printf (" can't create \"%s\"\n", cache_file);
	goto bail0;
    }
    
    list = FcStrListCreate (dirs);
    if (!list)
	goto bail1;
    
    while ((dir = FcStrListNext (list)))
    {
	base = FcFileBaseName (cache_file, dir);
	if (!FcFileCacheWriteString (f, base))
	    goto bail2;
	if (putc (' ', f) == EOF)
	    goto bail2;
	if (!FcFileCacheWriteInt (f, 0))
	    goto bail2;
        if (putc (' ', f) == EOF)
	    goto bail2;
	if (!FcFileCacheWriteString (f, FC_FONT_FILE_DIR))
	    goto bail2;
	if (putc ('\n', f) == EOF)
	    goto bail2;
    }
    
    for (n = 0; n < set->nfont; n++)
    {
	font = set->fonts[n];
	if (FcPatternGetString (font, FC_FILE, 0, (FcChar8 **) &file) != FcResultMatch)
	    goto bail2;
	base = FcFileBaseName (cache_file, file);
	if (FcPatternGetInteger (font, FC_INDEX, 0, &id) != FcResultMatch)
	    goto bail2;
	if (FcDebug () & FC_DBG_CACHEV)
	    printf (" write file \"%s\"\n", base);
	if (!FcFileCacheWriteString (f, base))
	    goto bail2;
	if (putc (' ', f) == EOF)
	    goto bail2;
	if (!FcFileCacheWriteInt (f, id))
	    goto bail2;
        if (putc (' ', f) == EOF)
	    goto bail2;
	name = FcNameUnparse (font);
	if (!name)
	    goto bail2;
	ret = FcFileCacheWriteString (f, name);
	free (name);
	if (!ret)
	    goto bail2;
	if (putc ('\n', f) == EOF)
	    goto bail2;
    }
    
    FcStrListDone (list);

    if (fclose (f) == EOF)
	goto bail0;
    
    if (FcDebug () & FC_DBG_CACHE)
	printf (" cache written\n");
    return FcTrue;
    
bail2:
    FcStrListDone (list);
bail1:
    fclose (f);
bail0:
    unlink ((char *) cache_file);
    return FcFalse;
}
