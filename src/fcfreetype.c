/*
 * $RCSId: xc/lib/fontconfig/src/fcfreetype.c,v 1.11 2002/08/31 22:17:32 keithp Exp $
 *
 * Copyright © 2001 Keith Packard
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

/*
  Copyright © 2002-2003 by Juliusz Chroboczek

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fcint.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_INTERNAL_OBJECTS_H
#include FT_TRUETYPE_TABLES_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include FT_TYPE1_TABLES_H

#if HAVE_FT_GET_BDF_PROPERTY
#include FT_BDF_H
#include FT_MODULE_H
#define HAS_BDF_PROPERTY(f) ((f) && (f)->driver && \
			     (f)->driver->root.clazz->get_interface)
#define MY_Get_BDF_Property(f,n,p) (HAS_BDF_PROPERTY(f) ? \
				    FT_Get_BDF_Property(f,n,p) : \
				    FT_Err_Invalid_Argument)
#endif

#if !HAVE_FT_GET_BDF_PROPERTY
#warning "No FT_Get_BDF_Property"
#endif

#if !HAVE_FT_GET_PS_FONT_INFO
#warning "No FT_Get_Font_Info"
#endif

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
    /* halfwidth forms, don't include kana or white parens */
    if (0xff01 <= ucs4 && ucs4 <= 0xff5e)
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

/* Order is significant.  For example, some B&H fonts are hinted by
   URW++, and both strings appear in the notice. */

static const struct {
    const FT_String *notice;
    const FcChar8   *foundry;
} FcNoticeFoundries[] = {
    { (const FT_String *) "Bigelow",	(const FcChar8 *) "b&h" },
    { (const FT_String *) "Adobe",	(const FcChar8 *) "adobe" },
    { (const FT_String *) "Bitstream",	(const FcChar8 *) "bitstream" },
    { (const FT_String *) "Monotype",	(const FcChar8 *) "monotype" },
    { (const FT_String *) "Linotype",	(const FcChar8 *) "linotype" },
    { (const FT_String *) "LINOTYPE-HELL",
					(const FcChar8 *) "linotype" },
    { (const FT_String *) "IBM",	(const FcChar8 *) "ibm" },
    { (const FT_String *) "URW",	(const FcChar8 *) "urw" },
    { (const FT_String *) "International Typeface Corporation", 
					(const FcChar8 *) "itc" },
    { (const FT_String *) "Tiro Typeworks",
					(const FcChar8 *) "tiro" },
    { (const FT_String *) "XFree86",	(const FcChar8 *) "xfree86" },
    { (const FT_String *) "Microsoft",	(const FcChar8 *) "microsoft" },
    { (const FT_String *) "Omega",	(const FcChar8 *) "omega" },
    { (const FT_String *) "Font21",	(const FcChar8 *) "hwan" },
    { (const FT_String *) "HanYang System",
					(const FcChar8 *) "hanyang" }
};

#define NUM_NOTICE_FOUNDRIES	(sizeof (FcNoticeFoundries) / sizeof (FcNoticeFoundries[0]))

static const FcChar8 *
FcNoticeFoundry(const FT_String *notice)
{
    int i;

    if (notice)
	for(i = 0; i < NUM_NOTICE_FOUNDRIES; i++)
	    if (strstr ((const char *) notice, (const char *) FcNoticeFoundries[i].notice))
		return FcNoticeFoundries[i].foundry;
    return 0;
}

static FcBool
FcVendorMatch(const FT_Char vendor[4], const FT_Char *vendor_string)
{
    /* vendor is not necessarily NUL-terminated. */
    int i, len;
    
    len = strlen(vendor_string);
    if (memcmp(vendor, vendor_string, len) != 0)
        return FcFalse;
    for (i = len; i < 4; i++)
        if (vendor[i] != ' ' && vendor[i] != '\0')
            return FcFalse;
    return FcTrue;
}

/* This table is partly taken from ttmkfdir by Joerg Pommnitz. */

/* It should not contain useless entries (such as UNKN) nor duplicate
   entries for padding both with spaces and NULs. */

static const struct {
    const FT_Char   *vendor;
    const FcChar8   *foundry;
} FcVendorFoundries[] = {
    { (const FT_Char *) "ADBE", (const FcChar8 *) "adobe"},
    { (const FT_Char *) "AGFA", (const FcChar8 *) "agfa"},
    { (const FT_Char *) "ALTS", (const FcChar8 *) "altsys"},
    { (const FT_Char *) "APPL", (const FcChar8 *) "apple"},
    { (const FT_Char *) "ARPH", (const FcChar8 *) "arphic"},
    { (const FT_Char *) "ATEC", (const FcChar8 *) "alltype"},
    { (const FT_Char *) "B&H",  (const FcChar8 *) "b&h"},
    { (const FT_Char *) "BITS", (const FcChar8 *) "bitstream"},
    { (const FT_Char *) "CANO", (const FcChar8 *) "cannon"},
    { (const FT_Char *) "DYNA", (const FcChar8 *) "dynalab"},
    { (const FT_Char *) "EPSN", (const FcChar8 *) "epson"},
    { (const FT_Char *) "FJ",   (const FcChar8 *) "fujitsu"},
    { (const FT_Char *) "IBM",  (const FcChar8 *) "ibm"},
    { (const FT_Char *) "ITC",  (const FcChar8 *) "itc"},
    { (const FT_Char *) "IMPR", (const FcChar8 *) "impress"},
    { (const FT_Char *) "LARA", (const FcChar8 *) "larabiefonts"},
    { (const FT_Char *) "LEAF", (const FcChar8 *) "interleaf"},
    { (const FT_Char *) "LETR", (const FcChar8 *) "letraset"},
    { (const FT_Char *) "LINO", (const FcChar8 *) "linotype"},
    { (const FT_Char *) "MACR", (const FcChar8 *) "macromedia"},
    { (const FT_Char *) "MONO", (const FcChar8 *) "monotype"},
    { (const FT_Char *) "MS",   (const FcChar8 *) "microsoft"},
    { (const FT_Char *) "MT",   (const FcChar8 *) "monotype"},
    { (const FT_Char *) "NEC",  (const FcChar8 *) "nec"},
    { (const FT_Char *) "PARA", (const FcChar8 *) "paratype"},
    { (const FT_Char *) "QMSI", (const FcChar8 *) "qms"},
    { (const FT_Char *) "RICO", (const FcChar8 *) "ricoh"},
    { (const FT_Char *) "URW",  (const FcChar8 *) "urw"},
    { (const FT_Char *) "Y&Y",  (const FcChar8 *) "y&y"}
};

#define NUM_VENDOR_FOUNDRIES	(sizeof (FcVendorFoundries) / sizeof (FcVendorFoundries[0]))

static const FcChar8 *
FcVendorFoundry(const FT_Char vendor[4])
{
    int i;
    
    if (vendor)
	for(i = 0; i < NUM_VENDOR_FOUNDRIES; i++)
	    if (FcVendorMatch (vendor, FcVendorFoundries[i].vendor))
		return FcVendorFoundries[i].foundry;
    return 0;
}

typedef struct _FcStringConst {
    const FcChar8   *name;
    int		    value;
} FcStringConst;

static int
FcStringIsConst (const FcChar8		*string,
		 const FcStringConst	*c,
		 int			nc)
{
    int	i;

    for (i = 0; i < nc; i++)
	if (FcStrCmpIgnoreBlanksAndCase (string, c[i].name) == 0)
	    return c[i].value;
    return -1;
}

static int
FcStringContainsConst (const FcChar8	    *string,
		       const FcStringConst  *c,
		       int		    nc)
{
    int	i;

    for (i = 0; i < nc; i++)
	if (FcStrContainsIgnoreBlanksAndCase (string, c[i].name))
	    return c[i].value;
    return -1;
}

static const FcStringConst  weightConsts[] = {
    { "thin",		FC_WEIGHT_THIN },
    { "extralight",	FC_WEIGHT_EXTRALIGHT },
    { "ultralight",	FC_WEIGHT_ULTRALIGHT },
    { "light",		FC_WEIGHT_LIGHT },
    { "book",		FC_WEIGHT_BOOK },
    { "regular",	FC_WEIGHT_REGULAR },
    { "normal",		FC_WEIGHT_NORMAL },
    { "medium",		FC_WEIGHT_MEDIUM },
    { "demibold",	FC_WEIGHT_DEMIBOLD },
    { "demi",		FC_WEIGHT_DEMIBOLD },
    { "semibold",	FC_WEIGHT_SEMIBOLD },
    { "bold",		FC_WEIGHT_BOLD },
    { "extrabold",	FC_WEIGHT_EXTRABOLD },
    { "ultrabold",	FC_WEIGHT_ULTRABOLD },
    { "black",		FC_WEIGHT_BLACK },
    { "heavy",		FC_WEIGHT_HEAVY },
};

#define NUM_WEIGHT_CONSTS  (sizeof (weightConsts) / sizeof (weightConsts[0]))

#define FcIsWeight(s)	    FcStringIsConst(s,weightConsts,NUM_WEIGHT_CONSTS)
#define FcContainsWeight(s) FcStringContainsConst (s,weightConsts,NUM_WEIGHT_CONSTS)

static const FcStringConst  widthConsts[] = {
    { "ultracondensed",	FC_WIDTH_ULTRACONDENSED },
    { "extracondensed",	FC_WIDTH_EXTRACONDENSED },
    { "semicondensed",  FC_WIDTH_SEMICONDENSED },
    { "condensed",	FC_WIDTH_CONDENSED },	/* must be after *condensed */
    { "normal",		FC_WIDTH_NORMAL },
    { "semiexpanded",	FC_WIDTH_SEMIEXPANDED },
    { "extraexpanded",	FC_WIDTH_EXTRAEXPANDED },
    { "ultraexpanded",	FC_WIDTH_ULTRAEXPANDED },
    { "expanded",	FC_WIDTH_EXPANDED },	/* must be after *expanded */
};

