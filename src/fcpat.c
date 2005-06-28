/*
 * $RCSId: xc/lib/fontconfig/src/fcpat.c,v 1.18 2002/09/18 17:11:46 tsi Exp $
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#include "fcint.h"

static FcPattern * fcpatterns = NULL;
static int fcpattern_ptr, fcpattern_count;
static FcPatternElt * fcpatternelts = NULL;
static int fcpatternelt_ptr, fcpatternelt_count;
static FcValueList * fcvaluelists = NULL;
static int fcvaluelist_ptr, fcvaluelist_count;

static char * object_content;
static int object_content_count;
static int object_content_ptr;

static FcBool
FcPatternEltIsDynamic (FcPatternEltPtr pei);

static FcPatternEltPtr
FcPatternEltPtrCreateDynamic (FcPatternElt * e);

FcPattern *
FcPatternCreate (void)
{
    FcPattern	*p;

    p = (FcPattern *) malloc (sizeof (FcPattern));
    if (!p)
	return 0;
    FcMemAlloc (FC_MEM_PATTERN, sizeof (FcPattern));
    p->num = 0;
    p->size = 0;
    p->elts = FcPatternEltPtrCreateDynamic(0);
    p->ref = 1;
    return p;
}

void
FcValueDestroy (FcValue v)
{
    switch (v.type) {
    case FcTypeString:
	FcObjectPtrDestroy (v.u.si);
	break;
    case FcTypeMatrix:
	FcMatrixPtrDestroy (v.u.mi);
	break;
    case FcTypeCharSet:
	FcCharSetPtrDestroy (v.u.ci);
	break;
    case FcTypeLangSet:
	FcLangSetPtrDestroy (v.u.li);
	break;
    default:
	break;
    }
}

FcValue
FcValueSave (FcValue v)
{
    switch (v.type) {
    case FcTypeString:
	v.u.si = FcObjectPtrCreateDynamic(FcStrCopy (FcObjectPtrU(v.u.si)));
	if (!FcObjectPtrU(v.u.si))
	    v.type = FcTypeVoid;
	break;
    case FcTypeMatrix:
	v.u.mi = FcMatrixPtrCreateDynamic
	    (FcMatrixCopy (FcMatrixPtrU(v.u.mi)));
	if (!FcMatrixPtrU(v.u.mi))
	    v.type = FcTypeVoid;
	break;
    case FcTypeCharSet:
	v.u.ci = FcCharSetPtrCreateDynamic
	    (FcCharSetCopy (FcCharSetPtrU(v.u.ci)));
	if (!FcCharSetPtrU(v.u.ci))
	    v.type = FcTypeVoid;
	break;
    case FcTypeLangSet:
	v.u.li = FcLangSetPtrCreateDynamic
	    (FcLangSetCopy (FcLangSetPtrU(v.u.li)));
	if (!FcLangSetPtrU(v.u.li))
	    v.type = FcTypeVoid;
	break;
    default:
	break;
    }
    return v;
}

void
FcValueListDestroy (FcValueListPtr l)
{
    FcValueListPtr next;
    for (; FcValueListPtrU(l); l = next)
    {
	switch (FcValueListPtrU(l)->value.type) {
	case FcTypeString:
	    FcObjectPtrDestroy (FcValueListPtrU(l)->value.u.si);
	    break;
	case FcTypeMatrix:
	    FcMatrixPtrDestroy (FcValueListPtrU(l)->value.u.mi);
	    break;
	case FcTypeCharSet:
	    FcCharSetDestroy 
		(FcCharSetPtrU (FcValueListPtrU(l)->value.u.ci));
	    break;
	case FcTypeLangSet:
	    FcLangSetDestroy 
		(FcLangSetPtrU (FcValueListPtrU(l)->value.u.li));
	    break;
	default:
	    break;
	}
	next = FcValueListPtrU(l)->next;

	FcMemFree (FC_MEM_VALLIST, sizeof (FcValueList));
	if (l.storage == FcStorageDynamic)
	    free(l.u.dyn);
    }
}

FcBool
FcValueEqual (FcValue va, FcValue vb)
{
    if (va.type != vb.type)
    {
	if (va.type == FcTypeInteger)
	{
	    va.type = FcTypeDouble;
	    va.u.d = va.u.i;
	}
	if (vb.type == FcTypeInteger)
	{
	    vb.type = FcTypeDouble;
	    vb.u.d = vb.u.i;
	}
	if (va.type != vb.type)
	    return FcFalse;
    }
    switch (va.type) {
    case FcTypeVoid:
	return FcTrue;
    case FcTypeInteger:
	return va.u.i == vb.u.i;
    case FcTypeDouble:
	return va.u.d == vb.u.d;
    case FcTypeString:
	return FcStrCmpIgnoreCase (FcObjectPtrU(va.u.si), 
				   FcObjectPtrU(vb.u.si)) == 0;
    case FcTypeBool:
	return va.u.b == vb.u.b;
    case FcTypeMatrix:
	return FcMatrixEqual (FcMatrixPtrU(va.u.mi), 
			      FcMatrixPtrU(vb.u.mi));
    case FcTypeCharSet:
	return FcCharSetEqual (FcCharSetPtrU(va.u.ci), 
			       FcCharSetPtrU(vb.u.ci));
    case FcTypeFTFace:
	return va.u.f == vb.u.f;
    case FcTypeLangSet:
	return FcLangSetEqual (FcLangSetPtrU(va.u.li), 
			       FcLangSetPtrU(vb.u.li));
    }
    return FcFalse;
}

static FcChar32
FcDoubleHash (double d)
{
    if (d < 0)
	d = -d;
    if (d > 0xffffffff)
	d = 0xffffffff;
    return (FcChar32) d;
}

static FcChar32
FcStringHash (const FcChar8 *s)
{
    FcChar8	c;
    FcChar32	h = 0;
    
    if (s)
	while ((c = *s++))
	    h = ((h << 1) | (h >> 31)) ^ c;
    return h;
}

static FcChar32
FcValueHash (FcValue v)
{
    switch (v.type) {
    case FcTypeVoid:
	return 0;
    case FcTypeInteger:
	return (FcChar32) v.u.i;
    case FcTypeDouble:
	return FcDoubleHash (v.u.d);
    case FcTypeString:
	return FcStringHash (FcObjectPtrU(v.u.si));
    case FcTypeBool:
	return (FcChar32) v.u.b;
    case FcTypeMatrix:
    {
	FcMatrix * m = FcMatrixPtrU(v.u.mi);
	return (FcDoubleHash (m->xx) ^ 
		FcDoubleHash (m->xy) ^ 
		FcDoubleHash (m->yx) ^ 
		FcDoubleHash (m->yy));
    }
    case FcTypeCharSet:
	return (FcChar32) (FcCharSetPtrU(v.u.ci))->num;
    case FcTypeFTFace:
	return FcStringHash ((const FcChar8 *) ((FT_Face) v.u.f)->family_name) ^
	       FcStringHash ((const FcChar8 *) ((FT_Face) v.u.f)->style_name);
    case FcTypeLangSet:
	return FcLangSetHash (FcLangSetPtrU(v.u.li));
    }
    return FcFalse;
}

static FcBool
FcValueListEqual (FcValueListPtr la, FcValueListPtr lb)
{
    if (FcValueListPtrU(la) == FcValueListPtrU(lb))
	return FcTrue;

    while (FcValueListPtrU(la) && FcValueListPtrU(lb))
    {
	if (!FcValueEqual (FcValueListPtrU(la)->value, 
			   FcValueListPtrU(lb)->value))
	    return FcFalse;
	la = FcValueListPtrU(la)->next;
	lb = FcValueListPtrU(lb)->next;
    }
    if (FcValueListPtrU(la) || FcValueListPtrU(lb))
	return FcFalse;
    return FcTrue;
}

static FcChar32
FcValueListHash (FcValueListPtr l)
{
    FcChar32	hash = 0;
    
    while (FcValueListPtrU(l))
    {
	hash = ((hash << 1) | (hash >> 31)) ^ 
	    FcValueHash (FcValueListPtrU(l)->value);
	l = FcValueListPtrU(l)->next;
    }
    return hash;
}

void
FcPatternDestroy (FcPattern *p)
{
    int		    i;
    
    if (p->ref == FC_REF_CONSTANT || --p->ref > 0)
	return;

    for (i = 0; i < p->num; i++)
	FcValueListDestroy ((FcPatternEltU(p->elts)+i)->values);

    p->num = 0;
    if (FcPatternEltU(p->elts) && FcPatternEltIsDynamic(p->elts))
    {
	FcMemFree (FC_MEM_PATELT, p->size * sizeof (FcPatternElt));
	free (FcPatternEltU(p->elts));
	p->elts = FcPatternEltPtrCreateDynamic(0);
    }
    p->size = 0;
    FcMemFree (FC_MEM_PATTERN, sizeof (FcPattern));
    free (p);
}

#define FC_VALUE_LIST_HASH_SIZE	    257
#define FC_PATTERN_HASH_SIZE	    67

typedef struct _FcValueListEnt FcValueListEnt;

struct _FcValueListEnt {
    FcValueListEnt  *next;
    FcValueListPtr  list;
    FcChar32	    hash, pad;
};

typedef union _FcValueListAlign {
    FcValueListEnt  ent;
    FcValueList	    list;
} FcValueListAlign;

static int	    FcValueListFrozenCount[FcTypeLangSet + 1];
static int	    FcValueListFrozenBytes[FcTypeLangSet + 1];
static char	    *FcValueListFrozenName[] = {
    "Void", 
    "Integer", 
    "Double", 
    "String", 
    "Bool",
    "Matrix",
    "CharSet",
    "FTFace",
    "LangSet"
};

void
FcValueListReport (void);
    
void
FcValueListReport (void)
{
    FcType  t;

    printf ("Fc Frozen Values:\n");
    printf ("\t%8s %9s %9s\n", "Type", "Count", "Bytes");
    for (t = FcTypeVoid; t <= FcTypeLangSet; t++)
	printf ("\t%8s %9d %9d\n", FcValueListFrozenName[t],
		FcValueListFrozenCount[t], FcValueListFrozenBytes[t]);
}

static FcValueListEnt *
FcValueListEntCreate (FcValueListPtr h)
{
    FcValueListAlign	*ea;
    FcValueListEnt  *e;
    FcValueListPtr  l;
    FcValueList     *new;
    int		    n;
    int		    size;

    n = 0;
    for (l = h; FcValueListPtrU(l); l = FcValueListPtrU(l)->next)
	n++;
    size = sizeof (FcValueListAlign) + n * sizeof (FcValueList);
    FcValueListFrozenCount[FcValueListPtrU(h)->value.type]++;
    FcValueListFrozenBytes[FcValueListPtrU(h)->value.type] += size;
    // this leaks for some reason
    ea = malloc (sizeof (FcValueListAlign));
    if (!ea)
	return 0;
    new = malloc (n * sizeof (FcValueList));
    if (!new)
        return 0;
    memset(new, 0, n * sizeof (FcValueList));
    FcMemAlloc (FC_MEM_VALLIST, size);
    e = &ea->ent;
    e->list = (FcValueListPtr) FcValueListPtrCreateDynamic(new);
    for (l = h; FcValueListPtrU(l); 
	 l = FcValueListPtrU(l)->next, new++)
    {
	if (FcValueListPtrU(l)->value.type == FcTypeString)
	{
	    new->value.type = FcTypeString;
	    new->value.u.si = FcObjectStaticName
		(FcObjectPtrU(FcValueListPtrU(l)->value.u.si));
	}
	else
	{
	    new->value = FcValueSave (FcValueListPtrU(l)->value);
	}
	new->binding = FcValueListPtrU(l)->binding;
	if (FcValueListPtrU(FcValueListPtrU(l)->next))
	{
	    new->next = FcValueListPtrCreateDynamic(new + 1);
	}
	else
	{
	    new->next = FcValueListPtrCreateDynamic(0);
	}
    }
    return e;
}

static void
FcValueListEntDestroy (FcValueListEnt *e)
{
    FcValueListPtr	l;

    FcValueListFrozenCount[FcValueListPtrU(e->list)->value.type]--;

    /* XXX: We should perform these two operations with "size" as
       computed in FcValueListEntCreate, but we don't have access to
       that value here. Without this, the FcValueListFrozenBytes
       values will be wrong as will the FcMemFree counts.

       FcValueListFrozenBytes[e->list->value.type] -= size;
       FcMemFree (FC_MEM_VALLIST, size);
    */

    for (l = e->list; FcValueListPtrU(l); 
	 l = FcValueListPtrU(l)->next)
    {
	if (FcValueListPtrU(l)->value.type != FcTypeString)
	    FcValueDestroy (FcValueListPtrU(l)->value);
    }
    /* XXX: Are we being too chummy with the implementation here to
       free(e) when it was actually the enclosing FcValueListAlign
       that was allocated? */
    free (e);
}

