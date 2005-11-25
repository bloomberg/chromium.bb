/*
 * $RCSId: xc/lib/fontconfig/src/fcmatch.c,v 1.20 2002/08/31 22:17:32 keithp Exp $
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

#include <string.h>
#include <ctype.h>
#include "fcint.h"
#include <stdio.h>

static double
FcCompareNumber (FcValue *value1, FcValue *value2)
{
    double  v1, v2, v;
    
    switch (value1->type) {
    case FcTypeInteger:
	v1 = (double) value1->u.i;
	break;
    case FcTypeDouble:
	v1 = value1->u.d;
	break;
    default:
	return -1.0;
    }
    switch (value2->type) {
    case FcTypeInteger:
	v2 = (double) value2->u.i;
	break;
    case FcTypeDouble:
	v2 = value2->u.d;
	break;
    default:
	return -1.0;
    }
    v = v2 - v1;
    if (v < 0)
	v = -v;
    return v;
}

static double
FcCompareString (FcValue *v1, FcValue *v2)
{
    return (double) FcStrCmpIgnoreCase (fc_value_string(v1), fc_value_string(v2)) != 0;
}

static double
FcCompareFamily (FcValue *v1, FcValue *v2)
{
    /* rely on the guarantee in FcPatternAddWithBinding that
     * families are always FcTypeString. */
    const FcChar8* v1_string = fc_value_string(v1);
    const FcChar8* v2_string = fc_value_string(v2);

    if (FcToLower(*v1_string) != FcToLower(*v2_string))
       return 1.0;

    return (double) FcStrCmpIgnoreBlanksAndCase (v1_string, v2_string) != 0;
}

static double
FcCompareLang (FcValue *v1, FcValue *v2)
{
    FcLangResult    result;
    FcValue value1 = FcValueCanonicalize(v1), value2 = FcValueCanonicalize(v2);
    
    switch (value1.type) {
    case FcTypeLangSet:
	switch (value2.type) {
	case FcTypeLangSet:
	    result = FcLangSetCompare (value1.u.l, value2.u.l);
	    break;
	case FcTypeString:
	    result = FcLangSetHasLang (value1.u.l, 
				       value2.u.s);
	    break;
	default:
	    return -1.0;
	}
	break;
    case FcTypeString:
	switch (value2.type) {
	case FcTypeLangSet:
	    result = FcLangSetHasLang (value2.u.l, value1.u.s);
	    break;
	case FcTypeString:
	    result = FcLangCompare (value1.u.s, 
				    value2.u.s);
	    break;
	default:
	    return -1.0;
	}
	break;
    default:
	return -1.0;
    }
    switch (result) {
    case FcLangEqual:
	return 0;
    case FcLangDifferentCountry:
	return 1;
    case FcLangDifferentLang:
    default:
	return 2;
    }
}

static double
FcCompareBool (FcValue *v1, FcValue *v2)
{
    if (fc_storage_type(v2) != FcTypeBool || fc_storage_type(v1) != FcTypeBool)
	return -1.0;
    return (double) v2->u.b != v1->u.b;
}

static double
FcCompareCharSet (FcValue *v1, FcValue *v2)
{
    return (double) FcCharSetSubtractCount (fc_value_charset(v1), fc_value_charset(v2));
}

static double
FcCompareSize (FcValue *value1, FcValue *value2)
{
    double  v1, v2, v;

    switch (value1->type) {
    case FcTypeInteger:
	v1 = value1->u.i;
	break;
    case FcTypeDouble:
	v1 = value1->u.d;
	break;
    default:
	return -1;
    }
    switch (value2->type) {
    case FcTypeInteger:
	v2 = value2->u.i;
	break;
    case FcTypeDouble:
	v2 = value2->u.d;
	break;
    default:
	return -1;
    }
    if (v2 == 0)
	return 0;
    v = v2 - v1;
    if (v < 0)
	v = -v;
    return v;
}

typedef struct _FcMatcher {
    const char	    *object;
    double	    (*compare) (FcValue *value1, FcValue *value2);
    int		    strong, weak;
} FcMatcher;