#define NUM_WIDTH_CONSTS    (sizeof (widthConsts) / sizeof (widthConsts[0]))

#define FcIsWidth(s)	    FcStringIsConst(s,widthConsts,NUM_WIDTH_CONSTS)
#define FcContainsWidth(s)  FcStringContainsConst (s,widthConsts,NUM_WIDTH_CONSTS)

static const FcStringConst  slantConsts[] = {
    { "italic",		FC_SLANT_ITALIC },
    { "oblique",	FC_SLANT_OBLIQUE },
};

#define NUM_SLANT_CONSTS    (sizeof (slantConsts) / sizeof (slantConsts[0]))

#define FcIsSlant(s)	    FcStringIsConst(s,slantConsts,NUM_SLANT_CONSTS)
#define FcContainsSlant(s)  FcStringContainsConst (s,slantConsts,NUM_SLANT_CONSTS)

static double
FcGetPixelSize (FT_Face face, int i)
{
#if HAVE_FT_GET_BDF_PROPERTY
    if (face->num_fixed_sizes == 1)
    {
	BDF_PropertyRec	prop;
	int		rc;

	rc = MY_Get_BDF_Property (face, "PIXEL_SIZE", &prop);
	if (rc == 0 && prop.type == BDF_PROPERTY_TYPE_INTEGER)
	    return (double) prop.u.integer;
    }
#endif
#if HAVE_FT_BITMAP_SIZE_Y_PPEM
    return (double) face->available_sizes[i].y_ppem / 64.0;
#else
    return (double) face->available_sizes[i].height;
#endif
}

FcPattern *
FcFreeTypeQuery (const FcChar8	*file,
		 int		id,
		 FcBlanks	*blanks,
		 int		*count)
{
    FT_Face	    face;
    FcPattern	    *pat;
    int		    slant = -1;
    int		    weight = -1;
    int		    width = -1;
    int		    i;
    FcCharSet	    *cs;
    FcLangSet	    *ls;
    FT_Library	    ftLibrary;
    FcChar8	    *family = 0;
    FcChar8	    *style = 0;
    const FcChar8   *foundry = 0;
    int		    spacing;
    TT_OS2	    *os2;
#if HAVE_FT_GET_PS_FONT_INFO
    PS_FontInfoRec  psfontinfo;
#endif
#if HAVE_FT_GET_BDF_PROPERTY
    BDF_PropertyRec prop;
#endif
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


    /*
     * Get the OS/2 table
     */
    os2 = (TT_OS2 *) FT_Get_Sfnt_Table (face, ft_sfnt_os2);

    /*
     * Look first in the OS/2 table for the foundry, if
     * not found here, the various notices will be searched for
     * that information, either from the sfnt name tables or
     * the Postscript FontInfo dictionary.  Finally, the
     * BDF properties will queried.
     */
    
    if (os2 && os2->version >= 0x0001 && os2->version != 0xffff)
        foundry = FcVendorFoundry(os2->achVendID);

    /*
     * Grub through the name table looking for family
     * and style names.  FreeType makes quite a hash
     * of them
     */
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
        case TT_NAME_ID_TRADEMARK:
        case TT_NAME_ID_MANUFACTURER:
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
        case TT_NAME_ID_TRADEMARK:
        case TT_NAME_ID_MANUFACTURER:
	    /* If the foundry wasn't found in the OS/2 table, look here */
            if(!foundry)
                foundry = FcNoticeFoundry((FT_String *) utf8);
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
	printf ("\n\"%s\" \"%s\"\n", family, style ? style : (FcChar8 *) "<none>");

    if (!FcPatternAddString (pat, FC_FAMILY, family))
	goto bail1;

    if (style)
    {
	if (!FcPatternAddString (pat, FC_STYLE, style))
	    goto bail1;
    }

    if (!FcPatternAddString (pat, FC_FILE, file))
	goto bail1;

    if (!FcPatternAddInteger (pat, FC_INDEX, id))
	goto bail1;

    if (!FcPatternAddString (pat, FC_SOURCE, (FcChar8 *) "FreeType"))
	goto bail1;

#if 0
    /*
     * don't even try this -- CJK 'monospace' fonts are really
     * dual width, and most other fonts don't bother to set
     * the attribute.  Sigh.
     */
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

    if (os2 && os2->version != 0xffff)
    {
	if (os2->usWeightClass == 0)
	    ;
	else if (os2->usWeightClass < 150)
	    weight = FC_WEIGHT_THIN;
	else if (os2->usWeightClass < 250)
	    weight = FC_WEIGHT_EXTRALIGHT;
	else if (os2->usWeightClass < 350)
	    weight = FC_WEIGHT_LIGHT;
	else if (os2->usWeightClass < 450)
	    weight = FC_WEIGHT_REGULAR;
	else if (os2->usWeightClass < 550)
	    weight = FC_WEIGHT_MEDIUM;
	else if (os2->usWeightClass < 650)
	    weight = FC_WEIGHT_SEMIBOLD;
	else if (os2->usWeightClass < 750)
	    weight = FC_WEIGHT_BOLD;
	else if (os2->usWeightClass < 850)
	    weight = FC_WEIGHT_EXTRABOLD;
	else if (os2->usWeightClass < 950)
	    weight = FC_WEIGHT_BLACK;

	switch (os2->usWidthClass) {
	case 1:	width = FC_WIDTH_ULTRACONDENSED; break;
	case 2:	width = FC_WIDTH_EXTRACONDENSED; break;
	case 3:	width = FC_WIDTH_CONDENSED; break;
	case 4:	width = FC_WIDTH_SEMICONDENSED; break;
	case 5:	width = FC_WIDTH_NORMAL; break;
	case 6:	width = FC_WIDTH_SEMIEXPANDED; break;
	case 7:	width = FC_WIDTH_EXPANDED; break;
	case 8:	width = FC_WIDTH_EXTRAEXPANDED; break;
	case 9:	width = FC_WIDTH_ULTRAEXPANDED; break;
	}
    }

    /*
     * Type 1: Check for FontInfo dictionary information
     * Code from g2@magestudios.net (Gerard Escalante)
     */
    
#if HAVE_FT_GET_PS_FONT_INFO
    if (FT_Get_PS_Font_Info(face, &psfontinfo) == 0)
    {
	if (weight == -1 && psfontinfo.weight)
	{
	    weight = FcIsWeight (psfontinfo.weight);
    	    if (FcDebug() & FC_DBG_SCANV)
    		printf ("\tType1 weight %s maps to %d\n",
			psfontinfo.weight, weight);
	}
     
#if 0
	/* 
	 * Don't bother with italic_angle; FreeType already extracts that
	 * information for us and sticks it into style_flags
	 */
        if (psfontinfo.italic_angle)
            slant = FC_SLANT_ITALIC; 
        else
            slant = FC_SLANT_ROMAN; 
#endif

        if(!foundry)
            foundry = FcNoticeFoundry(psfontinfo.notice);
    }
#endif /* HAVE_FT_GET_PS_FONT_INFO */
    
#if HAVE_FT_GET_BDF_PROPERTY
    /*
     * Finally, look for a FOUNDRY BDF property if no other
     * mechanism has managed to locate a foundry
     */

    if (!foundry)
    {
	int             rc;
	BDF_PropertyRec prop;
	rc = MY_Get_BDF_Property(face, "FOUNDRY", &prop);
	if(rc == 0 && prop.type == BDF_PROPERTY_TYPE_ATOM)
	    foundry = prop.u.atom;
    }

    if (width == -1)
    {
	if (MY_Get_BDF_Property(face, "RELATIVE_SETWIDTH", &prop) == 0 &&
	    (prop.type == BDF_PROPERTY_TYPE_INTEGER ||
	     prop.type == BDF_PROPERTY_TYPE_CARDINAL))
	{
	    FT_Int32	value;
	    
	    if (prop.type == BDF_PROPERTY_TYPE_INTEGER)
		value = prop.u.integer;
	    else
		value = (FT_Int32) prop.u.cardinal;
	    switch ((value + 5) / 10) {
	    case 1: width = FC_WIDTH_ULTRACONDENSED; break;
	    case 2: width = FC_WIDTH_EXTRACONDENSED; break;
	    case 3: width = FC_WIDTH_CONDENSED; break;
	    case 4: width = FC_WIDTH_SEMICONDENSED; break;
	    case 5: width = FC_WIDTH_NORMAL; break;
	    case 6: width = FC_WIDTH_SEMIEXPANDED; break;
	    case 7: width = FC_WIDTH_EXPANDED; break;
	    case 8: width = FC_WIDTH_EXTRAEXPANDED; break;
	    case 9: width = FC_WIDTH_ULTRAEXPANDED; break;
	    }
	}
	if (width == -1 &&
	    MY_Get_BDF_Property (face, "SETWIDTH_NAME", &prop) == 0 &&
	    prop.type == BDF_PROPERTY_TYPE_ATOM)
	{
	    width = FcIsWidth (prop.u.atom);
	    if (FcDebug () & FC_DBG_SCANV)
		printf ("\tsetwidth %s maps to %d\n", prop.u.atom, width);
	}
    }
#endif

    /*
     * Look for weight, width and slant names in the style value
     */
    if (style)
    {
	if (weight == -1)
	{
	    weight = FcContainsWeight (style);
	    if (FcDebug() & FC_DBG_SCANV)
		printf ("\tStyle %s maps to weight %d\n", style, weight);
	}
	if (width == -1)
	{
	    width = FcContainsWidth (style);
	    if (FcDebug() & FC_DBG_SCANV)
		printf ("\tStyle %s maps to width %d\n", style, width);
	}
	if (slant == -1)
	{
	    slant = FcContainsSlant (style);
	    if (FcDebug() & FC_DBG_SCANV)
		printf ("\tStyle %s maps to slant %d\n", style, slant);
	}
    }
    /*
     * Pull default values from the FreeType flags if more
     * specific values not found above
     */
    if (slant == -1)
    {
	slant = FC_SLANT_ROMAN;
	if (face->style_flags & FT_STYLE_FLAG_ITALIC)
	    slant = FC_SLANT_ITALIC;
    }

    if (weight == -1)
    {
	weight = FC_WEIGHT_MEDIUM;
	if (face->style_flags & FT_STYLE_FLAG_BOLD)
	    weight = FC_WEIGHT_BOLD;
    }

    if (width == -1)
	width = FC_WIDTH_NORMAL;

    if (foundry == 0)
	foundry = "unknown";

    if (!FcPatternAddInteger (pat, FC_SLANT, slant))
	goto bail1;

    if (!FcPatternAddInteger (pat, FC_WEIGHT, weight))
	goto bail1;

    if (!FcPatternAddInteger (pat, FC_WIDTH, width))
	goto bail1;

    if (!FcPatternAddString (pat, FC_FOUNDRY, foundry))
	goto bail1;

    /*
     * Compute the unicode coverage for the font
     */
    cs = FcFreeTypeCharSetAndSpacing (face, blanks, &spacing);
    if (!cs)
	goto bail1;

#if HAVE_FT_GET_BDF_PROPERTY
    /* For PCF fonts, override the computed spacing with the one from
       the property */
    if(MY_Get_BDF_Property(face, "SPACING", &prop) == 0 &&
       prop.type == BDF_PROPERTY_TYPE_ATOM) {
        if(!strcmp(prop.u.atom, "c") || !strcmp(prop.u.atom, "C"))
            spacing = FC_CHARCELL;
        else if(!strcmp(prop.u.atom, "m") || !strcmp(prop.u.atom, "M"))
            spacing = FC_MONO;
        else if(!strcmp(prop.u.atom, "p") || !strcmp(prop.u.atom, "P"))
            spacing = FC_PROPORTIONAL;
    }
#endif

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
    {
	FcLangSetDestroy (ls);
	goto bail2;
    }

    FcLangSetDestroy (ls);

    if (spacing != FC_PROPORTIONAL)
	if (!FcPatternAddInteger (pat, FC_SPACING, spacing))
	    goto bail2;

    if (!(face->face_flags & FT_FACE_FLAG_SCALABLE))
    {
	for (i = 0; i < face->num_fixed_sizes; i++)
	    if (!FcPatternAddDouble (pat, FC_PIXEL_SIZE,
				     FcGetPixelSize (face, i)))
		goto bail1;
	if (!FcPatternAddBool (pat, FC_ANTIALIAS, FcFalse))
	    goto bail1;
#if HAVE_FT_GET_BDF_PROPERTY
        if(face->num_fixed_sizes == 1) {
            int rc;
            int value;
            BDF_PropertyRec prop;

            rc = MY_Get_BDF_Property(face, "POINT_SIZE", &prop);
            if(rc == 0 && prop.type == BDF_PROPERTY_TYPE_INTEGER)
                value = prop.u.integer;
            else if(rc == 0 && prop.type == BDF_PROPERTY_TYPE_CARDINAL)
                value = prop.u.cardinal;
            else
                goto nevermind;
            if(!FcPatternAddDouble(pat, FC_SIZE, value / 10.0))
                goto nevermind;

            rc = MY_Get_BDF_Property(face, "RESOLUTION_Y", &prop);
            if(rc == 0 && prop.type == BDF_PROPERTY_TYPE_INTEGER)
                value = prop.u.integer;
            else if(rc == 0 && prop.type == BDF_PROPERTY_TYPE_CARDINAL)
                value = prop.u.cardinal;
            else
                goto nevermind;
            if(!FcPatternAddDouble(pat, FC_DPI, (double)value))
                goto nevermind;

        }
    nevermind:
        ;
#endif
    }

    /*
     * Drop our reference to the charset
     */
    FcCharSetDestroy (cs);
    
    /*
     * Deallocate family/style values
     */
    
    if (family_allocated)
	free (family);
    if (style_allocated)
	free (style);
    
    FT_Done_Face (face);
    FT_Done_FreeType (ftLibrary);
    return pat;

bail2:
    FcCharSetDestroy (cs);
bail1:
    FcPatternDestroy (pat);
    if (family_allocated)
	free (family);
    if (style_allocated)
	free (style);
bail0:
    FT_Done_Face (face);
bail:
    FT_Done_FreeType (ftLibrary);
    return 0;
}


