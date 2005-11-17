/*
 * $RCSId: xc/lib/fontconfig/src/fccharset.c,v 1.18 2002/08/22 07:36:44 keithp Exp $
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

#include <stdlib.h>
#include "fcint.h"

/* #define CHECK */

/* #define CHATTY */

static FcCharSet ** charsets = 0;
static FcChar16 ** numbers = 0;
static int charset_bank_count = 0, charset_ptr, charset_count;
static int charset_numbers_ptr, charset_numbers_count;
static FcCharLeaf ** leaves = 0;
static int charset_leaf_ptr, charset_leaf_count;
static int ** leaf_idx = 0;
static int charset_leaf_idx_ptr, charset_leaf_idx_count;

extern const FcChar16 *langBankNumbers;
extern const FcCharLeaf	*langBankLeaves;
extern const int *langBankLeafIdx;

static FcBool
FcCharSetEnsureBank (int bi);

void
FcLangCharSetPopulate (void)
{
    int bi = FcCacheBankToIndex (FC_BANK_LANGS);
    FcCharSetEnsureBank (bi);
    charsets[bi] = 0;
    numbers[bi] = (FcChar16 *)&langBankNumbers;
    leaves[bi] = (FcCharLeaf *)&langBankLeaves;
    leaf_idx[bi] = (int *)&langBankLeafIdx;
}

FcCharSet *
FcCharSetCreate (void)
{
    FcCharSet	*fcs;

    fcs = (FcCharSet *) malloc (sizeof (FcCharSet));
    if (!fcs)
	return 0;
    FcMemAlloc (FC_MEM_CHARSET, sizeof (FcCharSet));
    fcs->ref = 1;
    fcs->num = 0;
    fcs->bank = FC_BANK_DYNAMIC;
    fcs->u.dyn.leaves = 0;
    fcs->u.dyn.numbers = 0;
    return fcs;
}

FcCharSet *
FcCharSetNew (void);
    
FcCharSet *
FcCharSetNew (void)
{
    return FcCharSetCreate ();
}

void
FcCharSetDestroy (FcCharSet *fcs)
{
    int i;
    if (fcs->ref == FC_REF_CONSTANT)
	return;
    if (--fcs->ref > 0)
	return;
    if (fcs->bank == FC_BANK_DYNAMIC)
    {
	for (i = 0; i < fcs->num; i++)
	{
	    FcMemFree (FC_MEM_CHARLEAF, sizeof (FcCharLeaf));
	    free (fcs->u.dyn.leaves[i]);
	}
	if (fcs->u.dyn.leaves)
	{
	    FcMemFree (FC_MEM_CHARSET, fcs->num * sizeof (FcCharLeaf *));
	    free (fcs->u.dyn.leaves);
	}
	if (fcs->u.dyn.numbers)
	{
	    FcMemFree (FC_MEM_CHARSET, fcs->num * sizeof (FcChar16));
	    free (fcs->u.dyn.numbers);
	}
    }
    FcMemFree (FC_MEM_CHARSET, sizeof (FcCharSet));
    free (fcs);
}

/*
 * Locate the leaf containing the specified char, return
 * its index if it exists, otherwise return negative of
 * the (position + 1) where it should be inserted
 */

static int
FcCharSetFindLeafPos (const FcCharSet *fcs, FcChar32 ucs4)
{
    FcChar16		*numbers = FcCharSetGetNumbers(fcs);
    FcChar16		page;
    int			low = 0;
    int			high = fcs->num - 1;

    if (!numbers)
	return -1;
    ucs4 >>= 8;
    while (low <= high)
    {
	int mid = (low + high) >> 1;
	page = numbers[mid];
	if (page == ucs4)
	    return mid;
	if (page < ucs4)
	    low = mid + 1;
	else
	    high = mid - 1;
    }
    if (high < 0 || (high < fcs->num && numbers[high] < ucs4))
	high++;
    return -(high + 1);
}

static FcCharLeaf *
FcCharSetFindLeaf (const FcCharSet *fcs, FcChar32 ucs4)
{
    int	pos = FcCharSetFindLeafPos (fcs, ucs4);
    if (pos >= 0)
	return FcCharSetGetLeaf(fcs, pos);
    return 0;
}

static FcBool
FcCharSetPutLeaf (FcCharSet	*fcs, 
		  FcChar32	ucs4,
		  FcCharLeaf	*leaf, 
		  int		pos)
{
    FcCharLeaf	**leaves;
    FcChar16	*numbers;

    ucs4 >>= 8;
    if (ucs4 >= 0x10000)
	return FcFalse;
    if (fcs->bank != FC_BANK_DYNAMIC)
    {
	int i;

	leaves = malloc ((fcs->num + 1) * sizeof (FcCharLeaf *));
	if (!leaves)
	    return FcFalse;
	FcMemAlloc (FC_MEM_CHARSET, (fcs->num + 1) * sizeof (FcCharLeaf *));
	numbers = malloc ((fcs->num + 1) * sizeof (FcChar16));
	if (!numbers)
	    return FcFalse;
	FcMemAlloc (FC_MEM_CHARSET, (fcs->num + 1) * sizeof (FcChar16));

	for (i = 0; i < fcs->num; i++)
	    leaves[i] = FcCharSetGetLeaf(fcs, i);
	memcpy (numbers, FcCharSetGetNumbers(fcs), 
		fcs->num * sizeof (FcChar16));
    }
    else
    {
	if (!fcs->u.dyn.leaves)
	    leaves = malloc (sizeof (FcCharLeaf *));
	else
	    leaves = realloc (fcs->u.dyn.leaves, (fcs->num + 1) * sizeof (FcCharLeaf *));
	if (!leaves)
	    return FcFalse;
	if (fcs->num)
	    FcMemFree (FC_MEM_CHARSET, fcs->num * sizeof (FcCharLeaf *));
	FcMemAlloc (FC_MEM_CHARSET, (fcs->num + 1) * sizeof (FcCharLeaf *));
	fcs->u.dyn.leaves = leaves;
	if (!fcs->u.dyn.numbers)
	    numbers = malloc (sizeof (FcChar16));
	else
	    numbers = realloc (fcs->u.dyn.numbers, (fcs->num + 1) * sizeof (FcChar16));
	if (!numbers)
	    return FcFalse;
	if (fcs->num)
	    FcMemFree (FC_MEM_CHARSET, fcs->num * sizeof (FcChar16));
	FcMemAlloc (FC_MEM_CHARSET, (fcs->num + 1) * sizeof (FcChar16));
	fcs->u.dyn.numbers = numbers;
    }
    
    memmove (fcs->u.dyn.leaves + pos + 1, fcs->u.dyn.leaves + pos, 
	     (fcs->num - pos) * sizeof (FcCharLeaf *));
    memmove (fcs->u.dyn.numbers + pos + 1, fcs->u.dyn.numbers + pos,
	     (fcs->num - pos) * sizeof (FcChar16));
    fcs->u.dyn.numbers[pos] = (FcChar16) ucs4;
    fcs->u.dyn.leaves[pos] = leaf;
    fcs->num++;
    return FcTrue;
}