/*
 * Order is significant, it defines the precedence of
 * each value, earlier values are more significant than
 * later values
 */
static FcMatcher _FcMatchers [] = {
    { FC_FOUNDRY,	FcCompareString,	0, 0 },
#define MATCH_FOUNDRY	    0
#define MATCH_FOUNDRY_INDEX 0
    
    { FC_CHARSET,	FcCompareCharSet,	1, 1 },
#define MATCH_CHARSET	    1
#define MATCH_CHARSET_INDEX 1
    
    { FC_FAMILY,    	FcCompareFamily,	2, 4 },
#define MATCH_FAMILY	    2
#define MATCH_FAMILY_STRONG_INDEX   2
#define MATCH_FAMILY_WEAK_INDEX	    4
    
    { FC_LANG,		FcCompareLang,		3, 3 },
#define MATCH_LANG	    3
#define MATCH_LANG_INDEX    3
    
    { FC_SPACING,	FcCompareNumber,	5, 5 },
#define MATCH_SPACING	    4
#define MATCH_SPACING_INDEX 5
    
    { FC_PIXEL_SIZE,	FcCompareSize,		6, 6 },
#define MATCH_PIXEL_SIZE    5
#define MATCH_PIXEL_SIZE_INDEX	6
    
    { FC_STYLE,		FcCompareString,	7, 7 },
#define MATCH_STYLE	    6
#define MATCH_STYLE_INDEX   7
    
    { FC_SLANT,		FcCompareNumber,	8, 8 },
#define MATCH_SLANT	    7
#define MATCH_SLANT_INDEX   8
    
    { FC_WEIGHT,	FcCompareNumber,	9, 9 },
#define MATCH_WEIGHT	    8
#define MATCH_WEIGHT_INDEX  9
    
    { FC_WIDTH,		FcCompareNumber,	10, 10 },
#define MATCH_WIDTH	    9
#define MATCH_WIDTH_INDEX   10
    
    { FC_ANTIALIAS,	FcCompareBool,		11, 11 },
#define MATCH_ANTIALIAS	    10
#define MATCH_ANTIALIAS_INDEX	    11
    
    { FC_RASTERIZER,	FcCompareString,	12, 12 },
#define MATCH_RASTERIZER    11
#define MATCH_RASTERIZER_INDEX    12

    { FC_OUTLINE,	FcCompareBool,		13, 13 },
#define MATCH_OUTLINE	    12
#define MATCH_OUTLINE_INDEX	    13

    { FC_FONTVERSION,	FcCompareNumber,	14, 14 },
#define MATCH_FONTVERSION   13
#define MATCH_FONTVERSION_INDEX   14
};

#define NUM_MATCH_VALUES    15

