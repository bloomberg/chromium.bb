/*
 * $XFree86: xc/lib/fontconfig/src/fcdir.c,v 1.2 2002/02/15 06:01:28 keithp Exp $
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
#include <dirent.h>

static FcBool
FcFileIsDir (const FcChar8 *file)
{
    struct stat	    statb;

    if (stat ((const char *) file, &statb) != 0)
	return FcFalse;
    return S_ISDIR(statb.st_mode);
}

FcBool
FcFileScan (FcFontSet	    *set,
	    FcStrSet	    *dirs,
	    FcFileCache	    *cache,
	    FcBlanks	    *blanks,
	    const FcChar8   *file,
	    FcBool	    force)
{
    int		    id;
    FcChar8	    *name;
    FcPattern	    *font;
    FcBool	    ret = FcTrue;
    FcBool	    isDir;
    int		    count;
    
    id = 0;
    do
    {
	if (!force && cache)
	    name = FcFileCacheFind (cache, file, id, &count);
	else
	    name = 0;
	if (name)
	{
	    /* "." means the file doesn't contain a font */
	    if (FcStrCmp (name, FC_FONT_FILE_INVALID) == 0)
		font = 0;
	    else if (FcStrCmp (name, FC_FONT_FILE_DIR) == 0)
	    {
		ret = FcStrSetAdd (dirs, file);
		font = 0;
	    }
	    else
	    {
		font = FcNameParse (name);
		if (font)
		    FcPatternAddString (font, FC_FILE, file);
	    }
	}
	else
	{
	    if (FcDebug () & FC_DBG_SCAN)
	    {
		printf ("\tScanning file %s...", file);
		fflush (stdout);
	    }
	    font = FcFreeTypeQuery (file, id, blanks, &count);
	    if (FcDebug () & FC_DBG_SCAN)
		printf ("done\n");
	    isDir = FcFalse;
	    if (!font && FcFileIsDir (file))
	    {
		isDir = FcTrue;
		ret = FcStrSetAdd (dirs, file);
	    }
	    if (!force && cache)
	    {
		if (font)
		{
		    FcChar8	*unparse;

		    unparse = FcNameUnparse (font);
		    if (unparse)
		    {
			(void) FcFileCacheUpdate (cache, file, id, unparse);
			free (unparse);
		    }
		}
		else
		{
		    if (isDir)
		    {
			FcFileCacheUpdate (cache, file, id, (FcChar8 *) 
					   FC_FONT_FILE_DIR);
		    }
		    else
		    {
			/* negative cache files not containing fonts */
			FcFileCacheUpdate (cache, file, id, (FcChar8 *) 
					   FC_FONT_FILE_INVALID);
		    }
		}
	    }
	}
	if (font)
	{
	    if (!FcFontSetAdd (set, font))
	    {
		FcPatternDestroy (font);
		font = 0;
		ret = FcFalse;
	    }
	}
	id++;
    } while (font && ret && id < count);
    return ret;
}

FcBool
FcDirCacheValid (const FcChar8 *dir)
{
    FcChar8 *path;
    FcBool  ret;

    path = (FcChar8 *) malloc (strlen ((const char *) dir) + 1 + 
			       strlen ((const char *) FC_DIR_CACHE_FILE) + 1);
    if (!path)
	return FcFalse;
    strcpy ((char *) path, (const char *) dir);
    strcat ((char *) path, (const char *) "/");
    strcat ((char *) path, (const char *) FC_DIR_CACHE_FILE);
    ret = FcFileCacheValid (path);
    free (path);
    return ret;
}

#define FC_MAX_FILE_LEN	    4096

FcBool
FcDirScan (FcFontSet	    *set,
	   FcStrSet	    *dirs,
	   FcFileCache	    *cache,
	   FcBlanks	    *blanks,
	   const FcChar8    *dir,
	   FcBool	    force)
{
    DIR		    *d;
    struct dirent   *e;
    FcChar8	    *file;
    FcChar8	    *base;
    FcBool	    ret = FcTrue;

    file = (FcChar8 *) malloc (strlen ((char *) dir) + 1 + FC_MAX_FILE_LEN + 1);
    if (!file)
	return FcFalse;

    strcpy ((char *) file, (char *) dir);
    strcat ((char *) file, "/");
    base = file + strlen ((char *) file);
    if (!force)
    {
	strcpy ((char *) base, FC_DIR_CACHE_FILE);
	
	if (FcFileCacheReadDir (set, dirs, file))
	{
	    free (file);
	    return FcTrue;
	}
    }
    
    d = opendir ((char *) dir);
    if (!d)
    {
	free (file);
	/* Don't complain about missing directories */
	if (errno == ENOENT)
	    return FcTrue;
	return FcFalse;
    }
    while (ret && (e = readdir (d)))
    {
	if (e->d_name[0] != '.' && strlen (e->d_name) < FC_MAX_FILE_LEN)
	{
	    strcpy ((char *) base, (char *) e->d_name);
	    ret = FcFileScan (set, dirs, cache, blanks, file, force);
	}
    }
    free (file);
    closedir (d);
    return ret;
}

FcBool
FcDirSave (FcFontSet *set, FcStrSet *dirs, const FcChar8 *dir)
{
    FcChar8	    *file;
    FcChar8	    *base;
    FcBool	    ret;
    
    file = (FcChar8 *) malloc (strlen ((char *) dir) + 1 + 256 + 1);
    if (!file)
	return FcFalse;

    strcpy ((char *) file, (char *) dir);
    strcat ((char *) file, "/");
    base = file + strlen ((char *) file);
    strcpy ((char *) base, FC_DIR_CACHE_FILE);
    ret = FcFileCacheWriteDir (set, dirs, file);
    free (file);
    return ret;
}