/*
 * Locate the leaf containing the specified char, creating it
 * if desired
 */

FcCharLeaf *
FcCharSetFindLeafCreate (FcCharSet *fcs, FcChar32 ucs4)
{
    int			pos;
    FcCharLeaf		*leaf;

    pos = FcCharSetFindLeafPos (fcs, ucs4);
    if (pos >= 0)
	return FcCharSetGetLeaf(fcs, pos);
    
    leaf = calloc (1, sizeof (FcCharLeaf));
    if (!leaf)
	return 0;
    
    pos = -pos - 1;
    if (!FcCharSetPutLeaf (fcs, ucs4, leaf, pos))
    {
	free (leaf);
	return 0;
    }
    FcMemAlloc (FC_MEM_CHARLEAF, sizeof (FcCharLeaf));
    return leaf;
}

static FcBool
FcCharSetInsertLeaf (FcCharSet *fcs, FcChar32 ucs4, FcCharLeaf *leaf)
{
    int		    pos;

    pos = FcCharSetFindLeafPos (fcs, ucs4);
    if (pos >= 0)
    {
	FcMemFree (FC_MEM_CHARLEAF, sizeof (FcCharLeaf));
	if (fcs->bank == FC_BANK_DYNAMIC)
	{
	    free (fcs->u.dyn.leaves[pos]);
	    fcs->u.dyn.leaves[pos] = leaf;
	}
	else
	{
	    leaves[fcs->bank][leaf_idx[fcs->bank][fcs->u.stat.leafidx_offset]+pos] = *leaf;
	}
	return FcTrue;
    }
    pos = -pos - 1;
    return FcCharSetPutLeaf (fcs, ucs4, leaf, pos);
}

FcBool
FcCharSetAddChar (FcCharSet *fcs, FcChar32 ucs4)
{
    FcCharLeaf	*leaf;
    FcChar32	*b;
    
    if (fcs->ref == FC_REF_CONSTANT)
	return FcFalse;
    leaf = FcCharSetFindLeafCreate (fcs, ucs4);
    if (!leaf)
	return FcFalse;
    b = &leaf->map[(ucs4 & 0xff) >> 5];
    *b |= (1 << (ucs4 & 0x1f));
    return FcTrue;
}

/*
 * An iterator for the leaves of a charset
 */

typedef struct _fcCharSetIter {
    FcCharLeaf	    *leaf;
    FcChar32	    ucs4;
    int		    pos;
} FcCharSetIter;

/*
 * Set iter->leaf to the leaf containing iter->ucs4 or higher
 */

static void
FcCharSetIterSet (const FcCharSet *fcs, FcCharSetIter *iter)
{
    int		    pos = FcCharSetFindLeafPos (fcs, iter->ucs4);

    if (pos < 0)
    {
	pos = -pos - 1;
	if (pos == fcs->num)
	{
	    iter->ucs4 = ~0;
	    iter->leaf = 0;
	    return;
	}
        iter->ucs4 = (FcChar32) FcCharSetGetNumbers(fcs)[pos] << 8;
    }
    iter->leaf = FcCharSetGetLeaf(fcs, pos);
    iter->pos = pos;
#ifdef CHATTY
    printf ("set %08x: %08x\n", iter->ucs4, (FcChar32) iter->leaf);
#endif
}

static void
FcCharSetIterNext (const FcCharSet *fcs, FcCharSetIter *iter)
{
    int	pos = iter->pos + 1;
    if (pos >= fcs->num)
    {
	iter->ucs4 = ~0;
	iter->leaf = 0;
    }
    else
    {
	iter->ucs4 = (FcChar32) FcCharSetGetNumbers(fcs)[pos] << 8;
	iter->leaf = FcCharSetGetLeaf(fcs, pos);
	iter->pos = pos;
    }
}

#ifdef CHATTY
static void
FcCharSetDump (const FcCharSet *fcs)
{
    int		pos;

    printf ("fcs %08x:\n", (FcChar32) fcs);
    for (pos = 0; pos < fcs->num; pos++)
    {
	FcCharLeaf	*leaf = fcs->leaves[pos];
	FcChar32	ucs4 = (FcChar32) fcs->numbers[pos] << 8;
	
	printf ("    %08x: %08x\n", ucs4, (FcChar32) leaf);
    }
}
#endif

static void
FcCharSetIterStart (const FcCharSet *fcs, FcCharSetIter *iter)
{
#ifdef CHATTY
    FcCharSetDump (fcs);
#endif
    iter->ucs4 = 0;
    iter->pos = 0;
    FcCharSetIterSet (fcs, iter);
}

FcCharSet *
FcCharSetCopy (FcCharSet *src)
{
    if (src->ref != FC_REF_CONSTANT)
	src->ref++;
    return src;
}

