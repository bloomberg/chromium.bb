/*
 * $XFree86: xc/lib/fontconfig/src/fcfreetype.c,v 1.5 2002/06/29 20:31:02 keithp Exp $
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

static const FcChar8	*fcLangLatin1[] = {
    (FcChar8 *) "br",   /* Breton */
    (FcChar8 *) "ca",   /* Catalan */
    (FcChar8 *) "da",   /* Danish */
    (FcChar8 *) "de",   /* German */
    (FcChar8 *) "en",   /* English */
    (FcChar8 *) "es",   /* Spanish */
    (FcChar8 *) "eu",   /* Basque */
    (FcChar8 *) "fi",   /* Finnish */
    (FcChar8 *) "fo",   /* Faroese */
    (FcChar8 *) "fr",   /* French */
    (FcChar8 *) "ga",   /* Irish */
    (FcChar8 *) "gd",   /* Scottish */
    (FcChar8 *) "gl",   /* Galician */
    (FcChar8 *) "is",   /* Islandic */
    (FcChar8 *) "it",   /* Italian */
    (FcChar8 *) "kl",   /* Greenlandic */
    (FcChar8 *) "la",   /* Latin */
    (FcChar8 *) "nl",   /* Dutch */
    (FcChar8 *) "no",   /* Norwegian */
    (FcChar8 *) "pt",   /* Portuguese */
    (FcChar8 *) "rm",   /* Rhaeto-Romanic */
    (FcChar8 *) "sq",   /* Albanian */
    (FcChar8 *) "sv",   /* Swedish */
    0
};

static const FcChar8	*fcLangLatin2[] = {
    (FcChar8 *) "cs",   /* Czech */
    (FcChar8 *) "de",   /* German */
    (FcChar8 *) "en",   /* English */
    (FcChar8 *) "fi",   /* Finnish */
    (FcChar8 *) "hr",   /* Croatian */
    (FcChar8 *) "hu",   /* Hungarian */
    (FcChar8 *) "la",   /* Latin */
    (FcChar8 *) "pl",   /* Polish */
    (FcChar8 *) "ro",   /* Romanian */
    (FcChar8 *) "sk",   /* Slovak */
    (FcChar8 *) "sl",   /* Slovenian */
    (FcChar8 *) "sq",   /* Albanian */
    0
};

static const FcChar8	*fcLangCyrillic[] = {
    (FcChar8 *) "az",   /* Azerbaijani */
    (FcChar8 *) "ba",   /* Bashkir */
    (FcChar8 *) "bg",   /* Bulgarian */
    (FcChar8 *) "be",   /* Byelorussian */
    (FcChar8 *) "kk",   /* Kazakh */
    (FcChar8 *) "ky",   /* Kirghiz */
    (FcChar8 *) "mk",   /* Macedonian */
    (FcChar8 *) "mo",   /* Moldavian */
    (FcChar8 *) "mn",   /* Mongolian */
    (FcChar8 *) "ru",   /* Russian */
    (FcChar8 *) "sr",   /* Serbian */
    (FcChar8 *) "tg",   /* Tadzhik */
    (FcChar8 *) "tt",   /* Tatar */
    (FcChar8 *) "tk",   /* Turkmen */
    (FcChar8 *) "uz",   /* Uzbek */
    (FcChar8 *) "uk",   /* Ukrainian */
    0,
};

static const FcChar8	*fcLangGreek[] = {
    (FcChar8 *) "el",   /* Greek */
    0
};

static const FcChar8	*fcLangTurkish[] = {
    (FcChar8 *) "tr",   /* Turkish */
    0
};

static const FcChar8	*fcLangHebrew[] = {
    (FcChar8 *) "he",   /* Hebrew */
    (FcChar8 *) "yi",   /* Yiddish */
    0
};

static const FcChar8	*fcLangArabic[] = {
    (FcChar8 *) "ar",   /* arabic */
    0
};

static const FcChar8	*fcLangWindowsBaltic[] = {
    (FcChar8 *) "da",   /* Danish */
    (FcChar8 *) "de",   /* German */
    (FcChar8 *) "en",   /* English */
    (FcChar8 *) "et",   /* Estonian */
    (FcChar8 *) "fi",   /* Finnish */
    (FcChar8 *) "la",   /* Latin */
    (FcChar8 *) "lt",   /* Lithuanian */
    (FcChar8 *) "lv",   /* Latvian */
    (FcChar8 *) "no",   /* Norwegian */
    (FcChar8 *) "pl",   /* Polish */
    (FcChar8 *) "sl",   /* Slovenian */
    (FcChar8 *) "sv",   /* Swedish */
    0
};

