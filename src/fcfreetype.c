/*
 * $XFree86: xc/lib/fontconfig/src/fcfreetype.c,v 1.7 2002/07/08 07:31:53 keithp Exp $
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

/*
 * Keep Han languages separated by eliminating languages
 * that the codePageRange bits says aren't supported
 */

static const struct {
    int		    bit;
    const FcChar8   *lang;
} FcCodePageRange[] = {
    { 17,	(const FcChar8 *) "ja" },
    { 18,	(const FcChar8 *) "zh-cn" },
    { 19,	(const FcChar8 *) "ko" },
    { 20,	(const FcChar8 *) "zh-tw" },
};

#define NUM_CODE_PAGE_RANGE (sizeof FcCodePageRange / sizeof FcCodePageRange[0])

FcBool
FcFreeTypeIsExclusiveLang (const FcChar8  *lang)
{
    int	    i;

    for (i = 0; i < NUM_CODE_PAGE_RANGE; i++)
    {
	if (FcLangCompare (lang, FcCodePageRange[i].lang) != FcLangDifferentLang)
	    return FcTrue;
    }
    return FcFalse;
}

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
    const FcChar8   *exclusiveLang = 0;

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
		/* 
		 * If the font advertises support for multiple
		 * "exclusive" languages, then include support
		 * for any language found to have coverage
		 */
		if (exclusiveLang)
		{
		    exclusiveLang = 0;
		    break;
		}
		exclusiveLang = FcCodePageRange[i].lang;
	    }
	}
    }

    /*
     * Compute the unicode coverage for the font
     */
    cs = FcFreeTypeCharSet (face, blanks);
    if (!cs)
	goto bail1;

    /*
     * Skip over PCF fonts that have no encoded characters; they're
     * usually just Unicode fonts transcoded to some legacy encoding
     */
    if (FcCharSetCount (cs) == 0)
    {
	if (!strcmp(FT_MODULE_CLASS(&face->driver->root)->module_name, "pcf"))
	    goto bail2;
    }

    if (!FcPatternAddCharSet (pat, FC_CHARSET, cs))
	goto bail2;

    if (!FcFreeTypeSetLang (pat, cs, exclusiveLang))
        goto bail2;

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