FcBool
FcCharSetEqual (const FcCharSet *a, const FcCharSet *b)
{
    FcCharSetIter   ai, bi;
    int		    i;
    
    if (a == b)
	return FcTrue;
    for (FcCharSetIterStart (a, &ai), FcCharSetIterStart (b, &bi);
	 ai.leaf && bi.leaf;
	 FcCharSetIterNext (a, &ai), FcCharSetIterNext (b, &bi))
    {
	if (ai.ucs4 != bi.ucs4)
	    return FcFalse;
	for (i = 0; i < 256/32; i++)
	    if (ai.leaf->map[i] != bi.leaf->map[i])
		return FcFalse;
    }
    return ai.leaf == bi.leaf;
}

static FcBool
FcCharSetAddLeaf (FcCharSet	*fcs,
		  FcChar32	ucs4,
		  FcCharLeaf	*leaf)
{
    FcCharLeaf   *new = FcCharSetFindLeafCreate (fcs, ucs4);
    if (!new)
	return FcFalse;
    *new = *leaf;
    return FcTrue;
}

static FcCharSet *
FcCharSetOperate (const FcCharSet   *a,
		  const FcCharSet   *b,
		  FcBool	    (*overlap) (FcCharLeaf	    *result,
						const FcCharLeaf    *al,
						const FcCharLeaf    *bl),
		  FcBool	aonly,
		  FcBool	bonly)
{
    FcCharSet	    *fcs;
    FcCharSetIter   ai, bi;

    fcs = FcCharSetCreate ();
    if (!fcs)
	goto bail0;
    FcCharSetIterStart (a, &ai);
    FcCharSetIterStart (b, &bi);
    while ((ai.leaf || (bonly && bi.leaf)) && (bi.leaf || (aonly && ai.leaf)))
    {
	if (ai.ucs4 < bi.ucs4)
	{
	    if (aonly)
	    {
		if (!FcCharSetAddLeaf (fcs, ai.ucs4, ai.leaf))
		    goto bail1;
		FcCharSetIterNext (a, &ai);
	    }
	    else
	    {
		ai.ucs4 = bi.ucs4;
		FcCharSetIterSet (a, &ai);
	    }
	}
	else if (bi.ucs4 < ai.ucs4 )
	{
	    if (bonly)
	    {
		if (!FcCharSetAddLeaf (fcs, bi.ucs4, bi.leaf))
		    goto bail1;
		FcCharSetIterNext (b, &bi);
	    }
	    else
	    {
		bi.ucs4 = ai.ucs4;
		FcCharSetIterSet (b, &bi);
	    }
	}
	else
	{
	    FcCharLeaf  leaf;

	    if ((*overlap) (&leaf, ai.leaf, bi.leaf))
	    {
		if (!FcCharSetAddLeaf (fcs, ai.ucs4, &leaf))
		    goto bail1;
	    }
	    FcCharSetIterNext (a, &ai);
	    FcCharSetIterNext (b, &bi);
	}
    }
    return fcs;
bail1:
    FcCharSetDestroy (fcs);
bail0:
    return 0;
}

static FcBool
FcCharSetIntersectLeaf (FcCharLeaf *result,
			const FcCharLeaf *al,
			const FcCharLeaf *bl)
{
    int	    i;
    FcBool  nonempty = FcFalse;

    for (i = 0; i < 256/32; i++)
	if ((result->map[i] = al->map[i] & bl->map[i]))
	    nonempty = FcTrue;
    return nonempty;
}

FcCharSet *
FcCharSetIntersect (const FcCharSet *a, const FcCharSet *b)
{
    return FcCharSetOperate (a, b, FcCharSetIntersectLeaf, FcFalse, FcFalse);
}

static FcBool
FcCharSetUnionLeaf (FcCharLeaf *result,
		    const FcCharLeaf *al,
		    const FcCharLeaf *bl)
{
    int	i;

    for (i = 0; i < 256/32; i++)
	result->map[i] = al->map[i] | bl->map[i];
    return FcTrue;
}

FcCharSet *
FcCharSetUnion (const FcCharSet *a, const FcCharSet *b)
{
    return FcCharSetOperate (a, b, FcCharSetUnionLeaf, FcTrue, FcTrue);
}

static FcBool
FcCharSetSubtractLeaf (FcCharLeaf *result,
		       const FcCharLeaf *al,
		       const FcCharLeaf *bl)
{
    int	    i;
    FcBool  nonempty = FcFalse;

    for (i = 0; i < 256/32; i++)
	if ((result->map[i] = al->map[i] & ~bl->map[i]))
	    nonempty = FcTrue;
    return nonempty;
}

FcCharSet *
FcCharSetSubtract (const FcCharSet *a, const FcCharSet *b)
{
    return FcCharSetOperate (a, b, FcCharSetSubtractLeaf, FcTrue, FcFalse);
}

FcBool
FcCharSetHasChar (const FcCharSet *fcs, FcChar32 ucs4)
{
    FcCharLeaf	*leaf = FcCharSetFindLeaf (fcs, ucs4);
    if (!leaf)
	return FcFalse;
    return (leaf->map[(ucs4 & 0xff) >> 5] & (1 << (ucs4 & 0x1f))) != 0;
}

static FcChar32
FcCharSetPopCount (FcChar32 c1)
{
    /* hackmem 169 */
    FcChar32	c2 = (c1 >> 1) & 033333333333;
    c2 = c1 - c2 - ((c2 >> 1) & 033333333333);
    return (((c2 + (c2 >> 3)) & 030707070707) % 077);
}

FcChar32
FcCharSetIntersectCount (const FcCharSet *a, const FcCharSet *b)
{
    FcCharSetIter   ai, bi;
    FcChar32	    count = 0;
    
    FcCharSetIterStart (a, &ai);
    FcCharSetIterStart (b, &bi);
    while (ai.leaf && bi.leaf)
    {
	if (ai.ucs4 == bi.ucs4)
	{
	    FcChar32	*am = ai.leaf->map;
	    FcChar32	*bm = bi.leaf->map;
	    int		i = 256/32;
	    while (i--)
		count += FcCharSetPopCount (*am++ & *bm++);
	    FcCharSetIterNext (a, &ai);
	} 
	else if (ai.ucs4 < bi.ucs4)
	{
	    ai.ucs4 = bi.ucs4;
	    FcCharSetIterSet (a, &ai);
	}
	if (bi.ucs4 < ai.ucs4)
	{
	    bi.ucs4 = ai.ucs4;
	    FcCharSetIterSet (b, &bi);
	}
    }
    return count;
}