static const FcChar8	*fcLangVietnamese[] = {
    (FcChar8 *) "vi",   /* Vietnamese */
    0,
};

static const FcChar8	*fcLangThai[] = {
    (FcChar8 *) "th",   /* Thai */
    0,
};

static const FcChar8	*fcLangJapanese[] = {
    (FcChar8 *) "ja",   /* Japanese */
    0,
};

static const FcChar8	*fcLangSimplifiedChinese[] = {
    (FcChar8 *) "zh-cn",    /* Chinese-China */
    0,
};

static const FcChar8	*fcLangKorean[] = {
    (FcChar8 *) "ko",   /* Korean */
    0,
};

static const FcChar8	*fcLangTraditionalChinese[] = {
    (FcChar8 *) "zh-tw",    /* Chinese-Taiwan */
    0,
};

static const FcChar8	*fcLangEnglish[] = {
    (FcChar8 *) "en",   /* English */
    0,
};

/*
 * Elide some of the less useful bits
 */
static const struct {
    int		    bit;
    const FcChar8   **lang;
} FcCodePageRange[] = {
    { 0,	fcLangLatin1 },
    { 1,	fcLangLatin2 },
    { 2,	fcLangCyrillic },
    { 3,	fcLangGreek },
    { 4,	fcLangTurkish },
    { 5,	fcLangHebrew },
    { 6,	fcLangArabic },
    { 7,	fcLangWindowsBaltic },
    { 8,	fcLangVietnamese },
/* 9-15 reserved for Alternate ANSI */
    { 16,	fcLangThai },
    { 17,	fcLangJapanese },
    { 18,	fcLangSimplifiedChinese },
    { 19,	fcLangKorean },
    { 20,	fcLangTraditionalChinese },
    { 21,	fcLangKorean },
/* 22-28 reserved for Alternate ANSI & OEM */
/*    { 29,	fcLangMacintosh }, */
/*    { 30,	fcLangOem }, */
/*     { 31,	fcLangSymbol },*/
/* 32-47 reserved for OEM */
    { 48,	fcLangGreek },
    { 49,	fcLangCyrillic },
/*    { 50,	fcLangMsdosNordic }, */
    { 51,	fcLangArabic },
/*    { 52,	fcLangMSDOS_CANADIAN_FRENCH }, */
    { 53,	fcLangHebrew },
/*    { 54,	fcLangMSDOS_ICELANDIC }, */
/*    { 55,	fcLangMSDOS_PORTUGUESE }, */
    { 56,	fcLangTurkish },
    { 57,	fcLangCyrillic },
    { 58,	fcLangLatin2 },
    { 59,	fcLangWindowsBaltic },
    { 60,	fcLangGreek },
    { 61,	fcLangArabic },
    { 62,	fcLangLatin1 },
    { 63,	fcLangEnglish },
};

#define NUM_CODE_PAGE_RANGE (sizeof FcCodePageRange / sizeof FcCodePageRange[0])

FcBool
FcFreeTypeHasLang (FcPattern *pattern, const FcChar8 *lang)
{
    FcChar8	*old;
    int		i;

    for (i = 0; FcPatternGetString (pattern, FC_LANG, i, &old) == FcResultMatch; i++)
	if (!FcStrCmp (lang, old))
	    return FcTrue;
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
    FcChar32	    codepoints;
    FcChar8	    *lang;
    FcBool	    hasLang = FcFalse;
    

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
		int		j;
		const FcChar8	*lang;

		for (j = 0; (lang = FcCodePageRange[i].lang[j]); j++)
		    if (!FcFreeTypeHasLang (pat, lang))
		    {
			if (!FcPatternAddString (pat, FC_LANG, lang))
			    goto bail1;
			hasLang = FcTrue;
		    }
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

    if (!hasLang)
    {
	if (!FcFreeTypeSetLang (pat, cs))
	    goto bail2;

	/*
	 * Make sure it has a lang entry
	 */
	if (FcPatternGetString (pat, FC_LANG, 0, &lang) != FcResultMatch)
	    if (!FcPatternAddString (pat, FC_LANG, (FcChar8 *) "x-unknown"))
		goto bail2;
    }

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
