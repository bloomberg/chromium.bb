/*
 * $XFree86: xc/lib/fontconfig/src/fcfreetype.c,v 1.4 2002/05/31 23:21:25 keithp Exp $
 *
 * Copyright © 2001 Keith Packard, member of The XFree86 Project, Inc.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fcint.h"
#include <freetype/freetype.h>
#include <freetype/internal/ftobjs.h>
#include <freetype/tttables.h>
#include "fcknownsets.h"

static const struct {
    int	    bit;
    FcChar8 *name;
} FcCodePageRange[] = {
    { 0,	(FcChar8 *) FC_LANG_LATIN_1 },
    { 1,	(FcChar8 *) FC_LANG_LATIN_2 },
    { 2,	(FcChar8 *) FC_LANG_CYRILLIC },
    { 3,	(FcChar8 *) FC_LANG_GREEK },
    { 4,	(FcChar8 *) FC_LANG_TURKISH },
    { 5,	(FcChar8 *) FC_LANG_HEBREW },
    { 6,	(FcChar8 *) FC_LANG_ARABIC },
    { 7,	(FcChar8 *) FC_LANG_WINDOWS_BALTIC },
    { 8,	(FcChar8 *) FC_LANG_VIETNAMESE },
/* 9-15 reserved for Alternate ANSI */
    { 16,	(FcChar8 *) FC_LANG_THAI },
    { 17,	(FcChar8 *) FC_LANG_JAPANESE },
    { 18,	(FcChar8 *) FC_LANG_SIMPLIFIED_CHINESE },
    { 19,	(FcChar8 *) FC_LANG_KOREAN_WANSUNG },
    { 20,	(FcChar8 *) FC_LANG_TRADITIONAL_CHINESE },
    { 21,	(FcChar8 *) FC_LANG_KOREAN_JOHAB },
/* 22-28 reserved for Alternate ANSI & OEM */
    { 29,	(FcChar8 *) FC_LANG_MACINTOSH },
    { 30,	(FcChar8 *) FC_LANG_OEM },
    { 31,	(FcChar8 *) FC_LANG_SYMBOL },
/* 32-47 reserved for OEM */
    { 48,	(FcChar8 *) FC_LANG_IBM_GREEK },
    { 49,	(FcChar8 *) FC_LANG_MSDOS_RUSSIAN },
    { 50,	(FcChar8 *) FC_LANG_MSDOS_NORDIC },
    { 51,	(FcChar8 *) FC_LANG_ARABIC_864 },
    { 52,	(FcChar8 *) FC_LANG_MSDOS_CANADIAN_FRENCH },
    { 53,	(FcChar8 *) FC_LANG_HEBREW_862 },
    { 54,	(FcChar8 *) FC_LANG_MSDOS_ICELANDIC },
    { 55,	(FcChar8 *) FC_LANG_MSDOS_PORTUGUESE },
    { 56,	(FcChar8 *) FC_LANG_IBM_TURKISH },
    { 57,	(FcChar8 *) FC_LANG_IBM_CYRILLIC },
    { 58,	(FcChar8 *) FC_LANG_LATIN_2 },
    { 59,	(FcChar8 *) FC_LANG_MSDOS_BALTIC },
    { 60,	(FcChar8 *) FC_LANG_GREEK_437_G },
    { 61,	(FcChar8 *) FC_LANG_ARABIC_ASMO_708 },
    { 62,	(FcChar8 *) FC_LANG_WE_LATIN_1 },
    { 63,	(FcChar8 *) FC_LANG_US },
};

#define NUM_CODE_PAGE_RANGE (sizeof FcCodePageRange / sizeof FcCodePageRange[0])