static int	FcValueListTotal;
static int	FcValueListUsed;

static FcValueListEnt   *FcValueListHashTable[FC_VALUE_LIST_HASH_SIZE];

static FcValueListPtr
FcValueListFreeze (FcValueListPtr l)
{
    FcChar32		    hash = FcValueListHash (l);
    FcValueListEnt	    **bucket = &FcValueListHashTable[hash % FC_VALUE_LIST_HASH_SIZE];
    FcValueListEnt	    *ent;

    FcValueListTotal++;
    for (ent = *bucket; ent; ent = ent->next)
    {
	if (ent->hash == hash && FcValueListEqual (ent->list, l))
	    return ent->list;
    }

    ent = FcValueListEntCreate (l);
    if (!ent)
	return FcValueListPtrCreateDynamic(0);

    FcValueListUsed++;
    ent->hash = hash;
    ent->next = *bucket;
    *bucket = ent;
    return ent->list;
}

static void
FcValueListThawAll (void)
{
    int i;
    FcValueListEnt	*ent, *next;

    for (i = 0; i < FC_VALUE_LIST_HASH_SIZE; i++)
    {
	for (ent = FcValueListHashTable[i]; ent; ent = next)
	{
	    next = ent->next;
	    FcValueListEntDestroy (ent);
	}
	FcValueListHashTable[i] = 0;
    }

    FcValueListTotal = 0;
    FcValueListUsed = 0;
}

