/*
 * $RCSId: xc/lib/fontconfig/src/fclang.c,v 1.7 2002/08/26 23:34:31 keithp Exp $
 *
 * Copyright © 2002 Keith Packard
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

#include <fcntl.h>
#include <sys/mman.h>
#include "fcint.h"

typedef struct {
    FcChar8	*lang;
    FcCharSet	charset;
} FcLangCharSet;

typedef struct {
    int begin;
    int end;
} FcLangCharSetRange;

#include "../fc-lang/fclang.h"

struct _FcLangSet {
    FcChar32	map[NUM_LANG_SET_MAP];
    FcStrSetPtr	extra;
};

#define FcLangSetBitSet(ls, id)	((ls)->map[(id)>>5] |= ((FcChar32) 1 << ((id) & 0x1f)))
#define FcLangSetBitGet(ls, id) (((ls)->map[(id)>>5] >> ((id) & 0x1f)) & 1)

FcLangSet *
FcFreeTypeLangSet (const FcCharSet  *charset, 
		   const FcChar8    *exclusiveLang)
{
    int		    i, j;
    FcChar32	    missing;
    const FcCharSet *exclusiveCharset = 0;
    FcLangSet	    *ls;
    

    if (exclusiveLang)
	exclusiveCharset = FcCharSetForLang (exclusiveLang);
    ls = FcLangSetCreate ();
    if (!ls)
	return 0;
    for (i = 0; i < NUM_LANG_CHAR_SET; i++)
    {
	/*
	 * Check for Han charsets to make fonts
	 * which advertise support for a single language
	 * not support other Han languages
	 */
	if (exclusiveCharset &&
	    FcFreeTypeIsExclusiveLang (fcLangCharSets[i].lang))
	{
	    if (fcLangCharSets[i].charset.num != exclusiveCharset->num)
		continue;

	    for (j = 0; j < fcLangCharSets[i].charset.num; j++)
		if (FcCharSetGetLeaf(&fcLangCharSets[i].charset, j) != 
		    FcCharSetGetLeaf(exclusiveCharset, j))
		    continue;
	}
	missing = FcCharSetSubtractCount (&fcLangCharSets[i].charset, charset);
        if (FcDebug() & FC_DBG_SCANV)
	{
	    if (missing && missing < 10)
	    {
		FcCharSet   *missed = FcCharSetSubtract (&fcLangCharSets[i].charset, 
							 charset);
		FcChar32    ucs4;
		FcChar32    map[FC_CHARSET_MAP_SIZE];
		FcChar32    next;

		printf ("\n%s(%d) ", fcLangCharSets[i].lang, missing);
		printf ("{");
		for (ucs4 = FcCharSetFirstPage (missed, map, &next);
		     ucs4 != FC_CHARSET_DONE;
		     ucs4 = FcCharSetNextPage (missed, map, &next))
		{
		    int	    i, j;
		    for (i = 0; i < FC_CHARSET_MAP_SIZE; i++)
			if (map[i])
			{
			    for (j = 0; j < 32; j++)
				if (map[i] & (1 << j))
				    printf (" %04x", ucs4 + i * 32 + j);
			}
		}
		printf (" }\n\t");
		FcCharSetDestroy (missed);
	    }
	    else
		printf ("%s(%d) ", fcLangCharSets[i].lang, missing);
	}
	if (!missing)
	    FcLangSetBitSet (ls, i);
    }

    if (FcDebug() & FC_DBG_SCANV)
	printf ("\n");
    
    
    return ls;
}

#define FcLangEnd(c)	((c) == '-' || (c) == '\0')

FcLangResult
FcLangCompare (const FcChar8 *s1, const FcChar8 *s2)
{
    FcChar8	    c1, c2;
    FcLangResult    result = FcLangDifferentLang;

    for (;;)
    {
	c1 = *s1++;
	c2 = *s2++;
	
	c1 = FcToLower (c1);
	c2 = FcToLower (c2);
	if (c1 != c2)
	{
	    if (FcLangEnd (c1) && FcLangEnd (c2))
		result = FcLangDifferentCountry;
	    return result;
	}
	else if (!c1)
	    return FcLangEqual;
	else if (c1 == '-')
	    result = FcLangDifferentCountry;
    }
}

/*
 * Return FcTrue when super contains sub. 
 *
 * super contains sub if super and sub have the same
 * language and either the same country or one
 * is missing the country
 */

