/*
 * $XFree86: xc/lib/fontconfig/src/fcfreetype.c,v 1.11 2002/08/31 22:17:32 keithp Exp $
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
#include <freetype/ftsnames.h>
#include <freetype/ttnameid.h>

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

#define FC_NAME_PRIO_LANG	    0x0f00
#define FC_NAME_PRIO_LANG_ENGLISH   0x0200
#define FC_NAME_PRIO_LANG_LATIN	    0x0100
#define FC_NAME_PRIO_LANG_NONE	    0x0000

#define FC_NAME_PRIO_ENC	    0x00f0
#define FC_NAME_PRIO_ENC_UNICODE    0x0010
#define FC_NAME_PRIO_ENC_NONE	    0x0000

#define FC_NAME_PRIO_NAME	    0x000f
#define FC_NAME_PRIO_NAME_FAMILY    0x0002
#define FC_NAME_PRIO_NAME_PS	    0x0001
#define FC_NAME_PRIO_NAME_NONE	    0x0000

static FcBool
FcUcs4IsLatin (FcChar32 ucs4)
{
    FcChar32	page = ucs4 >> 8;
    
    if (page <= 2)
	return FcTrue;
    if (page == 0x1e)
	return FcTrue;
    if (0x20 <= page && page <= 0x23)
	return FcTrue;
    if (page == 0xfb)
	return FcTrue;
    if (page == 0xff)
	return FcTrue;
    return FcFalse;
}

static FcBool
FcUtf8IsLatin (FcChar8 *str, int len)
{
    while (len)
    {
	FcChar32    ucs4;
	int	    clen = FcUtf8ToUcs4 (str, &ucs4, len);
	if (clen <= 0)
	    return FcFalse;
	if (!FcUcs4IsLatin (ucs4))
	    return FcFalse;
	len -= clen;
	str += clen;
    }
    return FcTrue;
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
    FcLangSet	    *ls;
    FT_Library	    ftLibrary;
    FcChar8	    *family;
    FcChar8	    *style;
    TT_OS2	    *os2;
    TT_Header	    *head;
    const FcChar8   *exclusiveLang = 0;
    FT_SfntName	    sname;
    FT_UInt    	    snamei, snamec;
    FcBool	    family_allocated = FcFalse;
    FcBool	    style_allocated = FcFalse;
    int		    family_prio = 0;
    int		    style_prio = 0;

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

    /*
     * Grub through the name table looking for family
     * and style names.  FreeType makes quite a hash
     * of them
     */
    family = 0;
    style = 0;
    snamec = FT_Get_Sfnt_Name_Count (face);
    for (snamei = 0; snamei < snamec; snamei++)
    {
	FcChar8		*utf8;
	int		len;
	int		wchar;
	FcChar8		*src;
	int		src_len;
	FcChar8		*u8;
	FcChar32	ucs4;
	int		ilen, olen;
	int		prio = 0;
	
	const FcCharMap	*map;
	enum {
	    FcNameEncodingUtf16, 
	    FcNameEncodingAppleRoman,
	    FcNameEncodingLatin1 
	} encoding;
	
	
	if (FT_Get_Sfnt_Name (face, snamei, &sname) != 0)
	    break;
	
	/*
	 * Look for Unicode strings
	 */
	switch (sname.platform_id) {
	case TT_PLATFORM_APPLE_UNICODE:
	    /*
	     * All APPLE_UNICODE encodings are Utf16 BE
	     *
	     * Because there's no language id for Unicode,
	     * assume it's English
	     */
	    prio |= FC_NAME_PRIO_LANG_ENGLISH;
	    prio |= FC_NAME_PRIO_ENC_UNICODE;
	    encoding = FcNameEncodingUtf16;
	    break;
	case TT_PLATFORM_MACINTOSH:
	    switch (sname.encoding_id) {
	    case TT_MAC_ID_ROMAN:
		encoding = FcNameEncodingAppleRoman;
		break;
	    default:
		continue;
	    }
	    switch (sname.language_id) {
	    case TT_MAC_LANGID_ENGLISH:
		prio |= FC_NAME_PRIO_LANG_ENGLISH;
		break;
	    default:
		/*
		 * Sometimes Microsoft language ids
		 * end up in the macintosh table.  This
		 * is often accompanied by data in
		 * some mystic encoding.  Ignore these names
		 */
		if (sname.language_id >= 0x100)
		    continue;
		break;
	    }
	    break;
	case TT_PLATFORM_MICROSOFT:
	    switch (sname.encoding_id) {
	    case TT_MS_ID_UNICODE_CS:
		encoding = FcNameEncodingUtf16;
		prio |= FC_NAME_PRIO_ENC_UNICODE;
		break;
	    default:
		continue;
	    }
	    switch (sname.language_id & 0xff) {
	    case 0x09:
		prio |= FC_NAME_PRIO_LANG_ENGLISH;
		break;
	    default:
		break;
	    }
	    break;
	case TT_PLATFORM_ISO:
	    switch (sname.encoding_id) {
	    case TT_ISO_ID_10646:
		encoding = FcNameEncodingUtf16;
		prio |= FC_NAME_PRIO_ENC_UNICODE;
		break;
	    case TT_ISO_ID_7BIT_ASCII:
	    case TT_ISO_ID_8859_1:
		encoding = FcNameEncodingLatin1;
		break;
	    default:
		continue;
	    }
	    break;
	default:
	    continue;
	}
	
	/*
	 * Look for family and style names 
	 */
	switch (sname.name_id) {
	case TT_NAME_ID_FONT_FAMILY:
	    prio |= FC_NAME_PRIO_NAME_FAMILY;
	    break;
	case TT_NAME_ID_PS_NAME:
	    prio |= FC_NAME_PRIO_NAME_PS;
	    break;
	case TT_NAME_ID_FONT_SUBFAMILY:
	    break;
	default:
	    continue;
	}
	    
        src = (FcChar8 *) sname.string;
        src_len = sname.string_len;
	
	switch (encoding) {
	case FcNameEncodingUtf16:
	    /*
	     * Convert Utf16 to Utf8
	     */
	    
	    if (!FcUtf16Len (src, FcEndianBig, src_len, &len, &wchar))
		continue;
    
	    /*
	     * Allocate plenty of space.  Freed below
	     */
	    utf8 = malloc (len * FC_UTF8_MAX_LEN + 1);
	    if (!utf8)
		continue;
		
	    u8 = utf8;
	    
	    while ((ilen = FcUtf16ToUcs4 (src, FcEndianBig, &ucs4, src_len)) > 0)
	    {
		src_len -= ilen;
		src += ilen;
		olen = FcUcs4ToUtf8 (ucs4, u8);
		u8 += olen;
	    }
	    *u8 = '\0';
	    break;
	case FcNameEncodingLatin1:
	    /*
	     * Convert Latin1 to Utf8. Freed below
	     */
	    utf8 = malloc (src_len * 2 + 1);
	    if (!utf8)
		continue;

	    u8 = utf8;
	    while (src_len > 0)
	    {
		ucs4 = *src++;
		src_len--;
		olen = FcUcs4ToUtf8 (ucs4, u8);
		u8 += olen;
	    }
	    *u8 = '\0';
	    break;
	case FcNameEncodingAppleRoman:
	    /*
	     * Convert AppleRoman to Utf8
	     */
	    map = FcFreeTypeGetPrivateMap (ft_encoding_apple_roman);
	    if (!map)
		continue;

	    /* freed below */
	    utf8 = malloc (src_len * 3 + 1);
	    if (!utf8)
		continue;

	    u8 = utf8;
	    while (src_len > 0)
	    {
		ucs4 = FcFreeTypePrivateToUcs4 (*src++, map);
		src_len--;
		olen = FcUcs4ToUtf8 (ucs4, u8);
		u8 += olen;
	    }
	    *u8 = '\0';
	    break;
	default:
	    continue;
	}
	if ((prio & FC_NAME_PRIO_LANG) == FC_NAME_PRIO_LANG_NONE)
	    if (FcUtf8IsLatin (utf8, strlen ((char *) utf8)))
		prio |= FC_NAME_PRIO_LANG_LATIN;
			       
	if (FcDebug () & FC_DBG_SCANV)
	    printf ("\nfound name (name %d platform %d encoding %d language 0x%x prio 0x%x) %s\n",
		    sname.name_id, sname.platform_id,
		    sname.encoding_id, sname.language_id,
		    prio, utf8);
    
	switch (sname.name_id) {
	case TT_NAME_ID_FONT_FAMILY:
	case TT_NAME_ID_PS_NAME:
	    if (!family || prio > family_prio)
	    {
		if (family)
		    free (family);
		family = utf8;
		utf8 = 0;
		family_allocated = FcTrue;
		family_prio = prio;
	    }
	    break;
	case TT_NAME_ID_FONT_SUBFAMILY:
	    if (!style || prio > style_prio)
	    {
		if (style)
		    free (style);
		style = utf8;
		utf8 = 0;
		style_allocated = FcTrue;
		style_prio = prio;
	    }
	    break;
	}
	if (utf8)
	    free (utf8);
    }
    
    if (!family)
	family = (FcChar8 *) face->family_name;
    
    if (!style)
	style = (FcChar8 *) face->style_name;
    
    if (!family)
    {
	FcChar8	*start, *end;
	
	start = (FcChar8 *) strrchr ((char *) file, '/');
	if (start)
	    start++;
	else
	    start = (FcChar8 *) file;
	end = (FcChar8 *) strrchr ((char *) start, '.');
	if (!end)
	    end = start + strlen ((char *) start);
	/* freed below */
	family = malloc (end - start + 1);
	strncpy ((char *) family, (char *) start, end - start);
	family[end - start] = '\0';
	family_allocated = FcTrue;
    }

    if (FcDebug() & FC_DBG_SCAN)
	printf ("\"%s\" \"%s\" ", family, style ? style : (FcChar8 *) "<none>");

    if (!FcPatternAddString (pat, FC_FAMILY, family))
    {
	if (family_allocated)
	    free (family);
	if (style_allocated)
	    free (style);
	goto bail1;
    }

    if (family_allocated)
	free (family);

    if (style)
    {
	if (!FcPatternAddString (pat, FC_STYLE, style))
	{
	    if (style_allocated)
		free (style);
	    goto bail1;
	}
	if (style_allocated)
	    free (style);
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
     * Find the font revision (if available)
     */
    head = (TT_Header *) FT_Get_Sfnt_Table (face, ft_sfnt_head);
    if (head)
    {
	if (!FcPatternAddInteger (pat, FC_FONTVERSION, head->Font_Revision))
	    goto bail1;
    }
    else
    {
	if (!FcPatternAddInteger (pat, FC_FONTVERSION, 0))
	    goto bail1;
    }

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

    ls = FcFreeTypeLangSet (cs, exclusiveLang);
    if (!ls)
	goto bail2;

    if (!FcPatternAddLangSet (pat, FC_LANG, ls))
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