static FcBool
FcCompareValueList (const char  *object,
		    FcValueListPtr v1orig,	/* pattern */
		    FcValueListPtr v2orig,	/* target */
		    FcValue	*bestValue,
		    double	*value,
		    FcResult	*result)
{
    FcValueListPtr  v1, v2;
    FcValueList     *v1_ptrU, *v2_ptrU;
    double    	    v, best, bestStrong, bestWeak;
    int		    i;
    int		    j;
    
    /*
     * Locate the possible matching entry by examining the
     * first few characters in object
     */
    i = -1;
    switch (FcToLower (object[0])) {
    case 'f':
	switch (FcToLower (object[1])) {
	case 'o':
	    switch (FcToLower (object[2])) {
	    case 'u':
		i = MATCH_FOUNDRY; break;
	    case 'n':
		i = MATCH_FONTVERSION; break;
	    }
	    break;
	case 'a':
	    i = MATCH_FAMILY; break;
	}
	break;
    case 'c':
	i = MATCH_CHARSET; break;
    case 'a':
	i = MATCH_ANTIALIAS; break;
    case 'l':
	i = MATCH_LANG; break;
    case 's':
	switch (FcToLower (object[1])) {
	case 'p':
	    i = MATCH_SPACING; break;
	case 't':
	    i = MATCH_STYLE; break;
	case 'l':
	    i = MATCH_SLANT; break;
	}
	break;
    case 'p':
	i = MATCH_PIXEL_SIZE; break;
    case 'w':
	switch (FcToLower (object[1])) {
	case 'i':
	    i = MATCH_WIDTH; break;
	case 'e':
	    i = MATCH_WEIGHT; break;
	}
	break;
    case 'r':
	i = MATCH_RASTERIZER; break;
    case 'o':
	i = MATCH_OUTLINE; break;
    }
    if (i == -1 || 
	FcStrCmpIgnoreCase ((FcChar8 *) _FcMatchers[i].object,
			    (FcChar8 *) object) != 0)
    {
	if (bestValue)
	    *bestValue = FcValueCanonicalize(&FcValueListPtrU(v2orig)->value);
	return FcTrue;
    }
#if 0
    for (i = 0; i < NUM_MATCHER; i++)
    {
	if (!FcStrCmpIgnoreCase ((FcChar8 *) _FcMatchers[i].object,
				 (FcChar8 *) object))
	    break;
    }
    if (i == NUM_MATCHER)
    {
	if (bestValue)
	    *bestValue = v2orig->value;
	return FcTrue;
    }
#endif
    best = 1e99;
    bestStrong = 1e99;
    bestWeak = 1e99;
    j = 0;
    for (v1 = v1orig, v1_ptrU = FcValueListPtrU(v1); v1_ptrU;
	 v1 = FcValueListPtrU(v1)->next, v1_ptrU = FcValueListPtrU(v1))
    {
	for (v2 = v2orig, v2_ptrU = FcValueListPtrU(v2); FcValueListPtrU(v2); 
	     v2 = FcValueListPtrU(v2)->next)
	{
	    v = (*_FcMatchers[i].compare) (&v1_ptrU->value,
					   &v2_ptrU->value);
	    if (v < 0)
	    {
		*result = FcResultTypeMismatch;
		return FcFalse;
	    }
	    if (FcDebug () & FC_DBG_MATCHV)
		printf (" v %g j %d ", v, j);
	    v = v * 100 + j;
	    if (v < best)
	    {
		if (bestValue)
		    *bestValue = FcValueCanonicalize(&v2_ptrU->value);
		best = v;
	    }
	    if (v1_ptrU->binding == FcValueBindingStrong)
	    {
		if (v < bestStrong)
		    bestStrong = v;
	    }
	    else
	    {
		if (v < bestWeak)
		    bestWeak = v;
	    }
	}
	j++;
    }
    if (FcDebug () & FC_DBG_MATCHV)
    {
	printf (" %s: %g ", object, best);
	FcValueListPrint (v1orig);
	printf (", ");
	FcValueListPrint (v2orig);
	printf ("\n");
    }
    if (value)
    {
	int weak    = _FcMatchers[i].weak;
	int strong  = _FcMatchers[i].strong;
	if (weak == strong)
	    value[strong] += best;
	else
	{
	    value[weak] += bestWeak;
	    value[strong] += bestStrong;
	}
    }
    return FcTrue;
}

/*
 * Return a value indicating the distance between the two lists of
 * values
 */

static FcBool
FcCompare (FcPattern	*pat,
	   FcPattern	*fnt,
	   double	*value,
	   FcResult	*result)
{
    int		    i, i1, i2;
    
    for (i = 0; i < NUM_MATCH_VALUES; i++)
	value[i] = 0.0;
    
    i1 = 0;
    i2 = 0;
    while (i1 < pat->num && i2 < fnt->num)
    {
	FcPatternElt *elt_i1 = FcPatternEltU(pat->elts)+i1;
	FcPatternElt *elt_i2 = FcPatternEltU(fnt->elts)+i2;

	i = FcObjectPtrCompare(elt_i1->object, elt_i2->object);
	if (i > 0)
	    i2++;
	else if (i < 0)
	    i1++;
	else
	{
	    if (!FcCompareValueList (FcObjectPtrU(elt_i1->object), 
				     elt_i1->values, elt_i2->values,
				     0, value, result))
		return FcFalse;
	    i1++;
	    i2++;
	}
    }
    return FcTrue;
}