static const struct {
    const FcCharSet *set;
    FcChar32	    size;
    FcChar32	    missing_tolerance;
    FcChar8	    *name;
} FcCodePageSet[] = {
    {
	&fcCharSet_Latin1_1252,
	fcCharSet_Latin1_1252_size,
	8,
	(FcChar8 *) FC_LANG_LATIN_1,
    },
    {
	&fcCharSet_Latin2_1250,
	fcCharSet_Latin2_1250_size,
	8,
	(FcChar8 *) FC_LANG_LATIN_2,
    },
    {
	&fcCharSet_Cyrillic_1251,
	fcCharSet_Cyrillic_1251_size,
	16,
	(FcChar8 *) FC_LANG_CYRILLIC,
    },
    {
	&fcCharSet_Greek_1253,
	fcCharSet_Greek_1253_size,
	8,
	(FcChar8 *) FC_LANG_GREEK,
    },
    {
	&fcCharSet_Turkish_1254,
	fcCharSet_Turkish_1254_size,
	16,
	(FcChar8 *) FC_LANG_TURKISH,
    },
    {
	&fcCharSet_Hebrew_1255,
	fcCharSet_Hebrew_1255_size,
	16,
	(FcChar8 *) FC_LANG_HEBREW,
    },
    {
	&fcCharSet_Arabic_1256,
	fcCharSet_Arabic_1256_size,
	16,
	(FcChar8 *) FC_LANG_ARABIC,
    },
    {
	&fcCharSet_Windows_Baltic_1257,
	fcCharSet_Windows_Baltic_1257_size,
	16,
	(FcChar8 *) FC_LANG_WINDOWS_BALTIC,
    },
    {	
	&fcCharSet_Thai_874,		
	fcCharSet_Thai_874_size,
	16,
	(FcChar8 *) FC_LANG_THAI,
    },
    { 
	&fcCharSet_Japanese_932,		
	fcCharSet_Japanese_932_size,		
	500,
	(FcChar8 *) FC_LANG_JAPANESE,
    },
    { 
	&fcCharSet_SimplifiedChinese_936,
	fcCharSet_SimplifiedChinese_936_size,	
	500,
	(FcChar8 *) FC_LANG_SIMPLIFIED_CHINESE,
    },
    {
	&fcCharSet_Korean_949,
	fcCharSet_Korean_949_size,
	500,
	(FcChar8 *) FC_LANG_KOREAN_WANSUNG,
    },
    {
	&fcCharSet_TraditionalChinese_950,
	fcCharSet_TraditionalChinese_950_size,
	500,
	(FcChar8 *) FC_LANG_TRADITIONAL_CHINESE,
    },
};

#define NUM_CODE_PAGE_SET (sizeof FcCodePageSet / sizeof FcCodePageSet[0])