static FcChar32
FcPatternBaseHash (FcPattern *b)
{
    FcChar32	hash = b->num;
    int		i;

    for (i = 0; i < b->num; i++)
	hash = ((hash << 1) | (hash >> 31)) ^ 
	    (long) (FcValueListPtrU((FcPatternEltU(b->elts)+i)->values));
    return hash;
}

typedef struct _FcPatternEnt FcPatternEnt;

struct _FcPatternEnt {
    FcPatternEnt    *next;
    FcChar32	    hash;
    FcPattern  	    *pattern;
};

static int	FcPatternTotal;
static int	FcPatternUsed;

static FcPatternEnt	*FcPatternHashTable[FC_VALUE_LIST_HASH_SIZE];

static FcPattern *
FcPatternBaseFreeze (FcPattern *b)
{
    FcPattern           *ep;
    FcPatternElt	*epp;
    FcChar32		hash = FcPatternBaseHash (b);
    FcPatternEnt	**bucket = &FcPatternHashTable[hash % FC_VALUE_LIST_HASH_SIZE];
    FcPatternEnt	*ent;
    int			i;

    FcPatternTotal++;
    for (ent = *bucket; ent; ent = ent->next)
    {
        if (ent->hash == hash && b->num == ent->pattern->num)
        {
	    for (i = 0; i < b->num; i++)
	    {
		if (FcObjectPtrCompare((FcPatternEltU(b->elts)+i)->object,
				       (FcPatternEltU(ent->pattern->elts)+i)->object) != 0)
		    break;
		if (FcValueListPtrU((FcPatternEltU(b->elts)+i)->values) != 
                    FcValueListPtrU((FcPatternEltU(ent->pattern->elts)+i)->values))
		    break;
	    }
	    if (i == b->num)
		return ent->pattern;
	}
    }

    /*
     * Compute size of pattern + elts
     */
    ent = malloc (sizeof (FcPatternEnt));
    if (!ent)
	return 0;

    FcMemAlloc (FC_MEM_PATTERN, sizeof (FcPatternEnt));
    FcPatternUsed++;

    ep = FcPatternCreate();
    if (!ep)
        return 0;
    ent->pattern = ep;
    epp = malloc(b->num * sizeof (FcPatternElt));
    if (!epp)
        goto bail;
    ep->elts = FcPatternEltPtrCreateDynamic(epp);

    FcMemAlloc (FC_MEM_PATELT, sizeof (FcPatternElt)*(b->num));

    ep->num = b->num;
    ep->size = b->num;
    ep->ref = FC_REF_CONSTANT;

    for (i = 0; i < b->num; i++)
    {
	(FcPatternEltU(ep->elts)+i)->values = 
	    (FcPatternEltU(b->elts)+i)->values;
	(FcPatternEltU(ep->elts)+i)->object = 
	    (FcPatternEltU(b->elts)+i)->object;
    }

    ent->hash = hash;
    ent->next = *bucket;
    *bucket = ent;
    return ent->pattern;
 bail:
    free(ent);
    FcMemFree (FC_MEM_PATTERN, sizeof (FcPatternEnt));
    FcPatternUsed--;
    return 0;
}

static void
FcPatternBaseThawAll (void)
{
    int i;
    FcPatternEnt	*ent, *next;

    for (i = 0; i < FC_VALUE_LIST_HASH_SIZE; i++)
    {
	for (ent = FcPatternHashTable[i]; ent; ent = next)
	{
	    next = ent->next;
	    free (ent);
	}
	FcPatternHashTable[i] = 0;
    }

    FcPatternTotal = 0;
    FcPatternUsed = 0;
}