FcChar32
FcCharSetCount (const FcCharSet *a)
{
    FcCharSetIter   ai;
    FcChar32	    count = 0;
    
    for (FcCharSetIterStart (a, &ai); ai.leaf; FcCharSetIterNext (a, &ai))
    {
	int		    i = 256/32;
	FcChar32	    *am = ai.leaf->map;

	while (i--)
	    count += FcCharSetPopCount (*am++);
    }
    return count;
}

FcChar32
FcCharSetSubtractCount (const FcCharSet *a, const FcCharSet *b)
{
    FcCharSetIter   ai, bi;
    FcChar32	    count = 0;
    
    FcCharSetIterStart (a, &ai);
    FcCharSetIterStart (b, &bi);
    while (ai.leaf)
    {
	if (ai.ucs4 <= bi.ucs4)
	{
	    FcChar32	*am = ai.leaf->map;
	    int		i = 256/32;
	    if (ai.ucs4 == bi.ucs4)
	    {
		FcChar32	*bm = bi.leaf->map;;
		while (i--)
		    count += FcCharSetPopCount (*am++ & ~*bm++);
	    }
	    else
	    {
		while (i--)
		    count += FcCharSetPopCount (*am++);
	    }
	    FcCharSetIterNext (a, &ai);
	}
	else if (bi.leaf)
	{
	    bi.ucs4 = ai.ucs4;
	    FcCharSetIterSet (b, &bi);
	}
    }
    return count;
}

/*
 * return FcTrue iff a is a subset of b
 */
FcBool
FcCharSetIsSubset (const FcCharSet *a, const FcCharSet *b)
{
    int		ai, bi;
    FcChar16	an, bn;
    
    if (a == b) return FcTrue;
    bi = 0;
    ai = 0;
    while (ai < a->num && bi < b->num)
    {
	an = FcCharSetGetNumbers(a)[ai];
	bn = FcCharSetGetNumbers(b)[bi];
	/*
	 * Check matching pages
	 */
	if (an == bn)
	{
	    FcChar32	*am = FcCharSetGetLeaf(a, ai)->map;
	    FcChar32	*bm = FcCharSetGetLeaf(b, bi)->map;
	    
	    if (am != bm)
	    {
		int	i = 256/32;
		/*
		 * Does am have any bits not in bm?
		 */
		while (i--)
		    if (*am++ & ~*bm++)
			return FcFalse;
	    }
	    ai++;
	    bi++;
	}
	/*
	 * Does a have any pages not in b?
	 */
	else if (an < bn)
	    return FcFalse;
	else
	{
	    int	    low = bi + 1;
	    int	    high = b->num - 1;

	    /*
	     * Search for page 'an' in 'b'
	     */
	    while (low <= high)
	    {
		int mid = (low + high) >> 1;
		bn = FcCharSetGetNumbers(b)[mid];
		if (bn == an)
		{
		    high = mid;
		    break;
		}
		if (bn < an)
		    low = mid + 1;
		else
		    high = mid - 1;
	    }
	    bi = high;
	    while (bi < b->num && FcCharSetGetNumbers(b)[bi] < an)
		bi++;
	}
    }
    /*
     * did we look at every page?
     */
    return ai >= a->num;
}

/*
 * These two functions efficiently walk the entire charmap for
 * other software (like pango) that want their own copy
 */

FcChar32
FcCharSetNextPage (const FcCharSet  *a, 
		   FcChar32	    map[FC_CHARSET_MAP_SIZE],
		   FcChar32	    *next)
{
    FcCharSetIter   ai;
    FcChar32	    page;

    ai.ucs4 = *next;
    FcCharSetIterSet (a, &ai);
    if (!ai.leaf)
	return FC_CHARSET_DONE;
    
    /*
     * Save current information
     */
    page = ai.ucs4;
    memcpy (map, ai.leaf->map, sizeof (ai.leaf->map));
    /*
     * Step to next page
     */
    FcCharSetIterNext (a, &ai);
    *next = ai.ucs4;

    return page;
}

FcChar32
FcCharSetFirstPage (const FcCharSet *a, 
		    FcChar32	    map[FC_CHARSET_MAP_SIZE],
		    FcChar32	    *next)
{
    *next = 0;
    return FcCharSetNextPage (a, map, next);
}

/*
 * old coverage API, rather hard to use correctly
 */
FcChar32
FcCharSetCoverage (const FcCharSet *a, FcChar32 page, FcChar32 *result);
    
FcChar32
FcCharSetCoverage (const FcCharSet *a, FcChar32 page, FcChar32 *result)
{
    FcCharSetIter   ai;

    ai.ucs4 = page;
    FcCharSetIterSet (a, &ai);
    if (!ai.leaf)
    {
	memset (result, '\0', 256 / 8);
	page = 0;
    }
    else
    {
	memcpy (result, ai.leaf->map, sizeof (ai.leaf->map));
	FcCharSetIterNext (a, &ai);
	page = ai.ucs4;
    }
    return page;
}

/*
 * ASCII representation of charsets.
 * 
 * Each leaf is represented as 9 32-bit values, the code of the first character followed
 * by 8 32 bit values for the leaf itself.  Each value is encoded as 5 ASCII characters,
 * only 85 different values are used to avoid control characters as well as the other
 * characters used to encode font names.  85**5 > 2^32 so things work out, but
 * it's not exactly human readable output.  As a special case, 0 is encoded as a space
 */

