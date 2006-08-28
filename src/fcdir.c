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
    FcPattern		*font;
    FcBool		ret = FcTrue;
    int			count = 0;
    
    if (config && !FcConfigAcceptFilename (config, file))
	return FcTrue;

    if (FcFileIsDir (file))
	return FcStrSetAdd (dirs, file);

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
 * Strcmp helper that takes pointers to pointers, copied from qsort(3) manpage
 */

static int
cmpstringp(const void *p1, const void *p2)
{
    return strcmp(* (char **) p1, * (char **) p2);
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
    FcChar8		**dirlist;
    int			dirlistlen, dirlistalloc;
    FcChar8		*file;
    FcChar8		*base;
    FcBool		ret = FcTrue;
    FcFontSet		*tmpSet;
    int			i;

    if (config && !FcConfigAcceptFilename (config, dir))
	return FcTrue;

    if (!force)
    {
	/*
	 * Check ~/.fonts.cache-<version> file
	 */
	if (cache && FcGlobalCacheReadDir (set, dirs, cache, (char *)dir, config))
	    return FcTrue;

	if (FcDirCacheValid (dir, config) && 
	    FcDirCacheHasCurrentArch (dir, config) &&
	    FcDirCacheRead (set, dirs, dir, config))
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

    tmpSet = FcFontSetCreate();
    if (!tmpSet) 
    {
	ret = FcFalse;
	goto bail0;
    }

    dirlistlen = 0;
    dirlistalloc = 8;
    dirlist = malloc(dirlistalloc * sizeof(FcChar8 *));
    if (!dirlist) 
    {
	ret = FcFalse;
	goto bail1;
    }
    while ((e = readdir (d)))
    {
	if (e->d_name[0] != '.' && strlen (e->d_name) < FC_MAX_FILE_LEN)
	{
	    if (dirlistlen == dirlistalloc)
	    {
                FcChar8	**tmp_dirlist;

		dirlistalloc *= 2;
		tmp_dirlist = realloc(dirlist, 
                                      dirlistalloc * sizeof(FcChar8 *));
		if (!tmp_dirlist) 
                {
		    ret = FcFalse;
		    goto bail2;
		}
                dirlist = tmp_dirlist;
	    }
	    dirlist[dirlistlen] = malloc(strlen (e->d_name) + 1);
	    if (!dirlist[dirlistlen]) 
            {
		ret = FcFalse;
		goto bail2;
	    }
	    strcpy((char *)dirlist[dirlistlen], e->d_name);
	    dirlistlen++;
	}
    }
    qsort(dirlist, dirlistlen, sizeof(FcChar8 *), cmpstringp);
    i = 0;
    while (ret && i < dirlistlen)
    {
	strcpy ((char *) base, (char *) dirlist[i]);
	ret = FcFileScanConfig (tmpSet, dirs, cache, blanks, file, force, config);
	i++;
    }
    /*
     * Now that the directory has been scanned,
     * add the cache entry 
     */
    if (ret && cache)
	FcGlobalCacheUpdate (cache, dirs, (char *)dir, tmpSet, config);

    for (i = 0; i < tmpSet->nfont; i++)
	FcFontSetAdd (set, tmpSet->fonts[i]);

    if (tmpSet->fonts)
    {
	FcMemFree (FC_MEM_FONTPTR, tmpSet->sfont * sizeof (FcPattern *));
	free (tmpSet->fonts);
    }
    FcMemFree (FC_MEM_FONTSET, sizeof (FcFontSet));

 bail2:
    for (i = 0; i < dirlistlen; i++)
	free(dirlist[i]);

    free (dirlist);

 bail1:
    free (tmpSet);
    
 bail0:
    closedir (d);
    
    free (file);
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
FcDirSave (FcFontSet *set, FcStrSet * dirs, const FcChar8 *dir)
{
    return FcDirCacheWrite (set, dirs, dir, FcConfigGetCurrent ());
}