/*
 * For our purposes, this approximation is sufficient
 */
#if !HAVE_FT_GET_NEXT_CHAR
#define FT_Get_Next_Char(face, ucs4, gi) ((ucs4) >= 0xffffff ? \
					  (*(gi) = 0), 0 : \
					  (*(gi) = 1), (ucs4) + 1)
#warning "No FT_Get_Next_Char"
#endif

typedef struct _FcCharEnt {
    FcChar16	    bmp;
    unsigned char   encode;
} FcCharEnt;

struct _FcCharMap {
    const FcCharEnt *ent;
    int		    nent;
};

typedef struct _FcFontDecode {
    FT_Encoding	    encoding;
    const FcCharMap *map;
    FcChar32	    max;
} FcFontDecode;

static const FcCharEnt AppleRomanEnt[] = {
    { 0x0020, 0x20 }, /* SPACE */
    { 0x0021, 0x21 }, /* EXCLAMATION MARK */
    { 0x0022, 0x22 }, /* QUOTATION MARK */
    { 0x0023, 0x23 }, /* NUMBER SIGN */
    { 0x0024, 0x24 }, /* DOLLAR SIGN */
    { 0x0025, 0x25 }, /* PERCENT SIGN */
    { 0x0026, 0x26 }, /* AMPERSAND */
    { 0x0027, 0x27 }, /* APOSTROPHE */
    { 0x0028, 0x28 }, /* LEFT PARENTHESIS */
    { 0x0029, 0x29 }, /* RIGHT PARENTHESIS */
    { 0x002A, 0x2A }, /* ASTERISK */
    { 0x002B, 0x2B }, /* PLUS SIGN */
    { 0x002C, 0x2C }, /* COMMA */
    { 0x002D, 0x2D }, /* HYPHEN-MINUS */
    { 0x002E, 0x2E }, /* FULL STOP */
    { 0x002F, 0x2F }, /* SOLIDUS */
    { 0x0030, 0x30 }, /* DIGIT ZERO */
    { 0x0031, 0x31 }, /* DIGIT ONE */
    { 0x0032, 0x32 }, /* DIGIT TWO */
    { 0x0033, 0x33 }, /* DIGIT THREE */
    { 0x0034, 0x34 }, /* DIGIT FOUR */
    { 0x0035, 0x35 }, /* DIGIT FIVE */
    { 0x0036, 0x36 }, /* DIGIT SIX */
    { 0x0037, 0x37 }, /* DIGIT SEVEN */
    { 0x0038, 0x38 }, /* DIGIT EIGHT */
    { 0x0039, 0x39 }, /* DIGIT NINE */
    { 0x003A, 0x3A }, /* COLON */
    { 0x003B, 0x3B }, /* SEMICOLON */
    { 0x003C, 0x3C }, /* LESS-THAN SIGN */
    { 0x003D, 0x3D }, /* EQUALS SIGN */
    { 0x003E, 0x3E }, /* GREATER-THAN SIGN */
    { 0x003F, 0x3F }, /* QUESTION MARK */
    { 0x0040, 0x40 }, /* COMMERCIAL AT */
    { 0x0041, 0x41 }, /* LATIN CAPITAL LETTER A */
    { 0x0042, 0x42 }, /* LATIN CAPITAL LETTER B */
    { 0x0043, 0x43 }, /* LATIN CAPITAL LETTER C */
    { 0x0044, 0x44 }, /* LATIN CAPITAL LETTER D */
    { 0x0045, 0x45 }, /* LATIN CAPITAL LETTER E */
    { 0x0046, 0x46 }, /* LATIN CAPITAL LETTER F */
    { 0x0047, 0x47 }, /* LATIN CAPITAL LETTER G */
    { 0x0048, 0x48 }, /* LATIN CAPITAL LETTER H */
    { 0x0049, 0x49 }, /* LATIN CAPITAL LETTER I */
    { 0x004A, 0x4A }, /* LATIN CAPITAL LETTER J */
    { 0x004B, 0x4B }, /* LATIN CAPITAL LETTER K */
    { 0x004C, 0x4C }, /* LATIN CAPITAL LETTER L */
    { 0x004D, 0x4D }, /* LATIN CAPITAL LETTER M */
    { 0x004E, 0x4E }, /* LATIN CAPITAL LETTER N */
    { 0x004F, 0x4F }, /* LATIN CAPITAL LETTER O */
    { 0x0050, 0x50 }, /* LATIN CAPITAL LETTER P */
    { 0x0051, 0x51 }, /* LATIN CAPITAL LETTER Q */
    { 0x0052, 0x52 }, /* LATIN CAPITAL LETTER R */
    { 0x0053, 0x53 }, /* LATIN CAPITAL LETTER S */
    { 0x0054, 0x54 }, /* LATIN CAPITAL LETTER T */
    { 0x0055, 0x55 }, /* LATIN CAPITAL LETTER U */
    { 0x0056, 0x56 }, /* LATIN CAPITAL LETTER V */
    { 0x0057, 0x57 }, /* LATIN CAPITAL LETTER W */
    { 0x0058, 0x58 }, /* LATIN CAPITAL LETTER X */
    { 0x0059, 0x59 }, /* LATIN CAPITAL LETTER Y */
    { 0x005A, 0x5A }, /* LATIN CAPITAL LETTER Z */
    { 0x005B, 0x5B }, /* LEFT SQUARE BRACKET */
    { 0x005C, 0x5C }, /* REVERSE SOLIDUS */
    { 0x005D, 0x5D }, /* RIGHT SQUARE BRACKET */
    { 0x005E, 0x5E }, /* CIRCUMFLEX ACCENT */
    { 0x005F, 0x5F }, /* LOW LINE */
    { 0x0060, 0x60 }, /* GRAVE ACCENT */
    { 0x0061, 0x61 }, /* LATIN SMALL LETTER A */
    { 0x0062, 0x62 }, /* LATIN SMALL LETTER B */
    { 0x0063, 0x63 }, /* LATIN SMALL LETTER C */
    { 0x0064, 0x64 }, /* LATIN SMALL LETTER D */
    { 0x0065, 0x65 }, /* LATIN SMALL LETTER E */
    { 0x0066, 0x66 }, /* LATIN SMALL LETTER F */
    { 0x0067, 0x67 }, /* LATIN SMALL LETTER G */
    { 0x0068, 0x68 }, /* LATIN SMALL LETTER H */
    { 0x0069, 0x69 }, /* LATIN SMALL LETTER I */
    { 0x006A, 0x6A }, /* LATIN SMALL LETTER J */
    { 0x006B, 0x6B }, /* LATIN SMALL LETTER K */
    { 0x006C, 0x6C }, /* LATIN SMALL LETTER L */
    { 0x006D, 0x6D }, /* LATIN SMALL LETTER M */
    { 0x006E, 0x6E }, /* LATIN SMALL LETTER N */
    { 0x006F, 0x6F }, /* LATIN SMALL LETTER O */
    { 0x0070, 0x70 }, /* LATIN SMALL LETTER P */
    { 0x0071, 0x71 }, /* LATIN SMALL LETTER Q */
    { 0x0072, 0x72 }, /* LATIN SMALL LETTER R */
    { 0x0073, 0x73 }, /* LATIN SMALL LETTER S */
    { 0x0074, 0x74 }, /* LATIN SMALL LETTER T */
    { 0x0075, 0x75 }, /* LATIN SMALL LETTER U */
    { 0x0076, 0x76 }, /* LATIN SMALL LETTER V */
    { 0x0077, 0x77 }, /* LATIN SMALL LETTER W */
    { 0x0078, 0x78 }, /* LATIN SMALL LETTER X */
    { 0x0079, 0x79 }, /* LATIN SMALL LETTER Y */
    { 0x007A, 0x7A }, /* LATIN SMALL LETTER Z */
    { 0x007B, 0x7B }, /* LEFT CURLY BRACKET */
    { 0x007C, 0x7C }, /* VERTICAL LINE */
    { 0x007D, 0x7D }, /* RIGHT CURLY BRACKET */
    { 0x007E, 0x7E }, /* TILDE */
    { 0x00A0, 0xCA }, /* NO-BREAK SPACE */
    { 0x00A1, 0xC1 }, /* INVERTED EXCLAMATION MARK */
    { 0x00A2, 0xA2 }, /* CENT SIGN */
    { 0x00A3, 0xA3 }, /* POUND SIGN */
    { 0x00A5, 0xB4 }, /* YEN SIGN */
    { 0x00A7, 0xA4 }, /* SECTION SIGN */
    { 0x00A8, 0xAC }, /* DIAERESIS */
    { 0x00A9, 0xA9 }, /* COPYRIGHT SIGN */
    { 0x00AA, 0xBB }, /* FEMININE ORDINAL INDICATOR */
    { 0x00AB, 0xC7 }, /* LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
    { 0x00AC, 0xC2 }, /* NOT SIGN */
    { 0x00AE, 0xA8 }, /* REGISTERED SIGN */
    { 0x00AF, 0xF8 }, /* MACRON */
    { 0x00B0, 0xA1 }, /* DEGREE SIGN */
    { 0x00B1, 0xB1 }, /* PLUS-MINUS SIGN */
    { 0x00B4, 0xAB }, /* ACUTE ACCENT */
    { 0x00B5, 0xB5 }, /* MICRO SIGN */
    { 0x00B6, 0xA6 }, /* PILCROW SIGN */
    { 0x00B7, 0xE1 }, /* MIDDLE DOT */
    { 0x00B8, 0xFC }, /* CEDILLA */
    { 0x00BA, 0xBC }, /* MASCULINE ORDINAL INDICATOR */
    { 0x00BB, 0xC8 }, /* RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
    { 0x00BF, 0xC0 }, /* INVERTED QUESTION MARK */
    { 0x00C0, 0xCB }, /* LATIN CAPITAL LETTER A WITH GRAVE */
    { 0x00C1, 0xE7 }, /* LATIN CAPITAL LETTER A WITH ACUTE */
    { 0x00C2, 0xE5 }, /* LATIN CAPITAL LETTER A WITH CIRCUMFLEX */
    { 0x00C3, 0xCC }, /* LATIN CAPITAL LETTER A WITH TILDE */
    { 0x00C4, 0x80 }, /* LATIN CAPITAL LETTER A WITH DIAERESIS */
    { 0x00C5, 0x81 }, /* LATIN CAPITAL LETTER A WITH RING ABOVE */
    { 0x00C6, 0xAE }, /* LATIN CAPITAL LETTER AE */
    { 0x00C7, 0x82 }, /* LATIN CAPITAL LETTER C WITH CEDILLA */
    { 0x00C8, 0xE9 }, /* LATIN CAPITAL LETTER E WITH GRAVE */
    { 0x00C9, 0x83 }, /* LATIN CAPITAL LETTER E WITH ACUTE */
    { 0x00CA, 0xE6 }, /* LATIN CAPITAL LETTER E WITH CIRCUMFLEX */
    { 0x00CB, 0xE8 }, /* LATIN CAPITAL LETTER E WITH DIAERESIS */
    { 0x00CC, 0xED }, /* LATIN CAPITAL LETTER I WITH GRAVE */
    { 0x00CD, 0xEA }, /* LATIN CAPITAL LETTER I WITH ACUTE */
    { 0x00CE, 0xEB }, /* LATIN CAPITAL LETTER I WITH CIRCUMFLEX */
    { 0x00CF, 0xEC }, /* LATIN CAPITAL LETTER I WITH DIAERESIS */
    { 0x00D1, 0x84 }, /* LATIN CAPITAL LETTER N WITH TILDE */
    { 0x00D2, 0xF1 }, /* LATIN CAPITAL LETTER O WITH GRAVE */
    { 0x00D3, 0xEE }, /* LATIN CAPITAL LETTER O WITH ACUTE */
    { 0x00D4, 0xEF }, /* LATIN CAPITAL LETTER O WITH CIRCUMFLEX */
    { 0x00D5, 0xCD }, /* LATIN CAPITAL LETTER O WITH TILDE */
    { 0x00D6, 0x85 }, /* LATIN CAPITAL LETTER O WITH DIAERESIS */
    { 0x00D8, 0xAF }, /* LATIN CAPITAL LETTER O WITH STROKE */
    { 0x00D9, 0xF4 }, /* LATIN CAPITAL LETTER U WITH GRAVE */
    { 0x00DA, 0xF2 }, /* LATIN CAPITAL LETTER U WITH ACUTE */
    { 0x00DB, 0xF3 }, /* LATIN CAPITAL LETTER U WITH CIRCUMFLEX */
    { 0x00DC, 0x86 }, /* LATIN CAPITAL LETTER U WITH DIAERESIS */
    { 0x00DF, 0xA7 }, /* LATIN SMALL LETTER SHARP S */
    { 0x00E0, 0x88 }, /* LATIN SMALL LETTER A WITH GRAVE */
    { 0x00E1, 0x87 }, /* LATIN SMALL LETTER A WITH ACUTE */
    { 0x00E2, 0x89 }, /* LATIN SMALL LETTER A WITH CIRCUMFLEX */
    { 0x00E3, 0x8B }, /* LATIN SMALL LETTER A WITH TILDE */
    { 0x00E4, 0x8A }, /* LATIN SMALL LETTER A WITH DIAERESIS */
    { 0x00E5, 0x8C }, /* LATIN SMALL LETTER A WITH RING ABOVE */
    { 0x00E6, 0xBE }, /* LATIN SMALL LETTER AE */
    { 0x00E7, 0x8D }, /* LATIN SMALL LETTER C WITH CEDILLA */
    { 0x00E8, 0x8F }, /* LATIN SMALL LETTER E WITH GRAVE */
    { 0x00E9, 0x8E }, /* LATIN SMALL LETTER E WITH ACUTE */
    { 0x00EA, 0x90 }, /* LATIN SMALL LETTER E WITH CIRCUMFLEX */
    { 0x00EB, 0x91 }, /* LATIN SMALL LETTER E WITH DIAERESIS */
    { 0x00EC, 0x93 }, /* LATIN SMALL LETTER I WITH GRAVE */
    { 0x00ED, 0x92 }, /* LATIN SMALL LETTER I WITH ACUTE */
    { 0x00EE, 0x94 }, /* LATIN SMALL LETTER I WITH CIRCUMFLEX */
    { 0x00EF, 0x95 }, /* LATIN SMALL LETTER I WITH DIAERESIS */
    { 0x00F1, 0x96 }, /* LATIN SMALL LETTER N WITH TILDE */
    { 0x00F2, 0x98 }, /* LATIN SMALL LETTER O WITH GRAVE */
    { 0x00F3, 0x97 }, /* LATIN SMALL LETTER O WITH ACUTE */
    { 0x00F4, 0x99 }, /* LATIN SMALL LETTER O WITH CIRCUMFLEX */
    { 0x00F5, 0x9B }, /* LATIN SMALL LETTER O WITH TILDE */
    { 0x00F6, 0x9A }, /* LATIN SMALL LETTER O WITH DIAERESIS */
    { 0x00F7, 0xD6 }, /* DIVISION SIGN */
    { 0x00F8, 0xBF }, /* LATIN SMALL LETTER O WITH STROKE */
    { 0x00F9, 0x9D }, /* LATIN SMALL LETTER U WITH GRAVE */
    { 0x00FA, 0x9C }, /* LATIN SMALL LETTER U WITH ACUTE */
    { 0x00FB, 0x9E }, /* LATIN SMALL LETTER U WITH CIRCUMFLEX */
    { 0x00FC, 0x9F }, /* LATIN SMALL LETTER U WITH DIAERESIS */
    { 0x00FF, 0xD8 }, /* LATIN SMALL LETTER Y WITH DIAERESIS */
    { 0x0131, 0xF5 }, /* LATIN SMALL LETTER DOTLESS I */
    { 0x0152, 0xCE }, /* LATIN CAPITAL LIGATURE OE */
    { 0x0153, 0xCF }, /* LATIN SMALL LIGATURE OE */
    { 0x0178, 0xD9 }, /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
    { 0x0192, 0xC4 }, /* LATIN SMALL LETTER F WITH HOOK */
    { 0x02C6, 0xF6 }, /* MODIFIER LETTER CIRCUMFLEX ACCENT */
    { 0x02C7, 0xFF }, /* CARON */
    { 0x02D8, 0xF9 }, /* BREVE */
    { 0x02D9, 0xFA }, /* DOT ABOVE */
    { 0x02DA, 0xFB }, /* RING ABOVE */
    { 0x02DB, 0xFE }, /* OGONEK */
    { 0x02DC, 0xF7 }, /* SMALL TILDE */
    { 0x02DD, 0xFD }, /* DOUBLE ACUTE ACCENT */
    { 0x03A9, 0xBD }, /* GREEK CAPITAL LETTER OMEGA */
    { 0x03C0, 0xB9 }, /* GREEK SMALL LETTER PI */
    { 0x2013, 0xD0 }, /* EN DASH */
    { 0x2014, 0xD1 }, /* EM DASH */
    { 0x2018, 0xD4 }, /* LEFT SINGLE QUOTATION MARK */
    { 0x2019, 0xD5 }, /* RIGHT SINGLE QUOTATION MARK */
    { 0x201A, 0xE2 }, /* SINGLE LOW-9 QUOTATION MARK */
    { 0x201C, 0xD2 }, /* LEFT DOUBLE QUOTATION MARK */
    { 0x201D, 0xD3 }, /* RIGHT DOUBLE QUOTATION MARK */
    { 0x201E, 0xE3 }, /* DOUBLE LOW-9 QUOTATION MARK */
    { 0x2020, 0xA0 }, /* DAGGER */
    { 0x2021, 0xE0 }, /* DOUBLE DAGGER */
    { 0x2022, 0xA5 }, /* BULLET */
    { 0x2026, 0xC9 }, /* HORIZONTAL ELLIPSIS */
    { 0x2030, 0xE4 }, /* PER MILLE SIGN */
    { 0x2039, 0xDC }, /* SINGLE LEFT-POINTING ANGLE QUOTATION MARK */
    { 0x203A, 0xDD }, /* SINGLE RIGHT-POINTING ANGLE QUOTATION MARK */
    { 0x2044, 0xDA }, /* FRACTION SLASH */
    { 0x20AC, 0xDB }, /* EURO SIGN */
    { 0x2122, 0xAA }, /* TRADE MARK SIGN */
    { 0x2202, 0xB6 }, /* PARTIAL DIFFERENTIAL */
    { 0x2206, 0xC6 }, /* INCREMENT */
    { 0x220F, 0xB8 }, /* N-ARY PRODUCT */
    { 0x2211, 0xB7 }, /* N-ARY SUMMATION */
    { 0x221A, 0xC3 }, /* SQUARE ROOT */
    { 0x221E, 0xB0 }, /* INFINITY */
    { 0x222B, 0xBA }, /* INTEGRAL */
    { 0x2248, 0xC5 }, /* ALMOST EQUAL TO */
    { 0x2260, 0xAD }, /* NOT EQUAL TO */
    { 0x2264, 0xB2 }, /* LESS-THAN OR EQUAL TO */
    { 0x2265, 0xB3 }, /* GREATER-THAN OR EQUAL TO */
    { 0x25CA, 0xD7 }, /* LOZENGE */
    { 0xF8FF, 0xF0 }, /* Apple logo */
    { 0xFB01, 0xDE }, /* LATIN SMALL LIGATURE FI */
    { 0xFB02, 0xDF }, /* LATIN SMALL LIGATURE FL */
};