FcPattern *
FcFontRenderPrepare (FcConfig	    *config,
		     FcPattern	    *pat,
		     FcPattern	    *font)
{
    FcPattern	    *new;
    int		    i;
    FcPatternElt    *fe, *pe;
    FcValue	    v;
    FcResult	    result;
    
    new = FcPatternCreate ();
    if (!new)
	return 0;
    for (i = 0; i < font->num; i++)
    {
	fe = FcPatternEltU(font->elts)+i;
	pe = FcPatternFindElt (pat, FcObjectPtrU(fe->object));
	if (pe)
	{
	    if (!FcCompareValueList (FcObjectPtrU(pe->object), pe->values, 
				     fe->values, &v, 0, &result))
	    {
		FcPatternDestroy (new);
		return 0;
	    }
	}
	else
	    v = FcValueCanonicalize(&FcValueListPtrU(fe->values)->value);
	FcPatternAdd (new, FcObjectPtrU(fe->object), v, FcFalse);
    }
    for (i = 0; i < pat->num; i++)
    {
	pe = FcPatternEltU(pat->elts)+i;
	fe = FcPatternFindElt (font, FcObjectPtrU(pe->object));
	if (!fe)
	    FcPatternAdd (new, FcObjectPtrU(pe->object), 
                          FcValueCanonicalize(&FcValueListPtrU(pe->values)->value), FcTrue);
    }

    if (FcPatternFindElt (font, FC_FILE))
	FcPatternTransferFullFname (new, font);

    FcConfigSubstituteWithPat (config, new, pat, FcMatchFont);
    return new;
}

FcPattern *
FcFontSetMatch (FcConfig    *config,
		FcFontSet   **sets,
		int	    nsets,
		FcPattern   *p,
		FcResult    *result)
{
    double    	    score[NUM_MATCH_VALUES], bestscore[NUM_MATCH_VALUES];
    int		    f;
    FcFontSet	    *s;
    FcPattern	    *best;
    int		    i;
    int		    set;

    for (i = 0; i < NUM_MATCH_VALUES; i++)
	bestscore[i] = 0;
    best = 0;
    if (FcDebug () & FC_DBG_MATCH)
    {
	printf ("Match ");
	FcPatternPrint (p);
    }
    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	{
	    *result = FcResultOutOfMemory;
	    return 0;
	}
    }
    for (set = 0; set < nsets; set++)
    {
	s = sets[set];
	if (!s)
	    continue;
	for (f = 0; f < s->nfont; f++)
	{
	    if (FcDebug () & FC_DBG_MATCHV)
	    {
		printf ("Font %d ", f);
		FcPatternPrint (s->fonts[f]);
	    }
	    if (!FcCompare (p, s->fonts[f], score, result))
		return 0;
	    if (FcDebug () & FC_DBG_MATCHV)
	    {
		printf ("Score");
		for (i = 0; i < NUM_MATCH_VALUES; i++)
		{
		    printf (" %g", score[i]);
		}
		printf ("\n");
	    }
	    for (i = 0; i < NUM_MATCH_VALUES; i++)
	    {
		if (best && bestscore[i] < score[i])
		    break;
		if (!best || score[i] < bestscore[i])
		{
		    for (i = 0; i < NUM_MATCH_VALUES; i++)
			bestscore[i] = score[i];
		    best = s->fonts[f];
		    break;
		}
	    }
	}
    }
    if (FcDebug () & FC_DBG_MATCH)
    {
	printf ("Best score");
	for (i = 0; i < NUM_MATCH_VALUES; i++)
	    printf (" %g", bestscore[i]);
	FcPatternPrint (best);
    }
    if (!best)
    {
	*result = FcResultNoMatch;
	return 0;
    }
    return FcFontRenderPrepare (config, p, best);
}

