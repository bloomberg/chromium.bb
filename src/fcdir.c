/*
 * $RCSId: xc/lib/fontconfig/src/fcdir.c,v 1.9 2002/08/31 22:17:32 keithp Exp $
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
#include <dirent.h>

FcBool
FcFileIsDir (const FcChar8 *file)
{
    struct stat	    statb;

    if (stat ((const char *) file, &statb) != 0)
	return FcFalse;
    return S_ISDIR(statb.st_mode);
}

static FcBool
FcFileScanFontConfig (FcFontSet		*set,
		      FcBlanks		*blanks,
		      const FcChar8	*file,
		      FcConfig		*config)
{
    FcPattern	*font;
    FcBool	ret = FcTrue;
    int		id;
    int		count = 0;
    
    id = 0;
    do
    {
	font = 0;
	/*
	 * Nothing in the cache, scan the file
	 */
	if (FcDebug () & FC_DBG_SCAN)
	{
	    printf ("\tScanning file %s...", file);
	    fflush (stdout);
	}
	font = FcFreeTypeQuery (file, id, blanks, &count);
	if (FcDebug () & FC_DBG_SCAN)
	    printf ("done\n");

	/*
	 * Edit pattern with user-defined rules
	 */
	if (font && config && !FcConfigSubstituteWithPat (config, font, NULL, FcMatchScan))
	{
	    FcPatternDestroy (font);
	    font = NULL;
	    ret = FcFalse;
	}

	/*
	 * Add the font
	 */
	if (font && (!config || FcConfigAcceptFont (config, font)))
	{
	    if (FcDebug() & FC_DBG_SCANV)
	    {
		printf ("Final font pattern:\n");
		FcPatternPrint (font);
	    }
	    if (!FcFontSetAdd (set, font))
	    {
		FcPatternDestroy (font);
		font = NULL;
		ret = FcFalse;
	    }
	}
	else if (font)
	    FcPatternDestroy (font);
	id++;
    } while (font && ret && id < count);
    return ret;
}

FcBool
FcFileScanConfig (FcFontSet	*set,
		  FcStrSet	*dirs,
		  FcBlanks	*blanks,
		  const FcChar8	*file,
		  FcConfig	*config)
{
    if (FcFileIsDir (file))
	return FcStrSetAdd (dirs, file);
    else
	return FcFileScanFontConfig (set, blanks, file, config);
}

FcBool
FcFileScan (FcFontSet	    *set,
	    FcStrSet	    *dirs,
	    FcFileCache	    *cache, /* XXX unused */
	    FcBlanks	    *blanks,
	    const FcChar8   *file,
	    FcBool	    force)
{
    return FcFileScanConfig (set, dirs, blanks, file, NULL);
}

/*
 * Strcmp helper that takes pointers to pointers, copied from qsort(3) manpage
 */
static int
cmpstringp(const void *p1, const void *p2)
{
    return strcmp(* (char **) p1, * (char **) p2);
}

/*
 * Scan the specified directory and construct a cache of its contents
 */
FcCache *
FcDirCacheScan (const FcChar8 *dir, FcConfig *config)
{
    DIR			*d;
    struct dirent	*e;
    FcStrSet		*files;
    FcStrSet		*dirs;
    FcChar8		*file;
    FcChar8		*base;
    FcBool		ret = FcTrue;
    FcFontSet		*set;
    int			i;
    FcBlanks		*blanks = FcConfigGetBlanks (config);
    FcCache		*cache = NULL;
    struct stat		dir_stat;

    if (FcDebug () & FC_DBG_FONTSET)
    	printf ("cache scan dir %s\n", dir);

    /* freed below */
    file = (FcChar8 *) malloc (strlen ((char *) dir) + 1 + FC_MAX_FILE_LEN + 1);
    if (!file) {
	ret = FcFalse;
	goto bail;
    }

    strcpy ((char *) file, (char *) dir);
    strcat ((char *) file, "/");
    base = file + strlen ((char *) file);
    
    if (FcDebug () & FC_DBG_SCAN)
	printf ("\tScanning dir %s\n", dir);
	
    d = opendir ((char *) dir);
    if (!d)
    {
	/* Don't complain about missing directories */
	if (errno == ENOENT)
	    ret = FcTrue;
	else
	    ret = FcFalse;
	goto bail_1;
    }
    if (stat ((char *) dir, &dir_stat) < 0)
    {
	ret = FcFalse;
	goto bail_1;
    }

    set = FcFontSetCreate();
    if (!set) 
    {
	ret = FcFalse;
	goto bail0;
    }

    files = FcStrSetCreate ();
    if (!files)
    {
	ret = FcFalse;
	goto bail1;
    }
    while ((e = readdir (d)))
    {
	if (e->d_name[0] != '.' && strlen (e->d_name) < FC_MAX_FILE_LEN)
	{
	    strcpy ((char *) base, (char *) e->d_name);
	    if (!FcStrSetAdd (files, file)) {
		ret = FcFalse;
		goto bail2;
	    }
	}
    }

    /*
     * Sort files to make things prettier
     */
    qsort(files->strs, files->num, sizeof(FcChar8 *), cmpstringp);
    
    dirs = FcStrSetCreate ();
    if (!dirs)
	goto bail2;
    
    /*
     * Scan file files to build font patterns
     */
    for (i = 0; i < files->num; i++)
	FcFileScanConfig (set, dirs, blanks, files->strs[i], config);
    
    /*
     * Build the cache object
     */
    cache = FcDirCacheBuild (set, dir, &dir_stat, dirs);
    if (!cache)
	goto bail3;
    
    /*
     * Write out the cache file, ignoring any troubles
     */
    FcDirCacheWrite (cache, config);
    
 bail3:
    FcStrSetDestroy (dirs);
 bail2:
    FcStrSetDestroy (files);
 bail1:
    FcFontSetDestroy (set);
    
 bail0:
    closedir (d);
    
 bail_1:
    free (file);
 bail:
    return cache;
}

/*
 * Read (or construct) the cache for a directory
 */
FcCache *
FcDirCacheRead (const FcChar8 *dir, FcBool force, FcConfig *config)
{
    FcCache		*cache = NULL;

    if (config && !FcConfigAcceptFilename (config, dir))
	return NULL;

    /* Try to use existing cache file */
    if (!force)
	cache = FcDirCacheLoad (dir, config, NULL);
    
    /* Not using existing cache file, construct new cache */
    if (!cache)
	cache = FcDirCacheScan (dir, config);
    
    return cache;
}

FcBool
FcDirScanConfig (FcFontSet	*set,
		 FcStrSet	*dirs,
		 FcBlanks	*blanks,
		 const FcChar8	*dir,
		 FcBool		force,
		 FcConfig	*config)
{
    return FcFalse; /* XXX deprecated */
}

FcBool
FcDirScan (FcFontSet	    *set,
	   FcStrSet	    *dirs,
	   FcFileCache	    *cache, /* XXX unused */
	   FcBlanks	    *blanks,
	   const FcChar8    *dir,
	   FcBool	    force)
{
    return FcFalse; /* XXX deprecated */
}

FcBool
FcDirSave (FcFontSet *set, FcStrSet * dirs, const FcChar8 *dir)
{
    return FcFalse; /* XXX deprecated */
}
#define __fcdir__
#include "fcaliastail.h"
#undef __fcdir__
