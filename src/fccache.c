/*
 * $XFree86: $
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
FcFileCacheHash (const char *string)
{
    unsigned int    h = 0;
    char	    c;

    while ((c = *string++))
	h = (h << 1) ^ c;
    return h;
}

char *
FcFileCacheFind (FcFileCache	*cache,
		 const char	*file,
		 int		id,
		 int		*count)
{
    unsigned int    hash;
    const char	    *match;
    FcFileCacheEnt *c, *name;
    int		    maxid;
    struct stat	    statb;
    
    match = file;
    
    hash = FcFileCacheHash (match);
    name = 0;
    maxid = -1;
    for (c = cache->ents[hash % FC_FILE_CACHE_HASH_SIZE]; c; c = c->next)
    {
	if (c->hash == hash && !strcmp (match, c->file))
	{
	    if (c->id > maxid)
		maxid = c->id;
	    if (c->id == id)
	    {
		if (stat (file, &statb) < 0)
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
 
static FcBool
FcFileCacheReadString (FILE *f, char *dest, int len)
{
    int	    c;
    FcBool    escape;

    while ((c = getc (f)) != EOF)
	if (c == '"')
	    break;
    if (c == EOF)
	return FcFalse;
    if (len == 0)
	return FcFalse;
    
    escape = FcFalse;
    while ((c = getc (f)) != EOF)
    {
	if (!escape)
	{
	    switch (c) {
	    case '"':
		*dest++ = '\0';
		return FcTrue;
	    case '\\':
		escape = FcTrue;
		continue;
	    }
	}
        if (--len <= 1)
	    return FcFalse;
	*dest++ = c;
	escape = FcFalse;
    }
    return FcFalse;
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
		 const char	*file,
		 int		id,
		 time_t		time,
		 const char	*name,
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
	if (old->hash == hash && old->id == id && !strcmp (old->file, file))
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
		strlen (file) + 1 +
		strlen (name) + 1);
    if (!c)
	return FcFalse;
    c->next = *prev;
    *prev = c;
    c->hash = hash;
    c->file = (char *) (c + 1);
    c->id = id;
    c->name = c->file + strlen (file) + 1;
    strcpy (c->file, file);
    c->time = time;
    c->referenced = replace;
    strcpy (c->name, name);
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
		 const char	*cache_file)
{
    FILE	    *f;
    char	    file[8192];
    int		    id;
    time_t	    time;
    char	    name[8192];

    f = fopen (cache_file, "r");
    if (!f)
	return;

    cache->updated = FcFalse;
    while (FcFileCacheReadString (f, file, sizeof (file)) &&
	   FcFileCacheReadInt (f, &id) &&
	   FcFileCacheReadTime (f, &time) &&
	   FcFileCacheReadString (f, name, sizeof (name)))
    {
	(void) FcFileCacheAdd (cache, file, id, time, name, FcFalse);
    }
    fclose (f);
}

FcBool
FcFileCacheUpdate (FcFileCache	*cache,
		   const char	*file,
		   int		id,
		   const char	*name)
{
    const char	    *match;
    struct stat	    statb;
    FcBool	    ret;

    match = file;

    if (stat (file, &statb) < 0)
	return FcFalse;
    ret = FcFileCacheAdd (cache, match, id, 
			    statb.st_mtime, name, FcTrue);
    if (ret)
	cache->updated = FcTrue;
    return ret;
}

static FcBool
FcFileCacheWriteString (FILE *f, char *string)
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
		 const char	*cache_file)
{
    char	    *lck;
    char	    *tmp;
    FILE	    *f;
    int		    h;
    FcFileCacheEnt *c;

    if (!cache->updated && cache->referenced == cache->entries)
	return FcTrue;
    
    lck = malloc (strlen (cache_file)*2 + 4);
    if (!lck)
	goto bail0;
    tmp = lck + strlen (cache_file) + 2;
    strcpy (lck, cache_file);
    strcat (lck, "L");
    strcpy (tmp, cache_file);
    strcat (tmp, "T");
    if (link (lck, cache_file) < 0 && errno != ENOENT)
	goto bail1;
    if (access (tmp, F_OK) == 0)
	goto bail2;
    f = fopen (tmp, "w");
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
    
    if (rename (tmp, cache_file) < 0)
	goto bail3;
    
    unlink (lck);
    cache->updated = FcFalse;
    return FcTrue;

bail4:
    fclose (f);
bail3:
    unlink (tmp);
bail2:
    unlink (lck);
bail1:
    free (lck);
bail0:
    return FcFalse;
}

FcBool
FcFileCacheReadDir (FcFontSet *set, const char *cache_file)
{
    FcPattern	    *font;
    FILE	    *f;
    char	    *path;
    char	    *base;
    char	    file[8192];
    int		    id;
    char	    name[8192];
    FcBool	    ret = FcFalse;

    if (FcDebug () & FC_DBG_CACHE)
    {
	printf ("FcFileCacheReadDir cache_file \"%s\"\n", cache_file);
    }
    
    f = fopen (cache_file, "r");
    if (!f)
    {
	if (FcDebug () & FC_DBG_CACHE)
	{
	    printf (" no cache file\n");
	}
	goto bail0;
    }

    base = strrchr (cache_file, '/');
    if (!base)
	goto bail1;
    base++;
    path = malloc (base - cache_file + 8192 + 1);
    if (!path)
	goto bail1;
    memcpy (path, cache_file, base - cache_file);
    base = path + (base - cache_file);
    
    while (FcFileCacheReadString (f, file, sizeof (file)) &&
	   FcFileCacheReadInt (f, &id) &&
	   FcFileCacheReadString (f, name, sizeof (name)))
    {
	font = FcNameParse (name);
	if (font)
	{
	    strcpy (base, file);
	    if (FcDebug () & FC_DBG_CACHEV)
	    {
		printf (" dir cache file \"%s\"\n", file);
	    }
	    FcPatternAddString (font, FC_FILE, path);
	    if (!FcFontSetAdd (set, font))
		goto bail2;
	}
    }
    if (FcDebug () & FC_DBG_CACHE)
    {
	printf (" cache loaded\n");
    }
    
    ret = FcTrue;
bail2:
    free (path);
bail1:
    fclose (f);
bail0:
    return ret;
}

FcBool
FcFileCacheWriteDir (FcFontSet *set, const char *cache_file)
{
    FcPattern	    *font;
    FILE	    *f;
    char	    *name;
    char	    *file, *base;
    int		    n;
    int		    id;
    FcBool	    ret;

    if (FcDebug () & FC_DBG_CACHE)
	printf ("FcFileCacheWriteDir cache_file \"%s\"\n", cache_file);
    
    f = fopen (cache_file, "w");
    if (!f)
    {
	if (FcDebug () & FC_DBG_CACHE)
	    printf (" can't create \"%s\"\n", cache_file);
	goto bail0;
    }
    for (n = 0; n < set->nfont; n++)
    {
	font = set->fonts[n];
	if (FcPatternGetString (font, FC_FILE, 0, &file) != FcResultMatch)
	    goto bail1;
	base = strrchr (file, '/');
	if (base)
	    base = base + 1;
	else
	    base = file;
	if (FcPatternGetInteger (font, FC_INDEX, 0, &id) != FcResultMatch)
	    goto bail1;
	if (FcDebug () & FC_DBG_CACHEV)
	    printf (" write file \"%s\"\n", base);
	if (!FcFileCacheWriteString (f, base))
	    goto bail1;
	if (putc (' ', f) == EOF)
	    goto bail1;
	if (!FcFileCacheWriteInt (f, id))
	    goto bail1;
        if (putc (' ', f) == EOF)
	    goto bail1;
	name = FcNameUnparse (font);
	if (!name)
	    goto bail1;
	ret = FcFileCacheWriteString (f, name);
	free (name);
	if (!ret)
	    goto bail1;
	if (putc ('\n', f) == EOF)
	    goto bail1;
    }
    if (fclose (f) == EOF)
	goto bail0;
    
    if (FcDebug () & FC_DBG_CACHE)
	printf (" cache written\n");
    return FcTrue;
    
bail1:
    fclose (f);
bail0:
    unlink (cache_file);
    return FcFalse;
}