FcPattern *
FcFontMatch (FcConfig	*config,
	     FcPattern	*p, 
	     FcResult	*result)
{
    FcFontSet	*sets[2];
    int		nsets;

    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    nsets = 0;
    if (config->fonts[FcSetSystem])
	sets[nsets++] = config->fonts[FcSetSystem];
    if (config->fonts[FcSetApplication])
	sets[nsets++] = config->fonts[FcSetApplication];
    return FcFontSetMatch (config, sets, nsets, p, result);
}

typedef struct _FcSortNode {
    FcPattern	*pattern;
    double	score[NUM_MATCH_VALUES];
} FcSortNode;

static int
FcSortCompare (const void *aa, const void *ab)
{
    FcSortNode  *a = *(FcSortNode **) aa;
    FcSortNode  *b = *(FcSortNode **) ab;
    double	*as = &a->score[0];
    double	*bs = &b->score[0];
    double	ad = 0, bd = 0;
    int         i;

    i = NUM_MATCH_VALUES;
    while (i-- && (ad = *as++) == (bd = *bs++))
	;
    return ad < bd ? -1 : ad > bd ? 1 : 0;
}

static FcBool
FcSortWalk (FcSortNode **n, int nnode, FcFontSet *fs, FcCharSet **cs, FcBool trim)
{
    FcCharSet	*ncs;
    FcSortNode	*node;

    while (nnode--)
    {
	node = *n++;
	if (FcPatternGetCharSet (node->pattern, FC_CHARSET, 0, &ncs) == 
	    FcResultMatch)
	{
	    /*
	     * If this font isn't a subset of the previous fonts,
	     * add it to the list
	     */
	    if (!trim || !*cs || !FcCharSetIsSubset (ncs, *cs))
	    {
		if (*cs)
		{
		    ncs = FcCharSetUnion (ncs, *cs);
		    if (!ncs)
			return FcFalse;
		    FcCharSetDestroy (*cs);
		}
		else
		    ncs = FcCharSetCopy (ncs);
		*cs = ncs;
		FcPatternReference (node->pattern);
		if (FcDebug () & FC_DBG_MATCH)
		{
		    printf ("Add ");
		    FcPatternPrint (node->pattern);
		}
		if (!FcFontSetAdd (fs, node->pattern))
		{
		    FcPatternDestroy (node->pattern);
		    return FcFalse;
		}
	    }
	}
    }
    return FcTrue;
}

void
FcFontSetSortDestroy (FcFontSet *fs)
{
    FcFontSetDestroy (fs);
}