static const FcCharMap AppleRoman = {
    AppleRomanEnt,
    sizeof (AppleRomanEnt) / sizeof (AppleRomanEnt[0])
};

static const FcCharEnt AdobeSymbolEnt[] = {
    { 0x0020, 0x20 }, /* SPACE	# space */
    { 0x0021, 0x21 }, /* EXCLAMATION MARK	# exclam */
    { 0x0023, 0x23 }, /* NUMBER SIGN	# numbersign */
    { 0x0025, 0x25 }, /* PERCENT SIGN	# percent */
    { 0x0026, 0x26 }, /* AMPERSAND	# ampersand */
    { 0x0028, 0x28 }, /* LEFT PARENTHESIS	# parenleft */
    { 0x0029, 0x29 }, /* RIGHT PARENTHESIS	# parenright */
    { 0x002B, 0x2B }, /* PLUS SIGN	# plus */
    { 0x002C, 0x2C }, /* COMMA	# comma */
    { 0x002E, 0x2E }, /* FULL STOP	# period */
    { 0x002F, 0x2F }, /* SOLIDUS	# slash */
    { 0x0030, 0x30 }, /* DIGIT ZERO	# zero */
    { 0x0031, 0x31 }, /* DIGIT ONE	# one */
    { 0x0032, 0x32 }, /* DIGIT TWO	# two */
    { 0x0033, 0x33 }, /* DIGIT THREE	# three */
    { 0x0034, 0x34 }, /* DIGIT FOUR	# four */
    { 0x0035, 0x35 }, /* DIGIT FIVE	# five */
    { 0x0036, 0x36 }, /* DIGIT SIX	# six */
    { 0x0037, 0x37 }, /* DIGIT SEVEN	# seven */
    { 0x0038, 0x38 }, /* DIGIT EIGHT	# eight */
    { 0x0039, 0x39 }, /* DIGIT NINE	# nine */
    { 0x003A, 0x3A }, /* COLON	# colon */
    { 0x003B, 0x3B }, /* SEMICOLON	# semicolon */
    { 0x003C, 0x3C }, /* LESS-THAN SIGN	# less */
    { 0x003D, 0x3D }, /* EQUALS SIGN	# equal */
    { 0x003E, 0x3E }, /* GREATER-THAN SIGN	# greater */
    { 0x003F, 0x3F }, /* QUESTION MARK	# question */
    { 0x005B, 0x5B }, /* LEFT SQUARE BRACKET	# bracketleft */
    { 0x005D, 0x5D }, /* RIGHT SQUARE BRACKET	# bracketright */
    { 0x005F, 0x5F }, /* LOW LINE	# underscore */
    { 0x007B, 0x7B }, /* LEFT CURLY BRACKET	# braceleft */
    { 0x007C, 0x7C }, /* VERTICAL LINE	# bar */
    { 0x007D, 0x7D }, /* RIGHT CURLY BRACKET	# braceright */
    { 0x00A0, 0x20 }, /* NO-BREAK SPACE	# space */
    { 0x00AC, 0xD8 }, /* NOT SIGN	# logicalnot */
    { 0x00B0, 0xB0 }, /* DEGREE SIGN	# degree */
    { 0x00B1, 0xB1 }, /* PLUS-MINUS SIGN	# plusminus */
    { 0x00B5, 0x6D }, /* MICRO SIGN	# mu */
    { 0x00D7, 0xB4 }, /* MULTIPLICATION SIGN	# multiply */
    { 0x00F7, 0xB8 }, /* DIVISION SIGN	# divide */
    { 0x0192, 0xA6 }, /* LATIN SMALL LETTER F WITH HOOK	# florin */
    { 0x0391, 0x41 }, /* GREEK CAPITAL LETTER ALPHA	# Alpha */
    { 0x0392, 0x42 }, /* GREEK CAPITAL LETTER BETA	# Beta */
    { 0x0393, 0x47 }, /* GREEK CAPITAL LETTER GAMMA	# Gamma */
    { 0x0394, 0x44 }, /* GREEK CAPITAL LETTER DELTA	# Delta */
    { 0x0395, 0x45 }, /* GREEK CAPITAL LETTER EPSILON	# Epsilon */
    { 0x0396, 0x5A }, /* GREEK CAPITAL LETTER ZETA	# Zeta */
    { 0x0397, 0x48 }, /* GREEK CAPITAL LETTER ETA	# Eta */
    { 0x0398, 0x51 }, /* GREEK CAPITAL LETTER THETA	# Theta */
    { 0x0399, 0x49 }, /* GREEK CAPITAL LETTER IOTA	# Iota */
    { 0x039A, 0x4B }, /* GREEK CAPITAL LETTER KAPPA	# Kappa */
    { 0x039B, 0x4C }, /* GREEK CAPITAL LETTER LAMDA	# Lambda */
    { 0x039C, 0x4D }, /* GREEK CAPITAL LETTER MU	# Mu */
    { 0x039D, 0x4E }, /* GREEK CAPITAL LETTER NU	# Nu */
    { 0x039E, 0x58 }, /* GREEK CAPITAL LETTER XI	# Xi */
    { 0x039F, 0x4F }, /* GREEK CAPITAL LETTER OMICRON	# Omicron */
    { 0x03A0, 0x50 }, /* GREEK CAPITAL LETTER PI	# Pi */
    { 0x03A1, 0x52 }, /* GREEK CAPITAL LETTER RHO	# Rho */
    { 0x03A3, 0x53 }, /* GREEK CAPITAL LETTER SIGMA	# Sigma */
    { 0x03A4, 0x54 }, /* GREEK CAPITAL LETTER TAU	# Tau */
    { 0x03A5, 0x55 }, /* GREEK CAPITAL LETTER UPSILON	# Upsilon */
    { 0x03A6, 0x46 }, /* GREEK CAPITAL LETTER PHI	# Phi */
    { 0x03A7, 0x43 }, /* GREEK CAPITAL LETTER CHI	# Chi */
    { 0x03A8, 0x59 }, /* GREEK CAPITAL LETTER PSI	# Psi */
    { 0x03A9, 0x57 }, /* GREEK CAPITAL LETTER OMEGA	# Omega */
    { 0x03B1, 0x61 }, /* GREEK SMALL LETTER ALPHA	# alpha */
    { 0x03B2, 0x62 }, /* GREEK SMALL LETTER BETA	# beta */
    { 0x03B3, 0x67 }, /* GREEK SMALL LETTER GAMMA	# gamma */
    { 0x03B4, 0x64 }, /* GREEK SMALL LETTER DELTA	# delta */
    { 0x03B5, 0x65 }, /* GREEK SMALL LETTER EPSILON	# epsilon */
    { 0x03B6, 0x7A }, /* GREEK SMALL LETTER ZETA	# zeta */
    { 0x03B7, 0x68 }, /* GREEK SMALL LETTER ETA	# eta */
    { 0x03B8, 0x71 }, /* GREEK SMALL LETTER THETA	# theta */
    { 0x03B9, 0x69 }, /* GREEK SMALL LETTER IOTA	# iota */
    { 0x03BA, 0x6B }, /* GREEK SMALL LETTER KAPPA	# kappa */
    { 0x03BB, 0x6C }, /* GREEK SMALL LETTER LAMDA	# lambda */
    { 0x03BC, 0x6D }, /* GREEK SMALL LETTER MU	# mu */
    { 0x03BD, 0x6E }, /* GREEK SMALL LETTER NU	# nu */
    { 0x03BE, 0x78 }, /* GREEK SMALL LETTER XI	# xi */
    { 0x03BF, 0x6F }, /* GREEK SMALL LETTER OMICRON	# omicron */
    { 0x03C0, 0x70 }, /* GREEK SMALL LETTER PI	# pi */
    { 0x03C1, 0x72 }, /* GREEK SMALL LETTER RHO	# rho */
    { 0x03C2, 0x56 }, /* GREEK SMALL LETTER FINAL SIGMA	# sigma1 */
    { 0x03C3, 0x73 }, /* GREEK SMALL LETTER SIGMA	# sigma */
    { 0x03C4, 0x74 }, /* GREEK SMALL LETTER TAU	# tau */
    { 0x03C5, 0x75 }, /* GREEK SMALL LETTER UPSILON	# upsilon */
    { 0x03C6, 0x66 }, /* GREEK SMALL LETTER PHI	# phi */
    { 0x03C7, 0x63 }, /* GREEK SMALL LETTER CHI	# chi */
    { 0x03C8, 0x79 }, /* GREEK SMALL LETTER PSI	# psi */
    { 0x03C9, 0x77 }, /* GREEK SMALL LETTER OMEGA	# omega */
    { 0x03D1, 0x4A }, /* GREEK THETA SYMBOL	# theta1 */
    { 0x03D2, 0xA1 }, /* GREEK UPSILON WITH HOOK SYMBOL	# Upsilon1 */
    { 0x03D5, 0x6A }, /* GREEK PHI SYMBOL	# phi1 */
    { 0x03D6, 0x76 }, /* GREEK PI SYMBOL	# omega1 */
    { 0x2022, 0xB7 }, /* BULLET	# bullet */
    { 0x2026, 0xBC }, /* HORIZONTAL ELLIPSIS	# ellipsis */
    { 0x2032, 0xA2 }, /* PRIME	# minute */
    { 0x2033, 0xB2 }, /* DOUBLE PRIME	# second */
    { 0x2044, 0xA4 }, /* FRACTION SLASH	# fraction */
    { 0x20AC, 0xA0 }, /* EURO SIGN	# Euro */
    { 0x2111, 0xC1 }, /* BLACK-LETTER CAPITAL I	# Ifraktur */
    { 0x2118, 0xC3 }, /* SCRIPT CAPITAL P	# weierstrass */
    { 0x211C, 0xC2 }, /* BLACK-LETTER CAPITAL R	# Rfraktur */
    { 0x2126, 0x57 }, /* OHM SIGN	# Omega */
    { 0x2135, 0xC0 }, /* ALEF SYMBOL	# aleph */
    { 0x2190, 0xAC }, /* LEFTWARDS ARROW	# arrowleft */
    { 0x2191, 0xAD }, /* UPWARDS ARROW	# arrowup */
    { 0x2192, 0xAE }, /* RIGHTWARDS ARROW	# arrowright */
    { 0x2193, 0xAF }, /* DOWNWARDS ARROW	# arrowdown */
    { 0x2194, 0xAB }, /* LEFT RIGHT ARROW	# arrowboth */
    { 0x21B5, 0xBF }, /* DOWNWARDS ARROW WITH CORNER LEFTWARDS	# carriagereturn */
    { 0x21D0, 0xDC }, /* LEFTWARDS DOUBLE ARROW	# arrowdblleft */
    { 0x21D1, 0xDD }, /* UPWARDS DOUBLE ARROW	# arrowdblup */
    { 0x21D2, 0xDE }, /* RIGHTWARDS DOUBLE ARROW	# arrowdblright */
    { 0x21D3, 0xDF }, /* DOWNWARDS DOUBLE ARROW	# arrowdbldown */
    { 0x21D4, 0xDB }, /* LEFT RIGHT DOUBLE ARROW	# arrowdblboth */
    { 0x2200, 0x22 }, /* FOR ALL	# universal */
    { 0x2202, 0xB6 }, /* PARTIAL DIFFERENTIAL	# partialdiff */
    { 0x2203, 0x24 }, /* THERE EXISTS	# existential */
    { 0x2205, 0xC6 }, /* EMPTY SET	# emptyset */
    { 0x2206, 0x44 }, /* INCREMENT	# Delta */
    { 0x2207, 0xD1 }, /* NABLA	# gradient */
    { 0x2208, 0xCE }, /* ELEMENT OF	# element */
    { 0x2209, 0xCF }, /* NOT AN ELEMENT OF	# notelement */
    { 0x220B, 0x27 }, /* CONTAINS AS MEMBER	# suchthat */
    { 0x220F, 0xD5 }, /* N-ARY PRODUCT	# product */
    { 0x2211, 0xE5 }, /* N-ARY SUMMATION	# summation */
    { 0x2212, 0x2D }, /* MINUS SIGN	# minus */
    { 0x2215, 0xA4 }, /* DIVISION SLASH	# fraction */
    { 0x2217, 0x2A }, /* ASTERISK OPERATOR	# asteriskmath */
    { 0x221A, 0xD6 }, /* SQUARE ROOT	# radical */
    { 0x221D, 0xB5 }, /* PROPORTIONAL TO	# proportional */
    { 0x221E, 0xA5 }, /* INFINITY	# infinity */
    { 0x2220, 0xD0 }, /* ANGLE	# angle */
    { 0x2227, 0xD9 }, /* LOGICAL AND	# logicaland */
    { 0x2228, 0xDA }, /* LOGICAL OR	# logicalor */
    { 0x2229, 0xC7 }, /* INTERSECTION	# intersection */
    { 0x222A, 0xC8 }, /* UNION	# union */
    { 0x222B, 0xF2 }, /* INTEGRAL	# integral */
    { 0x2234, 0x5C }, /* THEREFORE	# therefore */
    { 0x223C, 0x7E }, /* TILDE OPERATOR	# similar */
    { 0x2245, 0x40 }, /* APPROXIMATELY EQUAL TO	# congruent */
    { 0x2248, 0xBB }, /* ALMOST EQUAL TO	# approxequal */
    { 0x2260, 0xB9 }, /* NOT EQUAL TO	# notequal */
    { 0x2261, 0xBA }, /* IDENTICAL TO	# equivalence */
    { 0x2264, 0xA3 }, /* LESS-THAN OR EQUAL TO	# lessequal */
    { 0x2265, 0xB3 }, /* GREATER-THAN OR EQUAL TO	# greaterequal */
    { 0x2282, 0xCC }, /* SUBSET OF	# propersubset */
    { 0x2283, 0xC9 }, /* SUPERSET OF	# propersuperset */
    { 0x2284, 0xCB }, /* NOT A SUBSET OF	# notsubset */
    { 0x2286, 0xCD }, /* SUBSET OF OR EQUAL TO	# reflexsubset */
    { 0x2287, 0xCA }, /* SUPERSET OF OR EQUAL TO	# reflexsuperset */
    { 0x2295, 0xC5 }, /* CIRCLED PLUS	# circleplus */
    { 0x2297, 0xC4 }, /* CIRCLED TIMES	# circlemultiply */
    { 0x22A5, 0x5E }, /* UP TACK	# perpendicular */
    { 0x22C5, 0xD7 }, /* DOT OPERATOR	# dotmath */
    { 0x2320, 0xF3 }, /* TOP HALF INTEGRAL	# integraltp */
    { 0x2321, 0xF5 }, /* BOTTOM HALF INTEGRAL	# integralbt */
    { 0x2329, 0xE1 }, /* LEFT-POINTING ANGLE BRACKET	# angleleft */
    { 0x232A, 0xF1 }, /* RIGHT-POINTING ANGLE BRACKET	# angleright */
    { 0x25CA, 0xE0 }, /* LOZENGE	# lozenge */
    { 0x2660, 0xAA }, /* BLACK SPADE SUIT	# spade */
    { 0x2663, 0xA7 }, /* BLACK CLUB SUIT	# club */
    { 0x2665, 0xA9 }, /* BLACK HEART SUIT	# heart */
    { 0x2666, 0xA8 }, /* BLACK DIAMOND SUIT	# diamond */
    { 0xF6D9, 0xD3 }, /* COPYRIGHT SIGN SERIF	# copyrightserif (CUS) */
    { 0xF6DA, 0xD2 }, /* REGISTERED SIGN SERIF	# registerserif (CUS) */
    { 0xF6DB, 0xD4 }, /* TRADE MARK SIGN SERIF	# trademarkserif (CUS) */
    { 0xF8E5, 0x60 }, /* RADICAL EXTENDER	# radicalex (CUS) */
    { 0xF8E6, 0xBD }, /* VERTICAL ARROW EXTENDER	# arrowvertex (CUS) */
    { 0xF8E7, 0xBE }, /* HORIZONTAL ARROW EXTENDER	# arrowhorizex (CUS) */
    { 0xF8E8, 0xE2 }, /* REGISTERED SIGN SANS SERIF	# registersans (CUS) */
    { 0xF8E9, 0xE3 }, /* COPYRIGHT SIGN SANS SERIF	# copyrightsans (CUS) */
    { 0xF8EA, 0xE4 }, /* TRADE MARK SIGN SANS SERIF	# trademarksans (CUS) */
    { 0xF8EB, 0xE6 }, /* LEFT PAREN TOP	# parenlefttp (CUS) */
    { 0xF8EC, 0xE7 }, /* LEFT PAREN EXTENDER	# parenleftex (CUS) */
    { 0xF8ED, 0xE8 }, /* LEFT PAREN BOTTOM	# parenleftbt (CUS) */
    { 0xF8EE, 0xE9 }, /* LEFT SQUARE BRACKET TOP	# bracketlefttp (CUS) */
    { 0xF8EF, 0xEA }, /* LEFT SQUARE BRACKET EXTENDER	# bracketleftex (CUS) */
    { 0xF8F0, 0xEB }, /* LEFT SQUARE BRACKET BOTTOM	# bracketleftbt (CUS) */
    { 0xF8F1, 0xEC }, /* LEFT CURLY BRACKET TOP	# bracelefttp (CUS) */
    { 0xF8F2, 0xED }, /* LEFT CURLY BRACKET MID	# braceleftmid (CUS) */
    { 0xF8F3, 0xEE }, /* LEFT CURLY BRACKET BOTTOM	# braceleftbt (CUS) */
    { 0xF8F4, 0xEF }, /* CURLY BRACKET EXTENDER	# braceex (CUS) */
    { 0xF8F5, 0xF4 }, /* INTEGRAL EXTENDER	# integralex (CUS) */
    { 0xF8F6, 0xF6 }, /* RIGHT PAREN TOP	# parenrighttp (CUS) */
    { 0xF8F7, 0xF7 }, /* RIGHT PAREN EXTENDER	# parenrightex (CUS) */
    { 0xF8F8, 0xF8 }, /* RIGHT PAREN BOTTOM	# parenrightbt (CUS) */
    { 0xF8F9, 0xF9 }, /* RIGHT SQUARE BRACKET TOP	# bracketrighttp (CUS) */
    { 0xF8FA, 0xFA }, /* RIGHT SQUARE BRACKET EXTENDER	# bracketrightex (CUS) */
    { 0xF8FB, 0xFB }, /* RIGHT SQUARE BRACKET BOTTOM	# bracketrightbt (CUS) */
    { 0xF8FC, 0xFC }, /* RIGHT CURLY BRACKET TOP	# bracerighttp (CUS) */
    { 0xF8FD, 0xFD }, /* RIGHT CURLY BRACKET MID	# bracerightmid (CUS) */
    { 0xF8FE, 0xFE }, /* RIGHT CURLY BRACKET BOTTOM	# bracerightbt (CUS) */
};