FcPattern *
FcPatternFreeze (FcPattern *p)
{
    FcPattern	*b, *n = 0;
    FcPatternElt *e;
    int		i;
    
    if (p->ref == FC_REF_CONSTANT)
       return p;

    b = FcPatternCreate();
    if (!b)
        return 0;

    b->num = p->num;
    b->size = b->num;
    b->ref = 1;

    e = malloc(b->num * sizeof (FcPatternElt));
    if (!e)
        return 0;
    b->elts = FcPatternEltPtrCreateDynamic(e);
    FcMemAlloc (FC_MEM_PATELT, sizeof (FcPatternElt)*(b->num));

    /*
     * Freeze object lists
     */
    for (i = 0; i < p->num; i++)
    {
	(FcPatternEltU(b->elts)+i)->object = 
	    (FcPatternEltU(p->elts)+i)->object;
	(FcPatternEltU(b->elts)+i)->values = 
	    FcValueListFreeze((FcPatternEltU(p->elts)+i)->values);
	if (!FcValueListPtrU((FcPatternEltU(p->elts)+i)->values))
	    goto bail;
    }
    /*
     * Freeze base
     */
    n = FcPatternBaseFreeze (b);
#ifdef CHATTY
    if (FcDebug() & FC_DBG_MEMORY)
    {
	printf ("ValueLists: total %9d used %9d\n", FcValueListTotal, FcValueListUsed);
	printf ("Patterns:   total %9d used %9d\n", FcPatternTotal, FcPatternUsed);
    }
#endif
 bail:
    free(FcPatternEltU(b->elts));
    b->elts = FcPatternEltPtrCreateDynamic(0);
    FcMemFree (FC_MEM_PATELT, sizeof (FcPatternElt)*(b->num));
    b->num = -1;
#ifdef DEBUG
    assert (FcPatternEqual (n, p));
#endif
    return n;
}

void
FcPatternThawAll (void)
{
    FcPatternBaseThawAll ();
    FcValueListThawAll ();
}

static int
FcPatternPosition (const FcPattern *p, const char *object)
{
    int	    low, high, mid, c;
    FcObjectPtr obj;

    obj = FcObjectStaticName(object);
    low = 0;
    high = p->num - 1;
    c = 1;
    mid = 0;
    while (low <= high)
    {
	mid = (low + high) >> 1;
	c = FcObjectPtrCompare((FcPatternEltU(p->elts)+mid)->object, obj);
	if (c == 0)
	    return mid;
	if (c < 0)
	    low = mid + 1;
	else
	    high = mid - 1;
    }
    if (c < 0)
	mid++;
    return -(mid + 1);
}

FcPatternElt *
FcPatternFindElt (const FcPattern *p, const char *object)
{
    int	    i = FcPatternPosition (p, object);
    if (i < 0)
	return 0;
    return FcPatternEltU(p->elts)+i;
}

FcPatternElt *
FcPatternInsertElt (FcPattern *p, const char *object)
{
    int		    i;
    FcPatternElt   *e;
    
    i = FcPatternPosition (p, object);
    if (i < 0)
    {
	i = -i - 1;
    
	/* reallocate array */
	if (p->num + 1 >= p->size)
	{
	    int s = p->size + 16;
	    if (FcPatternEltU(p->elts))
	    {
		FcPatternElt *e0 = FcPatternEltU(p->elts);
		e = (FcPatternElt *) realloc (e0, s * sizeof (FcPatternElt));
		if (!e) /* maybe it was mmapped */
		{
		    e = malloc(s * sizeof (FcPatternElt));
		    if (e)
			memcpy(e, e0, p->num * sizeof (FcPatternElt));
		}
	    }
	    else
		e = (FcPatternElt *) malloc (s * sizeof (FcPatternElt));
	    if (!e)
		return FcFalse;
	    p->elts = FcPatternEltPtrCreateDynamic(e);
	    if (p->size)
		FcMemFree (FC_MEM_PATELT, p->size * sizeof (FcPatternElt));
	    FcMemAlloc (FC_MEM_PATELT, s * sizeof (FcPatternElt));
	    while (p->size < s)
	    {
		(FcPatternEltU(p->elts)+p->size)->object = FcObjectPtrCreateDynamic(0);
		(FcPatternEltU(p->elts)+p->size)->values = 
		    FcValueListPtrCreateDynamic(0);
		p->size++;
	    }
	}
	
	/* move elts up */
	memmove (FcPatternEltU(p->elts) + i + 1,
		 FcPatternEltU(p->elts) + i,
		 sizeof (FcPatternElt) *
		 (p->num - i));
		 
	/* bump count */
	p->num++;
	
	(FcPatternEltU(p->elts)+i)->object = FcObjectStaticName (object);
	(FcPatternEltU(p->elts)+i)->values = FcValueListPtrCreateDynamic(0);
    }
    
    return FcPatternEltU(p->elts)+i;
}

FcBool
FcPatternEqual (const FcPattern *pa, const FcPattern *pb)
{
    int	i;

    if (pa == pb)
	return FcTrue;

    if (pa->num != pb->num)
	return FcFalse;
    for (i = 0; i < pa->num; i++)
    {
	if (FcObjectPtrCompare((FcPatternEltU(pa->elts)+i)->object,
			       (FcPatternEltU(pb->elts)+i)->object) != 0)
	    return FcFalse;
	if (!FcValueListEqual ((FcPatternEltU(pa->elts)+i)->values, 
			       (FcPatternEltU(pb->elts)+i)->values))
	    return FcFalse;
    }
    return FcTrue;
}

FcChar32
FcPatternHash (const FcPattern *p)
{
    int		i;
    FcChar32	h = 0;

    for (i = 0; i < p->num; i++)
    {
	h = (((h << 1) | (h >> 31)) ^ 
	     FcStringHash ((const FcChar8 *) FcObjectPtrU(((FcPatternEltU(p->elts)+i)->object))) ^
	     FcValueListHash ((FcPatternEltU(p->elts)+i)->values));
    }
    return h;
}