static const unsigned char	charToValue[256] = {
    /*     "" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /*   "\b" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\020" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\030" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /*    " " */ 0xff,  0x00,  0xff,  0x01,  0x02,  0x03,  0x04,  0xff, 
    /*    "(" */ 0x05,  0x06,  0x07,  0x08,  0xff,  0xff,  0x09,  0x0a, 
    /*    "0" */ 0x0b,  0x0c,  0x0d,  0x0e,  0x0f,  0x10,  0x11,  0x12, 
    /*    "8" */ 0x13,  0x14,  0xff,  0x15,  0x16,  0xff,  0x17,  0x18, 
    /*    "@" */ 0x19,  0x1a,  0x1b,  0x1c,  0x1d,  0x1e,  0x1f,  0x20, 
    /*    "H" */ 0x21,  0x22,  0x23,  0x24,  0x25,  0x26,  0x27,  0x28, 
    /*    "P" */ 0x29,  0x2a,  0x2b,  0x2c,  0x2d,  0x2e,  0x2f,  0x30, 
    /*    "X" */ 0x31,  0x32,  0x33,  0x34,  0xff,  0x35,  0x36,  0xff, 
    /*    "`" */ 0xff,  0x37,  0x38,  0x39,  0x3a,  0x3b,  0x3c,  0x3d, 
    /*    "h" */ 0x3e,  0x3f,  0x40,  0x41,  0x42,  0x43,  0x44,  0x45, 
    /*    "p" */ 0x46,  0x47,  0x48,  0x49,  0x4a,  0x4b,  0x4c,  0x4d, 
    /*    "x" */ 0x4e,  0x4f,  0x50,  0x51,  0x52,  0x53,  0x54,  0xff, 
    /* "\200" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\210" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\220" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\230" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\240" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\250" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\260" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\270" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\300" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\310" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\320" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\330" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\340" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\350" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\360" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
    /* "\370" */ 0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff,  0xff, 
};

static const FcChar8 valueToChar[0x55] = {
    /* 0x00 */ '!', '#', '$', '%', '&', '(', ')', '*',
    /* 0x08 */ '+', '.', '/', '0', '1', '2', '3', '4',
    /* 0x10 */ '5', '6', '7', '8', '9', ';', '<', '>',
    /* 0x18 */ '?', '@', 'A', 'B', 'C', 'D', 'E', 'F',
    /* 0x20 */ 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    /* 0x28 */ 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
    /* 0x30 */ 'W', 'X', 'Y', 'Z', '[', ']', '^', 'a',
    /* 0x38 */ 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
    /* 0x40 */ 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q',
    /* 0x48 */ 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
    /* 0x50 */ 'z', '{', '|', '}', '~',
};

static FcChar8 *
FcCharSetParseValue (FcChar8 *string, FcChar32 *value)
{
    int		i;
    FcChar32	v;
    FcChar32	c;
    
    if (*string == ' ')
    {
	v = 0;
	string++;
    }
    else
    {
	v = 0;
	for (i = 0; i < 5; i++)
	{
	    if (!(c = (FcChar32) (unsigned char) *string++))
		return 0;
	    c = charToValue[c];
	    if (c == 0xff)
		return 0;
	    v = v * 85 + c;
	}
    }
    *value = v;
    return string;
}

static FcBool
FcCharSetUnparseValue (FcStrBuf *buf, FcChar32 value)
{
    int	    i;
    if (value == 0)
    {
	return FcStrBufChar (buf, ' ');
    }
    else
    {
	FcChar8	string[6];
	FcChar8	*s = string + 5;
	string[5] = '\0';
	for (i = 0; i < 5; i++)
	{
	    *--s = valueToChar[value % 85];
	    value /= 85;
	}
	for (i = 0; i < 5; i++)
	    if (!FcStrBufChar (buf, *s++))
		return FcFalse;
    }
    return FcTrue;
}

typedef struct _FcCharLeafEnt FcCharLeafEnt;

struct _FcCharLeafEnt {
    FcCharLeafEnt   *next;
    FcChar32	    hash;
    FcCharLeaf	    leaf;
};

#define FC_CHAR_LEAF_BLOCK	(4096 / sizeof (FcCharLeafEnt))
static FcCharLeafEnt **FcCharLeafBlocks;
static int FcCharLeafBlockCount;

static FcCharLeafEnt *
FcCharLeafEntCreate (void)
{
    static FcCharLeafEnt    *block;
    static int		    remain;

    if (!remain)
    {
	FcCharLeafEnt **newBlocks;

	FcCharLeafBlockCount++;
	newBlocks = realloc (FcCharLeafBlocks, FcCharLeafBlockCount * sizeof (FcCharLeafEnt *));
	if (!newBlocks)
	    return 0;
	FcCharLeafBlocks = newBlocks;
	block = FcCharLeafBlocks[FcCharLeafBlockCount-1] = malloc (FC_CHAR_LEAF_BLOCK * sizeof (FcCharLeafEnt));
	if (!block)
	    return 0;
	FcMemAlloc (FC_MEM_CHARLEAF, FC_CHAR_LEAF_BLOCK * sizeof (FcCharLeafEnt));
	remain = FC_CHAR_LEAF_BLOCK;
    }
    remain--;
    return block++;
}

#define FC_CHAR_LEAF_HASH_SIZE	257

static FcChar32
FcCharLeafHash (FcCharLeaf *leaf)
{
    FcChar32	hash = 0;
    int		i;

    for (i = 0; i < 256/32; i++)
	hash = ((hash << 1) | (hash >> 31)) ^ leaf->map[i];
    return hash;
}

static int	FcCharLeafTotal;
static int	FcCharLeafUsed;

static FcCharLeafEnt	*FcCharLeafHashTable[FC_CHAR_LEAF_HASH_SIZE];

static FcCharLeaf *
FcCharSetFreezeLeaf (FcCharLeaf *leaf)
{
    FcChar32			hash = FcCharLeafHash (leaf);
    FcCharLeafEnt		**bucket = &FcCharLeafHashTable[hash % FC_CHAR_LEAF_HASH_SIZE];
    FcCharLeafEnt		*ent;
    
    FcCharLeafTotal++;
    for (ent = *bucket; ent; ent = ent->next)
    {
	if (ent->hash == hash && !memcmp (&ent->leaf, leaf, sizeof (FcCharLeaf)))
	    return &ent->leaf;
    }

    ent = FcCharLeafEntCreate();
    if (!ent)
	return 0;
    FcCharLeafUsed++;
    ent->leaf = *leaf;
    ent->hash = hash;
    ent->next = *bucket;
    *bucket = ent;
    return &ent->leaf;
}