static const FcCharMap AdobeSymbol = {
    AdobeSymbolEnt,
    sizeof (AdobeSymbolEnt) / sizeof (AdobeSymbolEnt[0]),
};
    
static const FcFontDecode fcFontDecoders[] = {
    { ft_encoding_unicode,	0,		(1 << 21) - 1 },
    { ft_encoding_symbol,	&AdobeSymbol,	(1 << 16) - 1 },
    { ft_encoding_apple_roman,	&AppleRoman,	(1 << 16) - 1 },
};

#define NUM_DECODE  (sizeof (fcFontDecoders) / sizeof (fcFontDecoders[0]))

FcChar32
FcFreeTypeUcs4ToPrivate (FcChar32 ucs4, const FcCharMap *map)
{
    int		low, high, mid;
    FcChar16	bmp;

    low = 0;
    high = map->nent - 1;
    if (ucs4 < map->ent[low].bmp || map->ent[high].bmp < ucs4)
	return ~0;
    while (low <= high)
    {
	mid = (high + low) >> 1;
	bmp = map->ent[mid].bmp;
	if (ucs4 == bmp)
	    return (FT_ULong) map->ent[mid].encode;
	if (ucs4 < bmp)
	    high = mid - 1;
	else
	    low = mid + 1;
    }
    return ~0;
}