FcPattern *
FcFreeTypeQuery (const FcChar8	*file,
		 int		id,
		 FcBlanks	*blanks,
		 int		*count)
{
    FT_Face	    face;
    FcPattern	    *pat;
    int		    slant;
    int		    weight;
    int		    i;
    FcCharSet	    *cs;
    FT_Library	    ftLibrary;
    const FcChar8   *family;
    TT_OS2	    *os2;
    FcBool	    hasLang = FcFalse;
    FcChar32	    codepoints;
    FcBool	    matchCodePage[NUM_CODE_PAGE_SET];

    if (FT_Init_FreeType (&ftLibrary))
	return 0;
    
    if (FT_New_Face (ftLibrary, (char *) file, id, &face))
	goto bail;

    *count = face->num_faces;

    pat = FcPatternCreate ();
    if (!pat)
	goto bail0;

    if (!FcPatternAddBool (pat, FC_OUTLINE,
			   (face->face_flags & FT_FACE_FLAG_SCALABLE) != 0))
	goto bail1;

    if (!FcPatternAddBool (pat, FC_SCALABLE,
			   (face->face_flags & FT_FACE_FLAG_SCALABLE) != 0))
	goto bail1;


    slant = FC_SLANT_ROMAN;
    if (face->style_flags & FT_STYLE_FLAG_ITALIC)
	slant = FC_SLANT_ITALIC;

    if (!FcPatternAddInteger (pat, FC_SLANT, slant))
	goto bail1;

    weight = FC_WEIGHT_MEDIUM;
    if (face->style_flags & FT_STYLE_FLAG_BOLD)
	weight = FC_WEIGHT_BOLD;

    if (!FcPatternAddInteger (pat, FC_WEIGHT, weight))
	goto bail1;

    family = (FcChar8 *) face->family_name;
    if (!family)
    {
	family = (FcChar8 *) strrchr ((char *) file, '/');
	if (family)
	    family++;
	else
	    family = file;
    }
    if (!FcPatternAddString (pat, FC_FAMILY, family))
	goto bail1;

    if (face->style_name)
    {
	if (!FcPatternAddString (pat, FC_STYLE, (FcChar8 *) face->style_name))
	    goto bail1;
    }

    if (!FcPatternAddString (pat, FC_FILE, file))
	goto bail1;

    if (!FcPatternAddInteger (pat, FC_INDEX, id))
	goto bail1;

    if (!FcPatternAddString (pat, FC_SOURCE, (FcChar8 *) "FreeType"))
	goto bail1;

#if 1
    if ((face->face_flags & FT_FACE_FLAG_FIXED_WIDTH) != 0)
	if (!FcPatternAddInteger (pat, FC_SPACING, FC_MONO))
	    goto bail1;
#endif

    /*
     * Get the OS/2 table and poke about
     */
    os2 = (TT_OS2 *) FT_Get_Sfnt_Table (face, ft_sfnt_os2);
    if (os2 && os2->version >= 0x0001 && os2->version != 0xffff)
    {
	for (i = 0; i < NUM_CODE_PAGE_RANGE; i++)
	{
	    FT_ULong	bits;
	    int		bit;
	    if (FcCodePageRange[i].bit < 32)
	    {
		bits = os2->ulCodePageRange1;
		bit = FcCodePageRange[i].bit;
	    }
	    else
	    {
		bits = os2->ulCodePageRange2;
		bit = FcCodePageRange[i].bit - 32;
	    }
	    if (bits & (1 << bit))
	    {
		if (!FcPatternAddString (pat, FC_LANG,
					 FcCodePageRange[i].name))
		    goto bail1;
		hasLang = FcTrue;
	    }
	}
    }

    /*
     * Compute the unicode coverage for the font
     */
    cs = FcFreeTypeCharSet (face, blanks);
    if (!cs)
	goto bail1;

    codepoints = FcCharSetCount (cs);
    /*
     * Skip over PCF fonts that have no encoded characters; they're
     * usually just Unicode fonts transcoded to some legacy encoding
     */
    if (codepoints == 0)
    {
	if (!strcmp(FT_MODULE_CLASS(&face->driver->root)->module_name, "pcf"))
	    goto bail2;
    }

    if (!FcPatternAddCharSet (pat, FC_CHARSET, cs))
	goto bail2;

    if (!hasLang || (FcDebug() & FC_DBG_SCANV))
    {
	/*
	 * Use the Unicode coverage to set lang if it wasn't
	 * set from the OS/2 tables
	 */

        if (FcDebug() & FC_DBG_SCANV)
	    printf ("%s: ", family);
	for (i = 0; i < NUM_CODE_PAGE_SET; i++)
	{
	    FcChar32    missing;

	    missing = FcCharSetSubtractCount (FcCodePageSet[i].set,
					      cs);
	    matchCodePage[i] = missing <= FcCodePageSet[i].missing_tolerance;
	    if (FcDebug() & FC_DBG_SCANV)
		printf ("%s(%d) ", FcCodePageSet[i].name, missing);
	}
        if (FcDebug() & FC_DBG_SCANV)
	    printf ("\n");

	if (hasLang)
	{
	    FcChar8	*lang;
	    int	j;
	    /*
	     * Validate the lang selections
	     */
	    for (i = 0; FcPatternGetString (pat, FC_LANG, i, &lang) == FcResultMatch; i++)
	    {
		for (j = 0; j < NUM_CODE_PAGE_SET; j++)
		    if (!strcmp ((char *) FcCodePageSet[j].name,
				 (char *) lang))
		    {
			if (!matchCodePage[j])
			    printf ("%s(%s): missing lang %s\n", file, family, lang);
		    }
	    }
	    for (j = 0; j < NUM_CODE_PAGE_SET; j++)
	    {
		if (!matchCodePage[j])
		    continue;
		lang = 0;
		for (i = 0; FcPatternGetString (pat, FC_LANG, i, &lang) == FcResultMatch; i++)
		{
		    if (!strcmp ((char *) FcCodePageSet[j].name, (char *) lang))
			break;
		    lang = 0;
		}
		if (!lang)
		    printf ("%s(%s): extra lang %s\n", file, family, FcCodePageSet[j].name);
	    }
	}
	else
	{
	    /*
	     * None provided, use the charset derived ones
	     */
	    for (i = 0; i < NUM_CODE_PAGE_SET; i++)
		if (matchCodePage[i])
		{
		    if (!FcPatternAddString (pat, FC_LANG,
					     FcCodePageRange[i].name))
			goto bail1;
		    hasLang = TRUE;
		}
	}
    }
    if (!hasLang)
	if (!FcPatternAddString (pat, FC_LANG, (FcChar8 *) FC_LANG_UNKNOWN))
	    goto bail1;
     
    /*
     * Drop our reference to the charset
     */
    FcCharSetDestroy (cs);
    
    if (!(face->face_flags & FT_FACE_FLAG_SCALABLE))
    {
	for (i = 0; i < face->num_fixed_sizes; i++)
	    if (!FcPatternAddDouble (pat, FC_PIXEL_SIZE,
				     (double) face->available_sizes[i].height))
		goto bail1;
	if (!FcPatternAddBool (pat, FC_ANTIALIAS, FcFalse))
	    goto bail1;
    }

    FT_Done_Face (face);
    FT_Done_FreeType (ftLibrary);
    return pat;

bail2:
    FcCharSetDestroy (cs);
bail1:
    FcPatternDestroy (pat);
bail0:
    FT_Done_Face (face);
bail:
    FT_Done_FreeType (ftLibrary);
    return 0;
}