FcBool
FcPatternEqualSubset (const FcPattern *pai, const FcPattern *pbi, const FcObjectSet *os)
{
    FcPatternElt    *ea, *eb;
    int		    i;
    
    for (i = 0; i < os->nobject; i++)
    {
	ea = FcPatternFindElt (pai, FcObjectPtrU(os->objects[i]));
	eb = FcPatternFindElt (pbi, FcObjectPtrU(os->objects[i]));
	if (ea)
	{
	    if (!eb)
		return FcFalse;
	    if (!FcValueListEqual (ea->values, eb->values))
		return FcFalse;
	}
	else
	{
	    if (eb)
		return FcFalse;
	}
    }
    return FcTrue;
}

FcBool
FcPatternAddWithBinding  (FcPattern	    *p,
			  const char	    *object,
			  FcValue	    value,
			  FcValueBinding    binding,
			  FcBool	    append)
{
    FcPatternElt   *e;
    FcValueListPtr new, *prev;
    FcValueList *  newp;

    if (p->ref == FC_REF_CONSTANT)
	goto bail0;

    newp = malloc (sizeof (FcValueList));
    if (!newp)
	goto bail0;

    memset(newp, 0, sizeof (FcValueList));
    new = FcValueListPtrCreateDynamic(newp);
    FcMemAlloc (FC_MEM_VALLIST, sizeof (FcValueList));
    /* dup string */
    value = FcValueSave (value);
    if (value.type == FcTypeVoid)
	goto bail1;

    FcValueListPtrU(new)->value = value;
    FcValueListPtrU(new)->binding = binding;
    FcValueListPtrU(new)->next = FcValueListPtrCreateDynamic(0);
    
    e = FcPatternInsertElt (p, object);
    if (!e)
	goto bail2;
    
    if (append)
    {
	for (prev = &e->values; FcValueListPtrU(*prev); prev = &FcValueListPtrU(*prev)->next)
	    ;
	*prev = new;
    }
    else
    {
	FcValueListPtrU(new)->next = e->values;
	e->values = new;
    }
    
    return FcTrue;

bail2:    
    switch (value.type) {
    case FcTypeString:
	FcStrFree ((FcChar8 *) FcObjectPtrU(value.u.si));
	break;
    case FcTypeMatrix:
	FcMatrixFree (FcMatrixPtrU(value.u.mi));
	break;
    case FcTypeCharSet:
	FcCharSetDestroy (FcCharSetPtrU(value.u.ci));
	break;
    case FcTypeLangSet:
	FcLangSetDestroy (FcLangSetPtrU(value.u.li));
	break;
    default:
	break;
    }
bail1:
    FcMemFree (FC_MEM_VALLIST, sizeof (FcValueList));
    free (FcValueListPtrU(new));
bail0:
    return FcFalse;
}

FcBool
FcPatternAdd (FcPattern *p, const char *object, FcValue value, FcBool append)
{
    return FcPatternAddWithBinding (p, object, value, FcValueBindingStrong, append);
}

FcBool
FcPatternAddWeak  (FcPattern *p, const char *object, FcValue value, FcBool append)
{
    return FcPatternAddWithBinding (p, object, value, FcValueBindingWeak, append);
}

FcBool
FcPatternDel (FcPattern *p, const char *object)
{
    FcPatternElt   *e;

    e = FcPatternFindElt (p, object);
    if (!e)
	return FcFalse;

    /* destroy value */
    FcValueListDestroy (e->values);
    
    /* shuffle existing ones down */
    memmove (e, e+1, 
	     (FcPatternEltU(p->elts) + p->num - (e + 1)) * 
	     sizeof (FcPatternElt));
    p->num--;
    (FcPatternEltU(p->elts)+p->num)->object = FcObjectPtrCreateDynamic(0);
    (FcPatternEltU(p->elts)+p->num)->values = FcValueListPtrCreateDynamic(0);
    return FcTrue;
}

FcBool
FcPatternRemove (FcPattern *p, const char *object, int id)
{
    FcPatternElt    *e;
    FcValueListPtr  *prev, l;

    e = FcPatternFindElt (p, object);
    if (!e)
	return FcFalse;
    for (prev = &e->values; 
	 FcValueListPtrU(l = *prev); 
	 prev = &FcValueListPtrU(l)->next)
    {
	if (!id)
	{
	    *prev = FcValueListPtrU(l)->next;
	    FcValueListPtrU(l)->next = FcValueListPtrCreateDynamic(0);
	    FcValueListDestroy (l);
	    if (!FcValueListPtrU(e->values))
		FcPatternDel (p, object);
	    return FcTrue;
	}
	id--;
    }
    return FcFalse;
}

FcBool
FcPatternAddInteger (FcPattern *p, const char *object, int i)
{
    FcValue	v;

    v.type = FcTypeInteger;
    v.u.i = i;
    return FcPatternAdd (p, object, v, FcTrue);
}

FcBool
FcPatternAddDouble (FcPattern *p, const char *object, double d)
{
    FcValue	v;

    v.type = FcTypeDouble;
    v.u.d = d;
    return FcPatternAdd (p, object, v, FcTrue);
}


FcBool
FcPatternAddString (FcPattern *p, const char *object, const FcChar8 *s)
{
    FcValue	v;

    v.type = FcTypeString;
    v.u.si = FcObjectPtrCreateDynamic(s);
    return FcPatternAdd (p, object, v, FcTrue);
}

FcBool
FcPatternAddMatrix (FcPattern *p, const char *object, const FcMatrix *s)
{
    FcValue	v;

    v.type = FcTypeMatrix;
    v.u.mi = FcMatrixPtrCreateDynamic((FcMatrix *) s);
    return FcPatternAdd (p, object, v, FcTrue);
}


FcBool
FcPatternAddBool (FcPattern *p, const char *object, FcBool b)
{
    FcValue	v;

    v.type = FcTypeBool;
    v.u.b = b;
    return FcPatternAdd (p, object, v, FcTrue);
}