static void
FcCharSetThawAllLeaf (void)
{
    int i;

    for (i = 0; i < FC_CHAR_LEAF_HASH_SIZE; i++)
	FcCharLeafHashTable[i] = 0;

    FcCharLeafTotal = 0;
    FcCharLeafUsed = 0;

    for (i = 0; i < FcCharLeafBlockCount; i++)
	free (FcCharLeafBlocks[i]);

    free (FcCharLeafBlocks);
    FcCharLeafBlocks = 0;
    FcCharLeafBlockCount = 0;
}

typedef struct _FcCharSetEnt FcCharSetEnt;

struct _FcCharSetEnt {
    FcCharSetEnt	*next;
    FcChar32		hash;
    FcCharSet		set;
};

#define FC_CHAR_SET_HASH_SIZE    67

static FcChar32
FcCharSetHash (FcCharSet *fcs)
{
    FcChar32	hash = 0;
    int		i;

    /* hash in leaves */
    for (i = 0; i < fcs->num * (int) (sizeof (FcCharLeaf *) / sizeof (FcChar32)); i++)
	hash = ((hash << 1) | (hash >> 31)) ^ (FcChar32)(FcCharSetGetLeaf(fcs, i)->map);
    /* hash in numbers */
    for (i = 0; i < fcs->num; i++)
	hash = ((hash << 1) | (hash >> 31)) ^ *FcCharSetGetNumbers(fcs);
    return hash;
}

static int	FcCharSetTotal;
static int	FcCharSetUsed;
static int	FcCharSetTotalEnts, FcCharSetUsedEnts;

static FcCharSetEnt	*FcCharSetHashTable[FC_CHAR_SET_HASH_SIZE];

static FcCharSet *
FcCharSetFreezeBase (FcCharSet *fcs)
{
    FcChar32		hash = FcCharSetHash (fcs);
    FcCharSetEnt	**bucket = &FcCharSetHashTable[hash % FC_CHAR_SET_HASH_SIZE];
    FcCharSetEnt	*ent;
    int			size;

    FcCharSetTotal++;
    FcCharSetTotalEnts += fcs->num;
    for (ent = *bucket; ent; ent = ent->next)
    {
	if (ent->hash == hash &&
	    ent->set.num == fcs->num &&
	    !memcmp (FcCharSetGetNumbers(&ent->set), 
		     FcCharSetGetNumbers(fcs),
		     fcs->num * sizeof (FcChar16)))
	{
	    FcBool ok = FcTrue;
	    int i;

	    for (i = 0; i < fcs->num; i++)
		if (FcCharSetGetLeaf(&ent->set, i) != FcCharSetGetLeaf(fcs, i))
		    ok = FcFalse;
	    if (ok)
		return &ent->set;
	}
    }

    size = (sizeof (FcCharSetEnt) +
	    fcs->num * sizeof (FcCharLeaf *) +
	    fcs->num * sizeof (FcChar16));
    ent = malloc (size);
    if (!ent)
	return 0;
    FcMemAlloc (FC_MEM_CHARSET, size);
    FcCharSetUsed++;
    FcCharSetUsedEnts += fcs->num;
    
    ent->set.ref = FC_REF_CONSTANT;
    ent->set.num = fcs->num;
    ent->set.bank = fcs->bank;
    if (fcs->bank == FC_BANK_DYNAMIC)
    {
	if (fcs->num)
	{
	    ent->set.u.dyn.leaves = (FcCharLeaf **) (ent + 1);
	    ent->set.u.dyn.numbers = (FcChar16 *) (ent->set.u.dyn.leaves + fcs->num);
	    memcpy (ent->set.u.dyn.leaves, fcs->u.dyn.leaves, fcs->num * sizeof (FcCharLeaf *));
	    memcpy (ent->set.u.dyn.numbers, fcs->u.dyn.numbers, fcs->num * sizeof (FcChar16));
	}
	else
	{
	    ent->set.u.dyn.leaves = 0;
	    ent->set.u.dyn.numbers = 0;
	}
    }
    else
    {
	ent->set.u.stat.leafidx_offset = fcs->u.stat.leafidx_offset;
	ent->set.u.stat.numbers_offset = fcs->u.stat.numbers_offset;
    }

    ent->hash = hash;
    ent->next = *bucket;
    *bucket = ent;
    return &ent->set;
}

void
FcCharSetThawAll (void)
{
    int i;
    FcCharSetEnt	*ent, *next;

    for (i = 0; i < FC_CHAR_SET_HASH_SIZE; i++)
    {
	for (ent = FcCharSetHashTable[i]; ent; ent = next)
	{
	    next = ent->next;
	    free (ent);
	}
	FcCharSetHashTable[i] = 0;
    }

    FcCharSetTotal = 0;
    FcCharSetTotalEnts = 0;
    FcCharSetUsed = 0;
    FcCharSetUsedEnts = 0;

    FcCharSetThawAllLeaf ();
}

FcCharSet *
FcCharSetFreeze (FcCharSet *fcs)
{
    FcCharSet	*b;
    FcCharSet	*n = 0;
    FcCharLeaf	*l;
    int		i;

    b = FcCharSetCreate ();
    if (!b)
	goto bail0;
    for (i = 0; i < fcs->num; i++)
    {
	l = FcCharSetFreezeLeaf (FcCharSetGetLeaf(fcs, i));
	if (!l)
	    goto bail1;
	if (!FcCharSetInsertLeaf (b, FcCharSetGetNumbers(fcs)[i] << 8, l))
	    goto bail1;
    }
    n = FcCharSetFreezeBase (b);
bail1:
    if (b->bank == FC_BANK_DYNAMIC)
    {
	if (b->u.dyn.leaves)
	{
	    FcMemFree (FC_MEM_CHARSET, b->num * sizeof (FcCharLeaf *));
	    free (b->u.dyn.leaves);
	}
	if (b->u.dyn.numbers)
	{
	    FcMemFree (FC_MEM_CHARSET, b->num * sizeof (FcChar16));
	    free (b->u.dyn.numbers);
	}
    }
    FcMemFree (FC_MEM_CHARSET, sizeof (FcCharSet));
    free (b);
bail0:
    return n;
}