FcChar32
FcFreeTypePrivateToUcs4 (FcChar32 private, const FcCharMap *map)
{
    int	    i;

    for (i = 0; i < map->nent; i++)
	if (map->ent[i].encode == private)
	    return (FcChar32) map->ent[i].bmp;
    return ~0;
}

const FcCharMap *
FcFreeTypeGetPrivateMap (FT_Encoding encoding)
{
    int	i;

    for (i = 0; i < NUM_DECODE; i++)
	if (fcFontDecoders[i].encoding == encoding)
	    return fcFontDecoders[i].map;
    return 0;
}

#include "../fc-glyphname/fcglyphname.h"

static FcChar32
FcHashGlyphName (const FcChar8 *name)
{
    FcChar32	h = 0;
    FcChar8	c;

    while ((c = *name++))
    {
	h = ((h << 1) | (h >> 31)) ^ c;
    }
    return h;
}

#if HAVE_FT_HAS_PS_GLYPH_NAMES
/*
 * Use Type1 glyph names for fonts which have reliable names
 * and which export an Adobe Custom mapping
 */
static FcBool
FcFreeTypeUseNames (FT_Face face)
{
    FT_Int  map;
    
    if (!FT_Has_PS_Glyph_Names (face))
	return FcFalse;
    for (map = 0; map < face->num_charmaps; map++)
	if (face->charmaps[map]->encoding == ft_encoding_adobe_custom)
	    return FcTrue;
    return FcFalse;
}