static FcBool
FcLangContains (const FcChar8 *super, const FcChar8 *sub)
{
    FcChar8	    c1, c2;

    for (;;)
    {
	c1 = *super++;
	c2 = *sub++;
	
	c1 = FcToLower (c1);
	c2 = FcToLower (c2);
	if (c1 != c2)
	{
	    /* see if super has a country while sub is mising one */
	    if (c1 == '-' && c2 == '\0')
		return FcTrue;
	    /* see if sub has a country while super is mising one */
	    if (c1 == '\0' && c2 == '-')
		return FcTrue;
	    return FcFalse;
	}
	else if (!c1)
	    return FcTrue;
    }
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

FcLangSet *
FcLangSetCreate (void)
{
    FcLangSet	*ls;

    ls = malloc (sizeof (FcLangSet));
    if (!ls)
	return 0;
    FcMemAlloc (FC_MEM_LANGSET, sizeof (FcLangSet));
    memset (ls->map, '\0', sizeof (ls->map));
    ls->extra = FcStrSetPtrCreateDynamic(0);
    return ls;
}

void
FcLangSetPtrDestroy (FcLangSetPtr li)
{
    if (li.storage == FcStorageDynamic)
	FcLangSetDestroy(FcLangSetPtrU(li));
}

void
FcLangSetDestroy (FcLangSet *ls)
{
    if (FcStrSetPtrU(ls->extra))
	FcStrSetDestroy (FcStrSetPtrU(ls->extra));
    FcMemFree (FC_MEM_LANGSET, sizeof (FcLangSet));
    free (ls);
}

FcLangSet *
FcLangSetCopy (const FcLangSet *ls)
{
    FcLangSet	*new;

    new = FcLangSetCreate ();
    if (!new)
	goto bail0;
    memcpy (new->map, ls->map, sizeof (new->map));
    if (FcStrSetPtrU(ls->extra))
    {
	FcStrList	*list;
	FcChar8		*extra;
	
	new->extra = FcStrSetPtrCreateDynamic(FcStrSetCreate ());
	if (!FcStrSetPtrU(new->extra))
	    goto bail1;

	list = FcStrListCreate (FcStrSetPtrU(ls->extra));
	if (!list)
	    goto bail1;
	
	while ((extra = FcStrListNext (list)))
	    if (!FcStrSetAdd (FcStrSetPtrU(new->extra), extra))
	    {
		FcStrListDone (list);
		goto bail1;
	    }
	FcStrListDone (list);
    }
    return new;
bail1:
    FcLangSetDestroy (new);
bail0:
    return 0;
}

static int
FcLangSetIndex (const FcChar8 *lang)
{
    int	    low, high, mid = 0;
    int	    cmp = 0;
    FcChar8 firstChar = FcToLower(lang[0]); 
    FcChar8 secondChar = firstChar ? FcToLower(lang[1]) : '\0';
    
    if (firstChar < 'a')
    {
	low = 0;
	high = fcLangCharSetRanges[0].begin;
    }
    else if(firstChar > 'z')
    {
	low = fcLangCharSetRanges[25].begin;
	high = NUM_LANG_CHAR_SET - 1;
    }
    else
    {
	low = fcLangCharSetRanges[firstChar - 'a'].begin;
	high = fcLangCharSetRanges[firstChar - 'a'].end;
	/* no matches */
	if (low > high)
	    return -low; /* next entry after where it would be */
    }

    while (low <= high)
    {
	mid = (high + low) >> 1;
	if(fcLangCharSets[mid].lang[0] != firstChar)
	    cmp = FcStrCmpIgnoreCase(fcLangCharSets[mid].lang, lang);
	else
	{   /* fast path for resolving 2-letter languages (by far the most common) after
	     * finding the first char (probably already true because of the hash table) */
	    cmp = fcLangCharSets[mid].lang[1] - secondChar;
	    if (cmp == 0 && 
		(fcLangCharSets[mid].lang[2] != '\0' || 
		 lang[2] != '\0'))
	    {
		cmp = FcStrCmpIgnoreCase(fcLangCharSets[mid].lang+2, 
					 lang+2);
	    }
	}
	if (cmp == 0)
	    return mid;
	if (cmp < 0)
	    low = mid + 1;
	else
	    high = mid - 1;
    }
    if (cmp < 0)
	mid++;
    return -(mid + 1);
}

FcBool
FcLangSetAdd (FcLangSet *ls, const FcChar8 *lang)
{
    int	    id;

    id = FcLangSetIndex (lang);
    if (id >= 0)
    {
	FcLangSetBitSet (ls, id);
	return FcTrue;
    }
    if (!FcStrSetPtrU(ls->extra))
    {
	ls->extra = FcStrSetPtrCreateDynamic(FcStrSetCreate ());
	if (!FcStrSetPtrU(ls->extra))
	    return FcFalse;
    }
    return FcStrSetAdd (FcStrSetPtrU(ls->extra), lang);
}

FcLangResult
FcLangSetHasLang (const FcLangSet *ls, const FcChar8 *lang)
{
    int		    id;
    FcLangResult    best, r;
    int		    i;

    id = FcLangSetIndex (lang);
    if (id < 0)
	id = -id - 1;
    else if (FcLangSetBitGet (ls, id))
	return FcLangEqual;
    best = FcLangDifferentLang;
    for (i = id - 1; i >= 0; i--)
    {
	r = FcLangCompare (lang, fcLangCharSets[i].lang);
	if (r == FcLangDifferentLang)
	    break;
	if (FcLangSetBitGet (ls, i) && r < best)
	    best = r;
    }
    for (i = id; i < NUM_LANG_CHAR_SET; i++)
    {
	r = FcLangCompare (lang, fcLangCharSets[i].lang);
	if (r == FcLangDifferentLang)
	    break;
	if (FcLangSetBitGet (ls, i) && r < best)
	    best = r;
    }
    if (FcStrSetPtrU(ls->extra))
    {
	FcStrList	*list = FcStrListCreate (FcStrSetPtrU(ls->extra));
	FcChar8		*extra;
	FcLangResult	r;
	
	if (list)
	{
	    while (best > FcLangEqual && (extra = FcStrListNext (list)))
	    {
		r = FcLangCompare (lang, extra);
		if (r < best)
		    best = r;
	    }
	    FcStrListDone (list);
	}
    }
    return best;
}

static FcLangResult
FcLangSetCompareStrSet (const FcLangSet *ls, FcStrSet *set)
{
    FcStrList	    *list = FcStrListCreate (set);
    FcLangResult    r, best = FcLangDifferentLang;
    FcChar8	    *extra;

    if (list)
    {
	while (best > FcLangEqual && (extra = FcStrListNext (list)))
	{
	    r = FcLangSetHasLang (ls, extra);
	    if (r < best)
		best = r;
	}
	FcStrListDone (list);
    }
    return best;
}

FcLangResult
FcLangSetCompare (const FcLangSet *lsa, const FcLangSet *lsb)
{
    int		    i, j;
    FcLangResult    best, r;

    for (i = 0; i < NUM_LANG_SET_MAP; i++)
	if (lsa->map[i] & lsb->map[i])
	    return FcLangEqual;
    best = FcLangDifferentLang;
    for (j = 0; j < NUM_COUNTRY_SET; j++)
	for (i = 0; i < NUM_LANG_SET_MAP; i++)
	    if ((lsa->map[i] & fcLangCountrySets[j][i]) &&
		(lsb->map[i] & fcLangCountrySets[j][i]))
	    {
		best = FcLangDifferentCountry;
		break;
	    }
    if (FcStrSetPtrU(lsa->extra))
    {
	r = FcLangSetCompareStrSet (lsb, FcStrSetPtrU(lsa->extra));
	if (r < best)
	    best = r;
    }
    if (best > FcLangEqual && FcStrSetPtrU(lsb->extra))
    {
	r = FcLangSetCompareStrSet (lsa, FcStrSetPtrU(lsb->extra));
	if (r < best)
	    best = r;
    }
    return best;
}

/*
 * Used in computing values -- mustn't allocate any storage
 */
FcLangSet *
FcLangSetPromote (const FcChar8 *lang)
{
    static FcLangSet	ls;
    static FcStrSet	strs;
    static FcChar8	*str;
    int			id;

    memset (ls.map, '\0', sizeof (ls.map));
    ls.extra = FcStrSetPtrCreateDynamic(0);
    id = FcLangSetIndex (lang);
    if (id > 0)
    {
	FcLangSetBitSet (&ls, id);
    }
    else
    {
	ls.extra = FcStrSetPtrCreateDynamic(&strs);
	strs.num = 1;
	strs.size = 1;
	strs.storage = FcStorageDynamic;
	strs.u.strs = &str;
	strs.ref = 1;
	str = (FcChar8 *) lang;
    }
    return &ls;
}

FcChar32
FcLangSetHash (const FcLangSet *ls)
{
    FcChar32	h = 0;
    int		i;

    for (i = 0; i < NUM_LANG_SET_MAP; i++)
	h ^= ls->map[i];
    if (FcStrSetPtrU(ls->extra))
	h ^= FcStrSetPtrU(ls->extra)->num;
    return h;
}

FcLangSet *
FcNameParseLangSet (const FcChar8 *string)
{
    FcChar8	    lang[32],c;
    int i;
    FcLangSet	    *ls;

    ls = FcLangSetCreate ();
    if (!ls)
	goto bail0;

    for(;;)
    {
	for(i = 0; i < 31;i++)
	{
	    c = *string++;
	    if(c == '\0' || c == '|')
		break; /* end of this code */
	    lang[i] = c;
	}
	lang[i] = '\0';
	if (!FcLangSetAdd (ls, lang))
	    goto bail1;
	if(c == '\0')
	    break;
    }
    return ls;
bail1:
    FcLangSetDestroy (ls);
bail0:
    return 0;
}

FcBool
FcNameUnparseLangSet (FcStrBuf *buf, const FcLangSet *ls)
{
    int		i, bit;
    FcChar32	bits;
    FcBool	first = FcTrue;

    for (i = 0; i < NUM_LANG_SET_MAP; i++)
    {
	if ((bits = ls->map[i]))
	{
	    for (bit = 0; bit <= 31; bit++)
		if (bits & (1 << bit))
		{
		    int id = (i << 5) | bit;
		    if (!first)
			if (!FcStrBufChar (buf, '|'))
			    return FcFalse;
		    if (!FcStrBufString (buf, fcLangCharSets[id].lang))
			return FcFalse;
		    first = FcFalse;
		}
	}
    }
    if (FcStrSetPtrU(ls->extra))
    {
	FcStrList   *list = FcStrListCreate (FcStrSetPtrU(ls->extra));
	FcChar8	    *extra;

	if (!list)
	    return FcFalse;
	while ((extra = FcStrListNext (list)))
	{
	    if (!first)
		if (!FcStrBufChar (buf, '|'))
		    return FcFalse;
	    if (!FcStrBufString (buf, extra))
		return FcFalse;
	    first = FcFalse;
	}
    }
    return FcTrue;
}

FcBool
FcLangSetEqual (const FcLangSet *lsa, const FcLangSet *lsb)
{
    int	    i;

    for (i = 0; i < NUM_LANG_SET_MAP; i++)
    {
	if (lsa->map[i] != lsb->map[i])
	    return FcFalse;
    }
    if (!FcStrSetPtrU(lsa->extra) && !FcStrSetPtrU(lsb->extra))
	return FcTrue;
    if (FcStrSetPtrU(lsa->extra) && FcStrSetPtrU(lsb->extra))
	return FcStrSetEqual (FcStrSetPtrU(lsa->extra), FcStrSetPtrU(lsb->extra));
    return FcFalse;
}

static FcBool
FcLangSetContainsLang (const FcLangSet *ls, const FcChar8 *lang)
{
    int		    id;
    int		    i;

    id = FcLangSetIndex (lang);
    if (id < 0)
	id = -id - 1;
    else if (FcLangSetBitGet (ls, id))
	return FcTrue;
    /*
     * search up and down among equal languages for a match
     */
    for (i = id - 1; i >= 0; i--)
    {
	if (FcLangCompare (fcLangCharSets[i].lang, lang) == FcLangDifferentLang)
	    break;
	if (FcLangSetBitGet (ls, i) &&
	    FcLangContains (fcLangCharSets[i].lang, lang))
	    return FcTrue;
    }
    for (i = id; i < NUM_LANG_CHAR_SET; i++)
    {
	if (FcLangCompare (fcLangCharSets[i].lang, lang) == FcLangDifferentLang)
	    break;
	if (FcLangSetBitGet (ls, i) &&
	    FcLangContains (fcLangCharSets[i].lang, lang))
	    return FcTrue;
    }
    if (FcStrSetPtrU(ls->extra))
    {
	FcStrList	*list = FcStrListCreate (FcStrSetPtrU(ls->extra));
	FcChar8		*extra;
	
	if (list)
	{
	    while ((extra = FcStrListNext (list)))
	    {
		if (FcLangContains (extra, lang))
		    break;
	    }
	    FcStrListDone (list);
    	    if (extra)
		return FcTrue;
	}
    }
    return FcFalse;
}

/*
 * return FcTrue if lsa contains every language in lsb
 */
FcBool
FcLangSetContains (const FcLangSet *lsa, const FcLangSet *lsb)
{
    int		    i, j;
    FcChar32	    missing;

    if (FcDebug() & FC_DBG_MATCHV)
    {
	printf ("FcLangSet "); FcLangSetPrint (lsa);
	printf (" contains "); FcLangSetPrint (lsb);
	printf ("\n");
    }
    /*
     * check bitmaps for missing language support
     */
    for (i = 0; i < NUM_LANG_SET_MAP; i++)
    {
	missing = lsb->map[i] & ~lsa->map[i];
	if (missing)
	{
	    for (j = 0; j < 32; j++)
		if (missing & (1 << j)) 
		{
		    if (!FcLangSetContainsLang (lsa,
						fcLangCharSets[i*32 + j].lang))
		    {
			if (FcDebug() & FC_DBG_MATCHV)
			    printf ("\tMissing bitmap %s\n", fcLangCharSets[i*32+j].lang);
			return FcFalse;
		    }
		}
	}
    }
    if (FcStrSetPtrU(lsb->extra))
    {
	FcStrList   *list = FcStrListCreate (FcStrSetPtrU(lsb->extra));
	FcChar8	    *extra;

	if (list)
	{
	    while ((extra = FcStrListNext (list)))
	    {
		if (!FcLangSetContainsLang (lsa, extra))
		{
		    if (FcDebug() & FC_DBG_MATCHV)
			printf ("\tMissing string %s\n", extra);
		    break;
		}
	    }
	    FcStrListDone (list);
	    if (extra)
		return FcFalse;
	}
    }
    return FcTrue;
}

static FcLangSet * langsets = 0;
static int langset_ptr = 0, langset_count = 0;

FcLangSet *
FcLangSetPtrU (FcLangSetPtr li)
{
    switch (li.storage)
    {
    case FcStorageDynamic:
	return li.u.dyn;
    case FcStorageStatic:
	return &langsets[li.u.stat];
    default:
	return 0;

    }
}

FcLangSetPtr
FcLangSetPtrCreateDynamic (FcLangSet *li)
{
    FcLangSetPtr new;
    new.storage = FcStorageDynamic;
    new.u.dyn = li;
    return new;
}

void
FcLangSetClearStatic (void)
{
    FcStrSetClearStatic();
    langset_ptr = 0;
    langset_count = 0;
}

/* should only write one copy of any particular FcLangSet */
FcBool
FcLangSetPrepareSerialize (FcLangSet *l)
{
    langset_count++;
    if (l && FcStrSetPtrU(l->extra))
	return FcStrSetPrepareSerialize (FcStrSetPtrU(l->extra));
    return FcTrue;
}

FcLangSetPtr
FcLangSetSerialize(FcLangSet *l)
{
    FcLangSetPtr new;
    int p = langset_ptr;

    if (!l) return FcLangSetPtrCreateDynamic(0);

    if (!langsets)
    {
	FcLangSet* t;
	t = (FcLangSet *)malloc(langset_count * sizeof(FcLangSet));
	if (!t)
	    return FcLangSetPtrCreateDynamic(0);
	langsets = t;
	langset_ptr = 0;
    }

    langsets[langset_ptr] = *l;
    if (FcStrSetPtrU(l->extra))
	langsets[langset_ptr].extra = 
	    FcStrSetSerialize(FcStrSetPtrU(l->extra));
    else
	langsets[langset_ptr].extra = FcStrSetPtrCreateDynamic(0);
    langset_ptr++;
    new.storage = FcStorageStatic;
    new.u.stat = p;
    return new;
}

FcBool
FcLangSetWrite (int fd, FcCache *metadata)
{
    metadata->langsets_length = langset_ptr;
    metadata->langsets_offset = FcCacheNextOffset(fd);

    if (langset_ptr > 0)
    {
	lseek (fd, metadata->langsets_offset, SEEK_SET);
	return write(fd, langsets, 
		     metadata->langsets_length * sizeof(FcLangSet)) != -1;
    }
    return FcTrue;
}

FcBool
FcLangSetRead (int fd, FcCache metadata)
{
    langsets = mmap(NULL, 
		    metadata.langsets_length * sizeof (FcLangSet),
		    PROT_READ,
		    MAP_SHARED, fd, metadata.langsets_offset);
    if (langsets == MAP_FAILED)
	return FcFalse;
    langset_count = langset_ptr = metadata.langsets_length;
    return FcTrue;
}