FcBool
FcPatternAddCharSet (FcPattern *p, const char *object, const FcCharSet *c)
{
    FcValue	v;

    v.type = FcTypeCharSet;
    v.u.ci = FcCharSetPtrCreateDynamic((FcCharSet *)c);
    return FcPatternAdd (p, object, v, FcTrue);
}

FcBool
FcPatternAddFTFace (FcPattern *p, const char *object, const FT_Face f)
{
    FcValue	v;

    v.type = FcTypeFTFace;
    v.u.f = (void *) f;
    return FcPatternAdd (p, object, v, FcTrue);
}

FcBool
FcPatternAddLangSet (FcPattern *p, const char *object, const FcLangSet *ls)
{
    FcValue	v;

    v.type = FcTypeLangSet;
    v.u.li = FcLangSetPtrCreateDynamic((FcLangSet *)ls);
    return FcPatternAdd (p, object, v, FcTrue);
}

FcResult
FcPatternGet (const FcPattern *p, const char *object, int id, FcValue *v)
{
    FcPatternElt   *e;
    FcValueListPtr l;

    e = FcPatternFindElt (p, object);
    if (!e)
	return FcResultNoMatch;
    for (l = e->values; FcValueListPtrU(l); l = FcValueListPtrU(l)->next)
    {
	if (!id)
	{
	    *v = FcValueListPtrU(l)->value;
	    return FcResultMatch;
	}
	id--;
    }
    return FcResultNoId;
}

FcResult
FcPatternGetInteger (const FcPattern *p, const char *object, int id, int *i)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    switch (v.type) {
    case FcTypeDouble:
	*i = (int) v.u.d;
	break;
    case FcTypeInteger:
	*i = v.u.i;
	break;
    default:
        return FcResultTypeMismatch;
    }
    return FcResultMatch;
}

FcResult
FcPatternGetDouble (const FcPattern *p, const char *object, int id, double *d)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    switch (v.type) {
    case FcTypeDouble:
	*d = v.u.d;
	break;
    case FcTypeInteger:
	*d = (double) v.u.i;
	break;
    default:
        return FcResultTypeMismatch;
    }
    return FcResultMatch;
}

FcResult
FcPatternGetString (const FcPattern *p, const char *object, int id, FcChar8 ** s)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    if (v.type != FcTypeString)
        return FcResultTypeMismatch;
    *s = (FcChar8 *) FcObjectPtrU(v.u.si);
    return FcResultMatch;
}

FcResult
FcPatternGetMatrix(const FcPattern *p, const char *object, int id, FcMatrix **m)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    if (v.type != FcTypeMatrix)
        return FcResultTypeMismatch;
    *m = FcMatrixPtrU(v.u.mi);
    return FcResultMatch;
}


FcResult
FcPatternGetBool(const FcPattern *p, const char *object, int id, FcBool *b)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    if (v.type != FcTypeBool)
        return FcResultTypeMismatch;
    *b = v.u.b;
    return FcResultMatch;
}

FcResult
FcPatternGetCharSet(const FcPattern *p, const char *object, int id, FcCharSet **c)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    if (v.type != FcTypeCharSet)
        return FcResultTypeMismatch;
    *c = FcCharSetPtrU(v.u.ci);
    return FcResultMatch;
}

FcResult
FcPatternGetFTFace(const FcPattern *p, const char *object, int id, FT_Face *f)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    if (v.type != FcTypeFTFace)
	return FcResultTypeMismatch;
    *f = (FT_Face) v.u.f;
    return FcResultMatch;
}

FcResult
FcPatternGetLangSet(const FcPattern *p, const char *object, int id, FcLangSet **ls)
{
    FcValue	v;
    FcResult	r;

    r = FcPatternGet (p, object, id, &v);
    if (r != FcResultMatch)
	return r;
    if (v.type != FcTypeLangSet)
        return FcResultTypeMismatch;
    *ls = FcLangSetPtrU(v.u.li);
    return FcResultMatch;
}

FcPattern *
FcPatternDuplicate (const FcPattern *orig)
{
    FcPattern	    *new;
    FcPatternElt    *e;
    int		    i;
    FcValueListPtr  l;

    new = FcPatternCreate ();
    if (!new)
	goto bail0;

    e = FcPatternEltU(orig->elts);

    for (i = 0; i < orig->num; i++)
    {
	for (l = (e + i)->values; 
	     FcValueListPtrU(l); 
	     l = FcValueListPtrU(l)->next)
	    if (!FcPatternAdd (new, FcObjectPtrU((e + i)->object), 
                               FcValueListPtrU(l)->value, FcTrue))
		goto bail1;
    }

    return new;

bail1:
    FcPatternDestroy (new);
bail0:
    return 0;
}

void
FcPatternReference (FcPattern *p)
{
    if (p->ref != FC_REF_CONSTANT)
	p->ref++;
}

FcPattern *
FcPatternVaBuild (FcPattern *orig, va_list va)
{
    FcPattern	*ret;
    
    FcPatternVapBuild (ret, orig, va);
    return ret;
}

FcPattern *
FcPatternBuild (FcPattern *orig, ...)
{
    va_list	va;
    
    va_start (va, orig);
    FcPatternVapBuild (orig, orig, va);
    va_end (va);
    return orig;
}

/*
 * Add all of the elements in 's' to 'p'
 */
FcBool
FcPatternAppend (FcPattern *p, FcPattern *s)
{
    int		    i;
    FcPatternElt    *e;
    FcValueListPtr  v;
    
    for (i = 0; i < s->num; i++)
    {
	e = FcPatternEltU(s->elts)+i;
	for (v = e->values; FcValueListPtrU(v); 
	     v = FcValueListPtrU(v)->next)
	{
	    if (!FcPatternAddWithBinding (p, FcObjectPtrU(e->object),
					  FcValueListPtrU(v)->value, 
					  FcValueListPtrU(v)->binding, FcTrue))
		return FcFalse;
	}
    }
    return FcTrue;
}

#define OBJECT_HASH_SIZE    31
struct objectBucket {
    struct objectBucket	*next;
    FcChar32		hash;
};
static struct objectBucket **buckets = 0;