static FcChar8 *
FcUcs4ToGlyphName (FcChar32 ucs4)
{
    int		i = (int) (ucs4 % FC_GLYPHNAME_HASH);
    int		r = 0;
    FcGlyphName	*gn;

    while ((gn = ucs_to_name[i]))
    {
	if (gn->ucs == ucs4)
	    return gn->name;
	if (!r) 
	{
	    r = (int) (ucs4 % FC_GLYPHNAME_REHASH);
	    if (!r)
		r = 1;
	}
	i += r;
	if (i >= FC_GLYPHNAME_HASH)
	    i -= FC_GLYPHNAME_HASH;
    }
    return 0;
}

static FcChar32
FcGlyphNameToUcs4 (FcChar8 *name)
{
    FcChar32	h = FcHashGlyphName (name);
    int		i = (int) (h % FC_GLYPHNAME_HASH);
    int		r = 0;
    FcGlyphName	*gn;

    while ((gn = name_to_ucs[i]))
    {
	if (!strcmp ((char *) name, (char *) gn->name))
	    return gn->ucs;
	if (!r) 
	{
	    r = (int) (h % FC_GLYPHNAME_REHASH);
	    if (!r)
		r = 1;
	}
	i += r;
	if (i >= FC_GLYPHNAME_HASH)
	    i -= FC_GLYPHNAME_HASH;
    }
    return 0xffff;
}

/*
 * Search through a font for a glyph by name.  This is
 * currently a linear search as there doesn't appear to be
 * any defined order within the font
 */
static FT_UInt
FcFreeTypeGlyphNameIndex (FT_Face face, FcChar8 *name)
{
    FT_UInt gindex;
    FcChar8 name_buf[FC_GLYPHNAME_MAXLEN + 2];

    for (gindex = 0; gindex < face->num_glyphs; gindex++)
    {
	if (FT_Get_Glyph_Name (face, gindex, name_buf, FC_GLYPHNAME_MAXLEN+1) == 0)
	    if (!strcmp ((char *) name, (char *) name_buf))
		return gindex;
    }
    return 0;
}
#endif

/*
 * Map a UCS4 glyph to a glyph index.  Use all available encoding
 * tables to try and find one that works.  This information is expected
 * to be cached by higher levels, so performance isn't critical
 */

FT_UInt
FcFreeTypeCharIndex (FT_Face face, FcChar32 ucs4)
{
    int		    initial, offset, decode;
    FT_UInt	    glyphindex;
    FcChar32	    charcode;

    initial = 0;
    /*
     * Find the current encoding
     */
    if (face->charmap)
    {
	for (; initial < NUM_DECODE; initial++)
	    if (fcFontDecoders[initial].encoding == face->charmap->encoding)
		break;
	if (initial == NUM_DECODE)
	    initial = 0;
    }
    /*
     * Check each encoding for the glyph, starting with the current one
     */
    for (offset = 0; offset < NUM_DECODE; offset++)
    {
	decode = (initial + offset) % NUM_DECODE;
	if (!face->charmap || face->charmap->encoding != fcFontDecoders[decode].encoding)
	    if (FT_Select_Charmap (face, fcFontDecoders[decode].encoding) != 0)
		continue;
	if (fcFontDecoders[decode].map)
	{
	    charcode = FcFreeTypeUcs4ToPrivate (ucs4, fcFontDecoders[decode].map);
	    if (charcode == ~0)
		continue;
	}
	else
	    charcode = ucs4;
	glyphindex = FT_Get_Char_Index (face, (FT_ULong) charcode);
	if (glyphindex)
	    return glyphindex;
    }
#if HAVE_FT_HAS_PS_GLYPH_NAMES
    /*
     * Check postscript name table if present
     */
    if (FcFreeTypeUseNames (face))
    {
	FcChar8	*name = FcUcs4ToGlyphName (ucs4);
	if (name)
	{
	    glyphindex = FcFreeTypeGlyphNameIndex (face, name);
	    if (glyphindex)
		return glyphindex;
	}
    }
#endif
    return 0;
}

