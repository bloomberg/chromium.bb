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

FcBool
FcFileScanConfig (FcFontSet	*set,
		  FcStrSet	*dirs,
		  FcGlobalCache *cache,
		  FcBlanks	*blanks,
		  const FcChar8	*file,
		  FcBool	force,
		  FcConfig	*config)
{
    int			id;
    FcChar8		*name;
    FcPattern		*font;
    FcBool		ret = FcTrue;
    FcBool		isDir;
    int			count = 0;
    FcGlobalCacheFile	*cache_file;
    FcGlobalCacheDir	*cache_dir;
    FcBool		need_scan;
    
    if (config && !FcConfigAcceptFilename (config, file))
	return FcTrue;

    if (force)
	cache = 0;
    id = 0;
    do
    {
	need_scan = FcTrue;
	font = 0;
	/*
	 * Check the cache
	 */
	if (cache)
	{
	    if ((cache_file = FcGlobalCacheFileGet (cache, file, id, &count)))
	    {
		/*
		 * Found a cache entry for the file
		 */
		if (FcGlobalCacheCheckTime (file, &cache_file->info))
		{
		    name = cache_file->name;
		    need_scan = FcFalse;
		    FcGlobalCacheReferenced (cache, &cache_file->info);
		    /* "." means the file doesn't contain a font */
		    if (FcStrCmp (name, FC_FONT_FILE_INVALID) != 0)
		    {
			font = FcNameParse (name);
			if (font)
			    if (!FcPatternAddString (font, FC_FILE, file))
				ret = FcFalse;
		    }
		}
	    }
	    else if ((cache_dir = FcGlobalCacheDirGet (cache, file,
						       strlen ((const char *) file),
						       FcFalse)))
	    {
		if (FcGlobalCacheCheckTime (cache_dir->info.file, 
					    &cache_dir->info))
		{
		    font = 0;
		    need_scan = FcFalse;
		    FcGlobalCacheReferenced (cache, &cache_dir->info);
		    if (!FcStrSetAdd (dirs, file))
			ret = FcFalse;
		}
	    }
	}
	/*
	 * Nothing in the cache, scan the file
	 */
	if (need_scan)
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
	    /*
	     * Update the cache
	     */
	    if (cache && font)
	    {
		FcChar8	*unparse;

		unparse = FcNameUnparse (font);
		if (unparse)
		{
		    (void) FcGlobalCacheUpdate (cache, file, id, unparse);
		    FcStrFree (unparse);
		}
	    }
	}
	/*
	 * Add the font
	 */
	if (font && (!config || FcConfigAcceptFont (config, font)))
	{
	    if (!FcFontSetAdd (set, font))
	    {
		FcPatternDestroy (font);
		font = 0;
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
FcFileScan (FcFontSet	    *set,
	    FcStrSet	    *dirs,
	    FcGlobalCache   *cache,
	    FcBlanks	    *blanks,
	    const FcChar8   *file,
	    FcBool	    force)
{
    return FcFileScanConfig (set, dirs, cache, blanks, file, force, 0);
}

/*
 * Scan 'dir', adding font files to 'set' and
 * subdirectories to 'dirs'
 */

FcBool
FcDirScanConfig (FcFontSet	*set,
		 FcStrSet	*dirs,
		 FcGlobalCache  *cache,
		 FcBlanks	*blanks,
		 const FcChar8  *dir,
		 FcBool		force,
		 FcConfig	*config)
{
    DIR			*d;
    struct dirent	*e;
    FcChar8		*file;
    FcChar8		*base;
    FcBool		ret = FcTrue;

    if (config && !FcConfigAcceptFilename (config, dir))
	return FcTrue;

    if (!force)
    {
	/*
	 * Check fonts.cache-<version> file
	 */
	if (FcDirCacheReadDir (set, dirs, dir, config))
	{
	    if (cache)
		FcGlobalCacheReferenceSubdir (cache, dir);
	    return FcTrue;
	}
    
	/*
	 * Check ~/.fonts.cache-<version> file
	 */
	if (cache && FcGlobalCacheScanDir (set, dirs, cache, dir, config))
	    return FcTrue;
    }
    
    /* freed below */
    file = (FcChar8 *) malloc (strlen ((char *) dir) + 1 + FC_MAX_FILE_LEN + 1);
    if (!file)
	return FcFalse;

    strcpy ((char *) file, (char *) dir);
    strcat ((char *) file, "/");
    base = file + strlen ((char *) file);
    
    if (FcDebug () & FC_DBG_SCAN)
	printf ("\tScanning dir %s\n", dir);
	
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
	    ret = FcFileScanConfig (set, dirs, cache, blanks, file, force, config);
	}
    }
    free (file);
    closedir (d);
    /*
     * Now that the directory has been scanned,
     * add the cache entry 
     */
    if (ret && cache)
	FcGlobalCacheUpdate (cache, dir, 0, 0);
	
    return ret;
}

FcBool
FcDirScan (FcFontSet	    *set,
	   FcStrSet	    *dirs,
	   FcGlobalCache    *cache,
	   FcBlanks	    *blanks,
	   const FcChar8    *dir,
	   FcBool	    force)
{
    return FcDirScanConfig (set, dirs, cache, blanks, dir, force, 0);
}

FcBool
FcDirSave (FcFontSet *set, FcStrSet *dirs, const FcChar8 *dir)
{
    return FcDirCacheWriteDir (set, dirs, dir);
}
