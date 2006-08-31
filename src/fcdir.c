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
FcFileScanConfig (FcFontSet	*set,
		  FcStrSet	*dirs,
		  FcBlanks	*blanks,
		  const FcChar8	*file,
		  FcBool	force,
		  FcConfig	*config)
{
    if (config && !FcConfigAcceptFilename (config, file))
	return FcTrue;

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
    return FcFileScanConfig (set, dirs, blanks, file, force, 0);
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
		 FcBlanks	*blanks,
		 const FcChar8  *dir,
		 FcBool		force,
		 FcConfig	*config)
{
    DIR			*d;
    FcChar8		*canon_dir;
    struct dirent	*e;
    FcStrSet		*dirlist, *filelist;
    FcChar8		*file;
    FcChar8		*base;
    FcBool		ret = FcTrue;
    FcFontSet		*tmpSet;
    int			i;

    canon_dir = FcStrCanonFilename (dir);
    if (!canon_dir) canon_dir = (FcChar8 *) dir;
    
    if (config && !FcConfigAcceptFilename (config, canon_dir)) {
	ret = FcTrue;
	goto bail;
    }

    if (!force)
    {
	if (FcDirCacheRead (set, dirs, canon_dir, config)) {
	    ret = FcTrue;
	    goto bail;
	}
    }
    
    if (FcDebug () & FC_DBG_FONTSET)
    	printf ("cache scan dir %s\n", canon_dir);

    /* freed below */
    file = (FcChar8 *) malloc (strlen ((char *) canon_dir) + 1 + FC_MAX_FILE_LEN + 1);
    if (!file) {
	ret = FcFalse;
	goto bail;
    }

    strcpy ((char *) file, (char *) canon_dir);
    strcat ((char *) file, "/");
    base = file + strlen ((char *) file);
    
    if (FcDebug () & FC_DBG_SCAN)
	printf ("\tScanning dir %s\n", canon_dir);
	
    d = opendir ((char *) canon_dir);
    if (!d)
    {
	/* Don't complain about missing directories */
	if (errno == ENOENT)
	    ret = FcTrue;
	else
	    ret = FcFalse;
	goto bail_1;
    }

    tmpSet = FcFontSetCreate();
    if (!tmpSet) 
    {
	ret = FcFalse;
	goto bail0;
    }

    dirlist = FcStrSetCreate ();
    if (!dirlist) 
    {
	ret = FcFalse;
	goto bail1;
    }
    filelist = FcStrSetCreate ();
    if (!filelist)
    {
	ret = FcFalse;
	goto bail2;
    }
    while ((e = readdir (d)))
    {
	if (e->d_name[0] != '.' && strlen (e->d_name) < FC_MAX_FILE_LEN)
	{
	    strcpy ((char *) base, (char *) e->d_name);
	    if (FcFileIsDir (file)) {
		if (!FcStrSetAdd (dirlist, file)) {
		    ret = FcFalse;
		    goto bail3;
		}
	    } else {
		if (!FcStrSetAdd (filelist, file)) {
		    ret = FcFalse;
		    goto bail3;
		}
	    }
	}
    }
    /*
     * Sort files and dirs to make things prettier
     */
    qsort(dirlist->strs, dirlist->num, sizeof(FcChar8 *), cmpstringp);
    qsort(filelist->strs, filelist->num, sizeof(FcChar8 *), cmpstringp);
    
    for (i = 0; i < filelist->num; i++)
	FcFileScanFontConfig (tmpSet, blanks, filelist->strs[i], config);
    
    /*
     * Now that the directory has been scanned,
     * write out the cache file
     */
    FcDirCacheWrite (tmpSet, dirlist, canon_dir, config);

    /*
     * Add the discovered fonts to our internal non-cache list
     */
    for (i = 0; i < tmpSet->nfont; i++)
	FcFontSetAdd (set, tmpSet->fonts[i]);

    /*
     * the patterns in tmpset now belong to set; don't free them 
     */
    tmpSet->nfont = 0;

    /*
     * Add the discovered directories to the list to be scanned
     */
    for (i = 0; i < dirlist->num; i++)
	if (!FcStrSetAdd (dirs, dirlist->strs[i])) {
	    ret = FcFalse;
	    goto bail3;
	}
    
 bail3:
    FcStrSetDestroy (filelist);
 bail2:
    FcStrSetDestroy (dirlist);
 bail1:
    FcFontSetDestroy (tmpSet);
    
 bail0:
    closedir (d);
    
 bail_1:
    free (file);
 bail:
    if (canon_dir != dir) free (canon_dir);
    return ret;
}

FcBool
FcDirScan (FcFontSet	    *set,
	   FcStrSet	    *dirs,
	   FcFileCache	    *cache, /* XXX unused */
	   FcBlanks	    *blanks,
	   const FcChar8    *dir,
	   FcBool	    force)
{
    return FcDirScanConfig (set, dirs, blanks, dir, force, 0);
}

FcBool
FcDirSave (FcFontSet *set, FcStrSet * dirs, const FcChar8 *dir)
{
    return FcFalse;
}