FcCharSet *
FcNameParseCharSet (FcChar8 *string)
{
    FcCharSet	*c, *n = 0;
    FcChar32	ucs4;
    FcCharLeaf	*leaf;
    FcCharLeaf	temp;
    FcChar32	bits;
    int		i;

    c = FcCharSetCreate ();
    if (!c)
	goto bail0;
    while (*string)
    {
	string = FcCharSetParseValue (string, &ucs4);
	if (!string)
	    goto bail1;
	bits = 0;
	for (i = 0; i < 256/32; i++)
	{
	    string = FcCharSetParseValue (string, &temp.map[i]);
	    if (!string)
		goto bail1;
	    bits |= temp.map[i];
	}
	if (bits)
	{
	    leaf = FcCharSetFreezeLeaf (&temp);
	    if (!leaf)
		goto bail1;
	    if (!FcCharSetInsertLeaf (c, ucs4, leaf))
		goto bail1;
	}
    }
#ifdef CHATTY
    printf ("          %8s %8s %8s %8s\n", "total", "totalmem", "new", "newmem");
    printf ("Leaves:   %8d %8d %8d %8d\n",
	    FcCharLeafTotal, sizeof (FcCharLeaf) * FcCharLeafTotal,
	    FcCharLeafUsed, sizeof (FcCharLeaf) * FcCharLeafUsed);
    printf ("Charsets: %8d %8d %8d %8d\n",
	    FcCharSetTotal, sizeof (FcCharSet) * FcCharSetTotal,
	    FcCharSetUsed, sizeof (FcCharSet) * FcCharSetUsed);
    printf ("Tables:   %8d %8d %8d %8d\n",
	    FcCharSetTotalEnts, FcCharSetTotalEnts * (sizeof (FcCharLeaf *) + sizeof (FcChar16)),
	    FcCharSetUsedEnts, FcCharSetUsedEnts * (sizeof (FcCharLeaf *) + sizeof (FcChar16)));
    printf ("Total:    %8s %8d %8s %8d\n", 
	    "", 
	    sizeof (FcCharLeaf) * FcCharLeafTotal +
	    sizeof (FcCharSet) * FcCharSetTotal +
	    FcCharSetTotalEnts * (sizeof (FcCharLeaf *) + sizeof (FcChar16)),
	    "",
	    sizeof (FcCharLeaf) * FcCharLeafUsed +
	    sizeof (FcCharSet) * FcCharSetUsed +
	    FcCharSetUsedEnts * (sizeof (FcCharLeaf *) + sizeof (FcChar16)));
#endif
    n = FcCharSetFreezeBase (c);
bail1:
    if (c->bank == FC_BANK_DYNAMIC)
    {
	if (c->u.dyn.leaves)
	{
	    FcMemFree (FC_MEM_CHARSET, c->num * sizeof (FcCharLeaf *));
	    free (c->u.dyn.leaves);
	}
	if (c->u.dyn.numbers)
	{
	    FcMemFree (FC_MEM_CHARSET, c->num * sizeof (FcChar16));
	    free (c->u.dyn.numbers);
	}
	FcMemFree (FC_MEM_CHARSET, sizeof (FcCharSet));
    }
    free (c);
bail0:
    return n;
}

FcBool
FcNameUnparseCharSet (FcStrBuf *buf, const FcCharSet *c)
{
    FcCharSetIter   ci;
    int		    i;
#ifdef CHECK
    int		    len = buf->len;
#endif

    for (FcCharSetIterStart (c, &ci);
	 ci.leaf;
	 FcCharSetIterNext (c, &ci))
    {
	if (!FcCharSetUnparseValue (buf, ci.ucs4))
	    return FcFalse;
	for (i = 0; i < 256/32; i++)
	    if (!FcCharSetUnparseValue (buf, ci.leaf->map[i]))
		return FcFalse;
    }
#ifdef CHECK
    {
	FcCharSet	*check;
	FcChar32	missing;
	FcCharSetIter	ci, checki;
	
	/* null terminate for parser */
	FcStrBufChar (buf, '\0');
	/* step back over null for life after test */
	buf->len--;
	check = FcNameParseCharSet (buf->buf + len);
	FcCharSetIterStart (c, &ci);
	FcCharSetIterStart (check, &checki);
	while (ci.leaf || checki.leaf)
	{
	    if (ci.ucs4 < checki.ucs4)
	    {
		printf ("Missing leaf node at 0x%x\n", ci.ucs4);
		FcCharSetIterNext (c, &ci);
	    }
	    else if (checki.ucs4 < ci.ucs4)
	    {
		printf ("Extra leaf node at 0x%x\n", checki.ucs4);
		FcCharSetIterNext (check, &checki);
	    }
	    else
	    {
		int	    i = 256/32;
		FcChar32    *cm = ci.leaf->map;
		FcChar32    *checkm = checki.leaf->map;

		for (i = 0; i < 256; i += 32)
		{
		    if (*cm != *checkm)
			printf ("Mismatching sets at 0x%08x: 0x%08x != 0x%08x\n",
				ci.ucs4 + i, *cm, *checkm);
		    cm++;
		    checkm++;
		}
		FcCharSetIterNext (c, &ci);
		FcCharSetIterNext (check, &checki);
	    }
	}
	if ((missing = FcCharSetSubtractCount (c, check)))
	    printf ("%d missing in reparsed result\n", missing);
	if ((missing = FcCharSetSubtractCount (check, c)))
	    printf ("%d extra in reparsed result\n", missing);
	FcCharSetDestroy (check);
    }
#endif
    
    return FcTrue;
}

void
FcCharSetNewBank(void)
{
    charset_count = 0;
    charset_numbers_count = 0;
    charset_leaf_count = 0;
    charset_leaf_idx_count = 0;
}