FcObjectPtr
FcObjectStaticName (const char *name)
{
    FcChar32		hash = FcStringHash ((const FcChar8 *) name);
    struct objectBucket	**p;
    struct objectBucket	*b;
    const char *        nn;
    int			size;
    FcObjectPtr		new;

    if (!buckets)
    {
        buckets = malloc(sizeof (struct objectBucket *)*OBJECT_HASH_SIZE);
        memset (buckets, 0, sizeof (struct objectBucket *)*OBJECT_HASH_SIZE);
    }

    for (p = &buckets[hash % OBJECT_HASH_SIZE]; (b = *p); p = &(b->next))
	if (b->hash == hash && !strcmp (name, FcObjectPtrU(*((FcObjectPtr *) (b + 1)))))
	    return *((FcObjectPtr *) (b + 1));
    size = sizeof (struct objectBucket) + sizeof (FcObjectPtr) + 1;
    b = malloc (size);
    FcMemAlloc (FC_MEM_STATICSTR, size);
    if (!b)
	return FcObjectPtrCreateDynamic(0);
    b->next = 0;
    b->hash = hash;
    nn = malloc(strlen(name)+1);
    if (!nn)
        goto bail;
    strcpy ((char *)nn, name);
    new = FcObjectPtrCreateDynamic ((char *) nn);
    *((FcObjectPtr *)(b+1)) = new;
    *p = b;
    return new;

 bail:
    free(b);
    return FcObjectPtrCreateDynamic(0);
}

FcPatternElt *
FcPatternEltU (FcPatternEltPtr pei)
{
    switch (pei.storage)
    {
    case FcStorageStatic:
	if (pei.u.stat == 0) return 0;
        return &fcpatternelts[pei.u.stat];
    case FcStorageDynamic:
        return pei.u.dyn;
    default:
	return 0;
    }
}

static FcPatternEltPtr
FcPatternEltPtrCreateDynamic (FcPatternElt * e)
{
    FcPatternEltPtr new;
    new.storage = FcStorageDynamic;
    new.u.dyn = e;
    return new;
}

static FcPatternEltPtr
FcPatternEltPtrCreateStatic (int i)
{
    FcPatternEltPtr new;
    new.storage = FcStorageStatic;
    new.u.stat = i;
    return new;
}

static FcBool
FcPatternEltIsDynamic (FcPatternEltPtr pei)
{
    return pei.storage == FcStorageDynamic;
}

struct objectTree {
  struct objectTree * left, * right;
  char * s;
};

FcObjectPtr
FcObjectPtrCreateDynamic (const char * s)
{
    FcObjectPtr new;
    new.storage = FcStorageDynamic;
    new.u.dyn = s;
    if (s)
	new.hash = FcStringHash(s);
    else
	new.hash = 0;
    return new;
}

void
FcObjectPtrDestroy (FcObjectPtr p)
{
    if (p.storage == FcStorageDynamic)
	FcStrFree ((char *)p.u.dyn);
}

const char *
FcObjectPtrU (FcObjectPtr si)
{
    switch (si.storage)
    {
    case FcStorageStatic:
	if (si.u.stat == 0) return 0;
        return &object_content[si.u.stat];
    case FcStorageDynamic:
        return si.u.dyn;
    default:
	return 0;
    }
}

int
FcObjectPtrCompare (const FcObjectPtr a, const FcObjectPtr b)
{
    int r = a.hash - b.hash;

    if (r == 0)
	return strcmp (FcObjectPtrU(a), FcObjectPtrU(b));
    return r;
}

void
FcObjectClearStatic(void)
{
    object_content = 0;
    object_content_count = 0;
    object_content_ptr = 0;
}

static FcObjectPtr
FcObjectSerialize (FcObjectPtr si)
{
    struct objectBucket	**p;
    struct objectBucket	*b;

    if (!object_content)
    {
	object_content = malloc(object_content_count * sizeof(char));
	if (!object_content)
	    return FcObjectPtrCreateDynamic(0);
    }

    if (!buckets)
	return FcObjectPtrCreateDynamic(0);

    for (p = &buckets[si.hash % OBJECT_HASH_SIZE]; (b = *p); p = &(b->next))
	if (b->hash == si.hash && !strcmp (FcObjectPtrU(si), FcObjectPtrU(*((FcObjectPtr *) (b + 1)))))
	{
	    FcObjectPtr *op = (FcObjectPtr *) (b + 1);
	    if (op->storage == FcStorageStatic)
		return *op;

	    if (object_content_ptr >= object_content_count)
		return FcObjectPtrCreateDynamic(0);

	    strcpy (object_content+object_content_ptr, 
		    FcObjectPtrU(si));

	    op->storage = FcStorageStatic;
	    op->u.stat = object_content_ptr;
	    
	    object_content_ptr += strlen(FcObjectPtrU(si))+1;

	    return *op;
	}

    return FcObjectPtrCreateDynamic(0);
}

FcBool
FcObjectPrepareSerialize (FcObjectPtr si)
{
    object_content_count += strlen(FcObjectPtrU(si)) + 1;
    return FcTrue;
}

void
FcPatternClearStatic (void)
{
    fcpatterns = 0;
    fcpattern_ptr = 0;
    fcpattern_count = 0;

    fcpatternelts = 0;
    fcpatternelt_ptr = 0;
    fcpatternelt_count = 0;
}

void
FcValueListClearStatic (void)
{
    fcvaluelists = 0;
    fcvaluelist_ptr = 0;
    fcvaluelist_count = 0;
}

FcBool
FcPatternPrepareSerialize (FcPattern * p)
{
    int i;

    fcpattern_count++;
    fcpatternelt_count += p->num;

    for (i = 0; i < p->num; i++)
    {
	FcObjectPrepareSerialize 
	    ((FcPatternEltU(p->elts)+i)->object);
	if (!FcValueListPrepareSerialize 
	    (FcValueListPtrU(((FcPatternEltU(p->elts)+i)->values))))
	    return FcFalse;
    }

    return FcTrue;
}