static FcBool
FcFreeTypeCheckGlyph (FT_Face face, FcChar32 ucs4, 
		      FT_UInt glyph, FcBlanks *blanks,
		      FT_Pos *advance)
{
    FT_Int	    load_flags = FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH | FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING;
    FT_GlyphSlot    slot;
    
    /*
     * For bitmap-only fonts, assume that they're OK.
     */
    if ((face->face_flags & FT_FACE_FLAG_SCALABLE) == 0)
	return FcTrue;

    /*
     * When using scalable fonts, only report those glyphs
     * which can be scaled; otherwise those fonts will
     * only be available at some sizes, and never when
     * transformed.  Avoid this by simply reporting bitmap-only
     * glyphs as missing
     */
    if (face->face_flags & FT_FACE_FLAG_SCALABLE)
	load_flags |= FT_LOAD_NO_BITMAP;
    
    if (FT_Load_Glyph (face, glyph, load_flags))
	return FcFalse;
    
    slot = face->glyph;
    if (!glyph)
	return FcFalse;
    
    *advance = slot->metrics.horiAdvance;

    switch (slot->format) {
    case ft_glyph_format_bitmap:
	/*
	 * Bitmaps are assumed to be reasonable; if
	 * this proves to be a rash assumption, this
	 * code can be easily modified
	 */
	return FcTrue;
    case ft_glyph_format_outline:
	/*
	 * Glyphs with contours are always OK
	 */
	if (slot->outline.n_contours != 0)
	    return FcTrue;
	/*
	 * Glyphs with no contours are only OK if
	 * they're members of the Blanks set specified
	 * in the configuration.  If blanks isn't set,
	 * then allow any glyph to be blank
	 */
	if (!blanks || FcBlanksIsMember (blanks, ucs4))
	    return FcTrue;
	/* fall through ... */
    default:
	break;
    }
    return FcFalse;
}

#define FC_MIN(a,b) ((a) < (b) ? (a) : (b))
#define FC_MAX(a,b) ((a) > (b) ? (a) : (b))
#define FC_ABS(a)   ((a) < 0 ? -(a) : (a))
#define APPROXIMATELY_EQUAL(x,y) (FC_ABS ((x) - (y)) <= FC_MAX (FC_ABS (x), FC_ABS (y)) / 33)

FcCharSet *
FcFreeTypeCharSetAndSpacing (FT_Face face, FcBlanks *blanks, int *spacing)
{
    FcChar32	    page, off, max, ucs4;
#ifdef CHECK
    FcChar32	    font_max = 0;
#endif
    FcCharSet	    *fcs;
    FcCharLeaf	    *leaf;
    const FcCharMap *map;
    int		    o;
    int		    i;
    FT_UInt	    glyph;
    FT_Pos	    advance, advance_one = 0, advance_two = 0;
    FcBool	    has_advance = FcFalse, fixed_advance = FcTrue, dual_advance = FcFalse;

    fcs = FcCharSetCreate ();
    if (!fcs)
	goto bail0;
    
#ifdef CHECK
    printf ("Family %s style %s\n", face->family_name, face->style_name);
#endif
    for (o = 0; o < NUM_DECODE; o++)
    {
	if (FT_Select_Charmap (face, fcFontDecoders[o].encoding) != 0)
	    continue;
	map = fcFontDecoders[o].map;
	if (map)
	{
	    /*
	     * Non-Unicode tables are easy; there's a list of all possible
	     * characters
	     */
	    for (i = 0; i < map->nent; i++)
	    {
		ucs4 = map->ent[i].bmp;
		glyph = FT_Get_Char_Index (face, map->ent[i].encode);
		if (glyph && 
		    FcFreeTypeCheckGlyph (face, ucs4, glyph, blanks, &advance))
		{
		    if (!has_advance)
		    {
			has_advance = FcTrue;
			advance_one = advance;
		    }
		    else if (!APPROXIMATELY_EQUAL (advance, advance_one))
                    {
                        if (fixed_advance)
                        {
                            dual_advance = FcTrue;
                            fixed_advance = FcFalse;
                            advance_two = advance;
                        }
                        else if (!APPROXIMATELY_EQUAL (advance, advance_two))
                            dual_advance = FcFalse;
                    }

		    leaf = FcCharSetFindLeafCreate (fcs, ucs4);
		    if (!leaf)
			goto bail1;
		    leaf->map[(ucs4 & 0xff) >> 5] |= (1 << (ucs4 & 0x1f));
#ifdef CHECK
		    if (ucs4 > font_max)
			font_max = ucs4;
#endif
		}
	    }
	}
	else
	{
	    FT_UInt gindex;
	  
	    max = fcFontDecoders[o].max;
	    /*
	     * Find the first encoded character in the font
	     */
	    if (FT_Get_Char_Index (face, 0))
	    {
		ucs4 = 0;
		gindex = 1;
	    }
	    else
	    {
		ucs4 = FT_Get_Next_Char (face, 0, &gindex);
		if (!ucs4)
		    gindex = 0;
	    }

	    while (gindex)
	    {
		page = ucs4 >> 8;
		leaf = 0;
		while ((ucs4 >> 8) == page)
		{
		    glyph = FT_Get_Char_Index (face, ucs4);
		    if (glyph && FcFreeTypeCheckGlyph (face, ucs4, 
						       glyph, blanks, &advance))
		    {
			if (!has_advance)
			{
			    has_advance = FcTrue;
			    advance_one = advance;
			}
		        else if (!APPROXIMATELY_EQUAL (advance, advance_one))
                        {
                            if (fixed_advance)
                            {
                                dual_advance = FcTrue;
                                fixed_advance = FcFalse;
                                advance_two = advance;
                            }
                            else if (!APPROXIMATELY_EQUAL (advance, advance_two))
                                dual_advance = FcFalse;
                        }

			if (!leaf)
			{
			    leaf = FcCharSetFindLeafCreate (fcs, ucs4);
			    if (!leaf)
				goto bail1;
			}
			off = ucs4 & 0xff;
			leaf->map[off >> 5] |= (1 << (off & 0x1f));
#ifdef CHECK
			if (ucs4 > font_max)
			    font_max = ucs4;
#endif
		    }
		    ucs4++;
		}
		ucs4 = FT_Get_Next_Char (face, ucs4 - 1, &gindex);
		if (!ucs4)
		    gindex = 0;
	    }
#ifdef CHECK
	    for (ucs4 = 0; ucs4 < 0x10000; ucs4++)
	    {
		FcBool	    FT_Has, FC_Has;

		FT_Has = FT_Get_Char_Index (face, ucs4) != 0;
		FC_Has = FcCharSetHasChar (fcs, ucs4);
		if (FT_Has != FC_Has)
		{
		    printf ("0x%08x FT says %d FC says %d\n", ucs4, FT_Has, FC_Has);
		}
	    }
#endif
	}
    }
#if HAVE_FT_HAS_PS_GLYPH_NAMES
    /*
     * Add mapping from PS glyph names if available
     */
    if (FcFreeTypeUseNames (face))
    {
	FcChar8 name_buf[FC_GLYPHNAME_MAXLEN + 2];

	for (glyph = 0; glyph < face->num_glyphs; glyph++)
	{
	    if (FT_Get_Glyph_Name (face, glyph, name_buf, FC_GLYPHNAME_MAXLEN+1) == 0)
	    {
		ucs4 = FcGlyphNameToUcs4 (name_buf);
		if (ucs4 != 0xffff && 
		    FcFreeTypeCheckGlyph (face, ucs4, glyph, blanks, &advance))
		{
		    if (!has_advance)
		    {
			has_advance = FcTrue;
			advance_one = advance;
		    }
		    else if (!APPROXIMATELY_EQUAL (advance, advance_one))
                    {
                        if (fixed_advance)
                        {
                            dual_advance = FcTrue;
                            fixed_advance = FcFalse;
                            advance_two = advance;
                        }
                        else if (!APPROXIMATELY_EQUAL (advance, advance_two))
                            dual_advance = FcFalse;
                    }
		    leaf = FcCharSetFindLeafCreate (fcs, ucs4);
		    if (!leaf)
			goto bail1;
		    leaf->map[(ucs4 & 0xff) >> 5] |= (1 << (ucs4 & 0x1f));
#ifdef CHECK
		    if (ucs4 > font_max)
			font_max = ucs4;
#endif
		}
	    }
	}
    }
#endif
#ifdef CHECK
    printf ("%d glyphs %d encoded\n", (int) face->num_glyphs, FcCharSetCount (fcs));
    for (ucs4 = 0; ucs4 <= font_max; ucs4++)
    {
	FcBool	has_char = (glyph = FcFreeTypeCharIndex (face, ucs4)) != 0;
	FcBool	has_bit = FcCharSetHasChar (fcs, ucs4);

	if (has_char && !has_bit)
	{
	    if (!FcFreeTypeCheckGlyph (face, ucs4, glyph, blanks, &advance))
		printf ("Bitmap missing broken char 0x%x\n", ucs4);
	    else
		printf ("Bitmap missing char 0x%x\n", ucs4);
	}
	else if (!has_char && has_bit)
	    printf ("Bitmap extra char 0x%x\n", ucs4);
    }
#endif
    if (fixed_advance)
	*spacing = FC_MONO;
    else if (dual_advance && APPROXIMATELY_EQUAL (2 * FC_MIN (advance_one, advance_two), FC_MAX (advance_one, advance_two)))
        *spacing = FC_DUAL;
    else
	*spacing = FC_PROPORTIONAL;
    return fcs;
bail1:
    FcCharSetDestroy (fcs);
bail0:
    return 0;
}

FcCharSet *
FcFreeTypeCharSet (FT_Face face, FcBlanks *blanks)
{
    int spacing;

    return FcFreeTypeCharSetAndSpacing (face, blanks, &spacing);
}