int
FcCharSetNeededBytes (const FcCharSet *c)
{
    /* yes, there's redundancy */
    charset_count++;
    charset_leaf_idx_count++;
    charset_leaf_count += c->num;
    charset_numbers_count += c->num;
    return sizeof (FcCharSet) + 
	sizeof (int) + 			/* leaf_idx */
	sizeof (FcCharLeaf) * c->num + 	/* leaf */
	sizeof (FcChar16) * c->num; 	/* number */
}

int
FcCharSetNeededBytesAlign (void)
{
    return __alignof__ (FcCharSet) + __alignof__ (int) + 
	__alignof__ (FcCharLeaf) + __alignof__ (FcChar16);
}

static FcBool
FcCharSetEnsureBank (int bi)
{
    if (!charsets || charset_bank_count <= bi)
    {
	int new_count = charset_bank_count + 2;
	FcCharSet ** cs;
	FcChar16 ** n;
	FcCharLeaf ** lvs;
	int ** lvi;
	int i;
	
	cs = realloc(charsets, sizeof(FcCharSet*) * new_count);
	if (!cs) return 0;
	n = realloc(numbers, sizeof(FcChar16*) * new_count);
	if (!n) return 0;
	lvs = realloc(leaves, sizeof(FcCharLeaf*) * new_count);
	if (!lvs) return 0;
	lvi = realloc(leaf_idx, sizeof(int*) * new_count);
	if (!lvi) return 0;

	charsets = cs; numbers = n; leaves = lvs; leaf_idx = lvi;
	for (i = charset_bank_count; i < new_count; i++)
	{
	    charsets[i] = 0;
	    numbers[i] = 0;
	    leaves[i] = 0;
	    leaf_idx[i] = 0;
	}
	charset_bank_count = new_count;
    }
    return FcTrue;
}

void *
FcCharSetDistributeBytes (FcCache * metadata, void * block_ptr)
{
    int bi = FcCacheBankToIndex(metadata->bank);
    if (!FcCharSetEnsureBank(bi))
	return 0;

    block_ptr = ALIGN (block_ptr, FcCharSet);
    charsets[bi] = (FcCharSet *)block_ptr;
    block_ptr = (void *)((char *)block_ptr + 
		     (sizeof (FcCharSet) * charset_count));
    block_ptr = ALIGN (block_ptr, FcChar16);
    numbers[bi] = (FcChar16 *)block_ptr;
    block_ptr = (void *)((char *)block_ptr + 
		     (sizeof(FcChar16) * charset_numbers_count));
    block_ptr = ALIGN (block_ptr, FcCharLeaf);
    leaves[bi] = (FcCharLeaf *)block_ptr;
    block_ptr = (void *)((char *)block_ptr +
		     (sizeof(FcCharLeaf) * charset_leaf_count));
    block_ptr = ALIGN (block_ptr, int);
    leaf_idx[bi] = (int *)block_ptr;
    block_ptr = (void *)((char *)block_ptr +
		     (sizeof(int) * charset_leaf_idx_count));

    metadata->charset_count = charset_count;
    metadata->charset_numbers_count = charset_numbers_count;
    metadata->charset_leaf_count = charset_leaf_count;
    metadata->charset_leaf_idx_count = charset_leaf_idx_count;
    charset_ptr = 0; charset_leaf_ptr = 0;
    charset_leaf_idx_ptr = 0; charset_numbers_ptr = 0;
    return block_ptr;
}

FcCharSet *
FcCharSetSerialize(int bank, FcCharSet *c)
{
    int i;
    FcCharSet new;
    int bi = FcCacheBankToIndex(bank), cp = charset_ptr;

    new.ref = FC_REF_CONSTANT;
    new.bank = bank;
    new.u.stat.leafidx_offset = charset_leaf_idx_ptr;
    new.u.stat.numbers_offset = charset_numbers_ptr;
    new.num = c->num;

    charsets[bi][charset_ptr++] = new;

    leaf_idx[bi][charset_leaf_idx_ptr++] = charset_leaf_ptr;
    for (i = 0; i < c->num; i++)
    {
	memcpy (&leaves[bi][charset_leaf_ptr++], 
		c->u.dyn.leaves[i], sizeof(FcCharLeaf));
	numbers[bi][charset_numbers_ptr++] = c->u.dyn.numbers[i];
    }

    return &charsets[bi][cp];
}

void *
FcCharSetUnserialize (FcCache metadata, void *block_ptr)
{
    int bi = FcCacheBankToIndex(metadata.bank);
    if (!FcCharSetEnsureBank(bi))
	return 0;

    block_ptr = ALIGN (block_ptr, FcCharSet);
    charsets[bi] = (FcCharSet *)block_ptr;
    block_ptr = (void *)((char *)block_ptr + 
		     (sizeof (FcCharSet) * metadata.charset_count));
    block_ptr = ALIGN (block_ptr, FcChar16);
    numbers[bi] = (FcChar16 *)block_ptr;
    block_ptr = (void *)((char *)block_ptr + 
		     (sizeof(FcChar16) * metadata.charset_numbers_count));
    block_ptr = ALIGN (block_ptr, FcCharLeaf);
    leaves[bi] = (FcCharLeaf *)block_ptr;
    block_ptr = (void *)((char *)block_ptr +
		     (sizeof(FcCharLeaf) * metadata.charset_leaf_count));
    block_ptr = ALIGN (block_ptr, int);
    leaf_idx[bi] = (int *)block_ptr;
    block_ptr = (void *)((char *)block_ptr +
		     (sizeof(int) * metadata.charset_leaf_idx_count));

    return block_ptr;
}

FcCharLeaf *
FcCharSetGetLeaf(const FcCharSet *c, int i)
{
    int bi;
    if (c->bank == FC_BANK_DYNAMIC)
	return c->u.dyn.leaves[i];
    bi = FcCacheBankToIndex(c->bank);

    return &leaves[bi][leaf_idx[bi][c->u.stat.leafidx_offset]+i];
}

FcChar16 *
FcCharSetGetNumbers(const FcCharSet *c)
{
    int bi;
    if (c->bank == FC_BANK_DYNAMIC)
	return c->u.dyn.numbers;
    bi = FcCacheBankToIndex(c->bank);

    return &numbers[bi][c->u.stat.numbers_offset];
}