FcBool
FcValueListPrepareSerialize (FcValueList *p)
{
    FcValueList *vl;

    for (vl = p;
	 vl; 
	 vl = FcValueListPtrU(vl->next))
    {
	FcValue v = vl->value;

	switch (v.type)
	{
	case FcTypeMatrix:
	    FcMatrixPrepareSerialize(FcMatrixPtrU(v.u.mi));
	    break;
	case FcTypeCharSet:
	    FcCharSetPrepareSerialize(FcCharSetPtrU(v.u.ci));
	    break;
	case FcTypeLangSet:
	    FcLangSetPrepareSerialize(FcLangSetPtrU(v.u.li));
	    break;
	case FcTypeString:
	    FcObjectPrepareSerialize(v.u.si);
	default:
	    break;
	}
	fcvaluelist_count++;
    }
    
    return FcTrue;
}

FcPattern *
FcPatternSerialize (FcPattern *old)
{
    FcPattern *p;
    FcPatternElt *e, *nep;
    FcValueList * nv;
    FcValueListPtr v, nv_head, nvp;
    int i, elts;

    if (!fcpatterns)
    {
	p = malloc (sizeof (FcPattern) * fcpattern_count);
	if (!p)
	    goto bail;

	FcMemAlloc (FC_MEM_PATTERN, sizeof (FcPattern) * fcpattern_count);
	fcpatterns = p;
	fcpattern_ptr = 0;

	e = malloc (sizeof (FcPatternElt) * fcpatternelt_count);
	if (!e)
	    goto bail1;

	FcMemAlloc (FC_MEM_PATELT, sizeof (FcPatternElt) * fcpatternelt_count);
	fcpatternelts = e;
	fcpatternelt_ptr = 0;
    }

    p = FcPatternCreate();
    elts = fcpatternelt_ptr;
    nep = &fcpatternelts[elts];
    if (!nep)
	return FcFalse;
    fcpatternelt_ptr += old->num;
    
    for (e = FcPatternEltU(old->elts), i=0; i < old->num; i++, e++) 
    {
        v = e->values;
        nvp = nv_head = FcValueListSerialize(FcValueListPtrU(v));
        if (!FcValueListPtrU(nv_head))
            goto bail2;
	nv = FcValueListPtrU(nvp);
	
        for (;
             FcValueListPtrU(v);
             v = FcValueListPtrU(v)->next, 
		 nv = FcValueListPtrU(nv->next))
        {
	    
	    if (FcValueListPtrU(FcValueListPtrU(v)->next))
	    {
                nvp = FcValueListSerialize
		    (FcValueListPtrU(FcValueListPtrU(v)->next));
                nv->next = nvp;
	    }
        }
	
	nep[i].values = nv_head;
	nep[i].object = FcObjectSerialize
	    (FcObjectStaticName(FcObjectPtrU(e->object)));
    }
    
    p->elts = FcPatternEltPtrCreateStatic(elts);
    p->size = old->num;
    p->ref = FC_REF_CONSTANT;
    return p;
    
 bail2:
    free (fcpatternelts);
 bail1:
    free (fcpatterns);
 bail:
    return 0;
 }

FcValueListPtr
FcValueListSerialize(FcValueList *pi)
{
    FcValueListPtr new; 
    FcValue * v;
    FcValueList * vl;

    if (!fcvaluelists)
    {
	vl = malloc (sizeof (FcValueList) * fcvaluelist_count);
	if (!vl)
	    return FcValueListPtrCreateDynamic(0);

	FcMemAlloc (FC_MEM_VALLIST, sizeof (FcValueList) * fcvaluelist_count);
	fcvaluelists = vl;
	fcvaluelist_ptr = 0;
    }

    fcvaluelists[fcvaluelist_ptr] = *pi;
    new.storage = FcStorageStatic;
    new.u.stat = fcvaluelist_ptr++;
    v = &fcvaluelists[new.u.stat].value;
    switch (v->type)
    {
    case FcTypeString:
	if (FcObjectPtrU(v->u.si))
	{
	    FcObjectPtr si = 
		FcObjectSerialize(FcObjectStaticName(FcObjectPtrU(v->u.si)));
	    if (!FcObjectPtrU(v->u.si))
		return FcValueListPtrCreateDynamic(pi);
	    v->u.si = si;
	}
	break;
    case FcTypeMatrix:
	if (FcMatrixPtrU(v->u.mi))
	{
	    FcMatrixPtr mi = FcMatrixSerialize(FcMatrixPtrU(v->u.mi));

	    if (!FcMatrixPtrU(mi))
		return FcValueListPtrCreateDynamic(pi);
	    v->u.mi = mi;
	}
	break;
    case FcTypeCharSet:
	if (FcCharSetPtrU(v->u.ci))
	{
	    FcCharSetPtr ci = FcCharSetSerialize(FcCharSetPtrU(v->u.ci));
	    if (!FcCharSetPtrU(v->u.ci))
		return FcValueListPtrCreateDynamic(pi);
	    v->u.ci = ci;
	}
	break;
    case FcTypeLangSet:
	if (FcLangSetPtrU(v->u.li))
	{
	    FcLangSetPtr li = FcLangSetSerialize(FcLangSetPtrU(v->u.li));
	    if (!FcLangSetPtrU(v->u.li))
		return FcValueListPtrCreateDynamic(pi);
	    v->u.li = li;
	}
	break;
    default:
	break;
    }
    return new;
}

FcValueList * 
FcValueListPtrU (FcValueListPtr pi)
{
    switch (pi.storage)
    {
    case FcStorageStatic:
	if (pi.u.stat == 0) return 0;
        return &fcvaluelists[pi.u.stat];
    case FcStorageDynamic:
        return pi.u.dyn;
    default:
	return 0;
    }
}

FcValueListPtr
FcValueListPtrCreateDynamic(FcValueList * p)
{
    FcValueListPtr r; 

    r.storage = FcStorageDynamic; 
    r.u.dyn = p;
    return r;
}
