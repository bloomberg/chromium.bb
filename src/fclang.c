/*
 * $XFree86: xc/lib/fontconfig/fc-lang/fclang.tmpl.c,v 1.1 2002/07/06 23:21:36 keithp Exp $
 *
 * Copyright © 2002 Keith Packard, member of The XFree86 Project, Inc.
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

typedef struct {
    FcChar8	*lang;
    FcCharSet	charset;
} FcLangCharSet;

#include "../fc-lang/fclang.h"

#define NUM_LANG_CHAR_SET	(sizeof (fcLangCharSets) / sizeof (fcLangCharSets[0]))
						 
FcBool
FcFreeTypeSetLang (FcPattern	    *pattern, 
		   FcCharSet	    *charset, 
		   const FcChar8    *exclusiveLang)
{
    int		    i;
    FcChar32	    missing;
    FcBool	    hasLang = FcFalse;
    const FcCharSet *exclusiveCharset = 0;

    if (exclusiveLang)
	exclusiveCharset = FcCharSetForLang (exclusiveLang);
    for (i = 0; i < NUM_LANG_CHAR_SET; i++)
    {
	/*
	 * Check for Han charsets to make fonts
	 * which advertise support for a single language
	 * not support other Han languages
	 */
	if (exclusiveCharset &&
	    FcFreeTypeIsExclusiveLang (fcLangCharSets[i].lang) &&
	    fcLangCharSets[i].charset.leaves != exclusiveCharset->leaves)
	{
	    continue;
	}
	missing = FcCharSetSubtractCount (&fcLangCharSets[i].charset, charset);
        if (FcDebug() & FC_DBG_SCANV)
	    printf ("%s(%d) ", fcLangCharSets[i].lang, missing);
	if (!missing)
	{
	    if (!FcPatternAddString (pattern, FC_LANG, fcLangCharSets[i].lang))
		return FcFalse;
	    hasLang = FcTrue;
	}
    }
    /*
     * Make sure it has a lang entry
     */
    if (!hasLang)
	if (!FcPatternAddString (pattern, FC_LANG, (FcChar8 *) "x-unknown"))
	    return FcFalse;

    if (FcDebug() & FC_DBG_SCANV)
	printf ("\n");
    return FcTrue;
}


FcLangResult
FcLangCompare (const FcChar8 *s1, const FcChar8 *s2)
{
    const FcChar8   *orig_s1 = s1;
    FcChar8	    c1, c2;
    FcLangResult    result;
    /*
     * Compare ISO 639 language codes
     */
    for (;;)
    {
	c1 = *s1++;
	c2 = *s2++;
	if (c1 == '\0' || c1 == '-')
	    break;
	if (c2 == '\0' || c2 == '-')
	    break;
	c1 = FcToLower (c1);
	c2 = FcToLower (c2);
	if (c1 != c2)
	    return FcLangDifferentLang;	    /* mismatching lang code */
    }
    if (!c1 && !c2)
	return FcLangEqual;
    /*
     * Make x-* mismatch as if the lang part didn't match
     */
    result = FcLangDifferentCountry;
    if (orig_s1[0] == 'x' && (orig_s1[1] == '\0' || orig_s1[1] == '-'))
	result = FcLangDifferentLang;
    
    if (c1 == '\0' || c2 == '\0')
	return result;
    /*
     * Compare ISO 3166 country codes
     */
    for (;;)
    {
	c1 = *s1++;
	c2 = *s2++;
	if (!c1 || !c2)
	    break;
	c1 = FcToLower (c1);
	c2 = FcToLower (c2);
	if (c1 != c2)
	    break;
    }
    if (c1 == c2)
	return FcLangEqual;
    else
	return result;
}

const FcCharSet *
FcCharSetForLang (const FcChar8 *lang)
{
    int		i;
    int		country = -1;
    for (i = 0; i < NUM_LANG_CHAR_SET; i++)
    {
	switch (FcLangCompare (lang, fcLangCharSets[i].lang)) {
	case FcLangEqual:
	    return &fcLangCharSets[i].charset;
	case FcLangDifferentCountry:
	    if (country == -1)
		country = i;
	default:
	    break;
	}
    }
    if (country == -1)
	return 0;
    return &fcLangCharSets[i].charset;
}