FcFontSet *
FcFontSetSort (FcConfig	    *config,
	       FcFontSet    **sets,
	       int	    nsets,
	       FcPattern    *p,
	       FcBool	    trim,
	       FcCharSet    **csp,
	       FcResult	    *result)
{
    FcFontSet	    *ret;
    FcFontSet	    *s;
    FcSortNode	    *nodes;
    FcSortNode	    **nodeps, **nodep;
    int		    nnodes;
    FcSortNode	    *new;
    FcCharSet	    *cs;
    int		    set;
    int		    f;
    int		    i;
    int		    nPatternLang;
    FcBool    	    *patternLangSat;
    FcValue	    patternLang;

    if (FcDebug () & FC_DBG_MATCH)
    {
	printf ("Sort ");
	FcPatternPrint (p);
    }
    nnodes = 0;
    for (set = 0; set < nsets; set++)
    {
	s = sets[set];
	if (!s)
	    continue;
	nnodes += s->nfont;
    }
    if (!nnodes)
	goto bail0;
    
    for (nPatternLang = 0;
	 FcPatternGet (p, FC_LANG, nPatternLang, &patternLang) == FcResultMatch;
	 nPatternLang++)
	;
	
    /* freed below */
    nodes = malloc (nnodes * sizeof (FcSortNode) + 
		    nnodes * sizeof (FcSortNode *) +
		    nPatternLang * sizeof (FcBool));
    if (!nodes)
	goto bail0;
    nodeps = (FcSortNode **) (nodes + nnodes);
    patternLangSat = (FcBool *) (nodeps + nnodes);
    
    new = nodes;
    nodep = nodeps;
    for (set = 0; set < nsets; set++)
    {
	s = sets[set];
	if (!s)
	    continue;
	for (f = 0; f < s->nfont; f++)
	{
	    if (FcDebug () & FC_DBG_MATCHV)
	    {
		printf ("Font %d ", f);
		FcPatternPrint (s->fonts[f]);
	    }
	    new->pattern = s->fonts[f];
	    if (!FcCompare (p, new->pattern, new->score, result))
		goto bail1;
	    if (FcDebug () & FC_DBG_MATCHV)
	    {
		printf ("Score");
		for (i = 0; i < NUM_MATCH_VALUES; i++)
		{
		    printf (" %g", new->score[i]);
		}
		printf ("\n");
	    }
	    *nodep = new;
	    new++;
	    nodep++;
	}
    }

    nnodes = new - nodes;
    
    qsort (nodeps, nnodes, sizeof (FcSortNode *),
	   FcSortCompare);
    
    for (i = 0; i < nPatternLang; i++)
	patternLangSat[i] = FcFalse;
    
    for (f = 0; f < nnodes; f++)
    {
	FcBool	satisfies = FcFalse;
	/*
	 * If this node matches any language, go check
	 * which ones and satisfy those entries
	 */
	if (nodeps[f]->score[MATCH_LANG_INDEX] < nPatternLang)
	{
	    for (i = 0; i < nPatternLang; i++)
	    {
		FcValue	    nodeLang;
		
		if (!patternLangSat[i] &&
		    FcPatternGet (p, FC_LANG, i, &patternLang) == FcResultMatch &&
		    FcPatternGet (nodeps[f]->pattern, FC_LANG, 0, &nodeLang) == FcResultMatch)
		{
		    double  compare = FcCompareLang (&patternLang, &nodeLang);
		    if (compare >= 0 && compare < 2)
		    {
			if (FcDebug () & FC_DBG_MATCHV)
			{
			    FcChar8 *family;
			    FcChar8 *style;

			    if (FcPatternGetString (nodeps[f]->pattern, FC_FAMILY, 0, &family) == FcResultMatch &&
				FcPatternGetString (nodeps[f]->pattern, FC_STYLE, 0, &style) == FcResultMatch)
				printf ("Font %s:%s matches language %d\n", family, style, i);
			}
			patternLangSat[i] = FcTrue;
			satisfies = FcTrue;
			break;
		    }
		}
	    }
	}
	if (!satisfies)
	    nodeps[f]->score[MATCH_LANG_INDEX] = 1000.0;
    }

    /*
     * Re-sort once the language issues have been settled
     */
    qsort (nodeps, nnodes, sizeof (FcSortNode *),
	   FcSortCompare);

    ret = FcFontSetCreate ();
    if (!ret)
	goto bail1;

    cs = 0;

    if (!FcSortWalk (nodeps, nnodes, ret, &cs, trim))
	goto bail2;

    if (csp)
	*csp = cs;
    else
	FcCharSetDestroy (cs);

    free (nodes);

    return ret;

bail2:
    if (cs)
	FcCharSetDestroy (cs);
    FcFontSetDestroy (ret);
bail1:
    free (nodes);
bail0:
    return 0;
}

FcFontSet *
FcFontSort (FcConfig	*config,
	    FcPattern	*p, 
	    FcBool	trim,
	    FcCharSet	**csp,
	    FcResult	*result)
{
    FcFontSet	*sets[2];
    int		nsets;

    if (!config)
    {
	config = FcConfigGetCurrent ();
	if (!config)
	    return 0;
    }
    nsets = 0;
    if (config->fonts[FcSetSystem])
	sets[nsets++] = config->fonts[FcSetSystem];
    if (config->fonts[FcSetApplication])
	sets[nsets++] = config->fonts[FcSetApplication];
    return FcFontSetSort (config, sets, nsets, p, trim, csp, result);
}
