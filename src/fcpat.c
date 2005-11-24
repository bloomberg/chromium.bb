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
#include "fcint.h"

static FcPattern ** fcpatterns = 0;
static int fcpattern_bank_count = 0, fcpattern_ptr, fcpattern_count;
static FcPatternElt ** fcpatternelts = 0;
static int fcpatternelt_ptr, fcpatternelt_count;
static FcValueList ** fcvaluelists = 0;
static int fcvaluelist_bank_count = 0, fcvaluelist_ptr, fcvaluelist_count;

static const char *
FcPatternFindFullFname (const FcPattern *p);
static FcPatternEltPtr
FcPatternEltPtrCreateDynamic (FcPatternElt * e);

/* If you are trying to duplicate an FcPattern which will be used for
 * rendering, be aware that (internally) you also have to use
 * FcPatternTransferFullFname to transfer the associated filename.  If
 * you are copying the font (externally) using FcPatternGetString,
 * then everything's fine; this caveat only applies if you're copying
 * the bits individually.  */

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
    p->bank = FC_BANK_DYNAMIC;
    p->ref = 1;
    return p;
}

void
FcValueDestroy (FcValue v)
{
    switch (v.type) {
    case FcTypeString:
	FcStrFree ((FcChar8 *) v.u.s);
	break;
    case FcTypeMatrix:
	FcMatrixFree ((FcMatrix *) v.u.m);
	break;
    case FcTypeCharSet:
	FcCharSetDestroy ((FcCharSet *) v.u.c);
	break;
    case FcTypeLangSet:
	FcLangSetDestroy ((FcLangSet *) v.u.l);
	break;
    default:
	break;
    }
}

FcValue
FcValueCanonicalize (const FcValue *v)
{
    if (v->type & FC_STORAGE_STATIC)
    {
	FcValue new = *v;

	switch (v->type & ~FC_STORAGE_STATIC)
	{
	case FcTypeString:
	    new.u.s = fc_value_string(v);
	    new.type = FcTypeString;
	    break;
	case FcTypeCharSet:
	    new.u.c = fc_value_charset(v);
	    new.type = FcTypeCharSet;
	    break;
	case FcTypeLangSet:
	    new.u.l = fc_value_langset(v);
	    new.type = FcTypeLangSet;
	    break;
	}
	return new;
    }
    return *v;
}

FcValue
FcValueSave (FcValue v)
{
    switch (v.type) {
    case FcTypeString:
	v.u.s = FcStrCopy (v.u.s);
	if (!v.u.s)
	    v.type = FcTypeVoid;
	break;
    case FcTypeMatrix:
	v.u.m = FcMatrixCopy (v.u.m);
	if (!v.u.m)
	    v.type = FcTypeVoid;
	break;
    case FcTypeCharSet:
	v.u.c = FcCharSetCopy ((FcCharSet *) v.u.c);
	if (!v.u.c)
	    v.type = FcTypeVoid;
	break;
    case FcTypeLangSet:
	v.u.l = FcLangSetCopy (v.u.l);
	if (!v.u.l)
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
	    FcStrFree ((FcChar8 *)FcValueListPtrU(l)->value.u.s);
	    break;
	case FcTypeMatrix:
	    FcMatrixFree ((FcMatrix *)FcValueListPtrU(l)->value.u.m);
	    break;
	case FcTypeCharSet:
	    FcCharSetDestroy 
		((FcCharSet *) (FcValueListPtrU(l)->value.u.c));
	    break;
	case FcTypeLangSet:
	    FcLangSetDestroy 
		((FcLangSet *) (FcValueListPtrU(l)->value.u.l));
	    break;
	default:
	    break;
	}
	next = FcValueListPtrU(l)->next;
	FcMemFree (FC_MEM_VALLIST, sizeof (FcValueList));
	if (l.bank == FC_BANK_DYNAMIC)
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
	return FcStrCmpIgnoreCase (va.u.s, vb.u.s) == 0;
    case FcTypeBool:
	return va.u.b == vb.u.b;
    case FcTypeMatrix:
	return FcMatrixEqual (va.u.m, vb.u.m);
    case FcTypeCharSet:
	return FcCharSetEqual (va.u.c, vb.u.c);
    case FcTypeFTFace:
	return va.u.f == vb.u.f;
    case FcTypeLangSet:
	return FcLangSetEqual (va.u.l, vb.u.l);
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

FcChar32
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
FcValueHash (const FcValue *v0)
{
    FcValue v = FcValueCanonicalize(v0);
    switch (v.type) {
    case FcTypeVoid:
	return 0;
    case FcTypeInteger:
	return (FcChar32) v.u.i;
    case FcTypeDouble:
	return FcDoubleHash (v.u.d);
    case FcTypeString:
	return FcStringHash (v.u.s);
    case FcTypeBool:
	return (FcChar32) v.u.b;
    case FcTypeMatrix:
	return (FcDoubleHash (v.u.m->xx) ^ 
		FcDoubleHash (v.u.m->xy) ^ 
		FcDoubleHash (v.u.m->yx) ^ 
		FcDoubleHash (v.u.m->yy));
    case FcTypeCharSet:
	return (FcChar32) v.u.c->num;
    case FcTypeFTFace:
	return FcStringHash ((const FcChar8 *) ((FT_Face) v.u.f)->family_name) ^
	       FcStringHash ((const FcChar8 *) ((FT_Face) v.u.f)->style_name);
    case FcTypeLangSet:
	return FcLangSetHash (v.u.l);
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
	    FcValueHash (&FcValueListPtrU(l)->value);
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

    if (FcPatternFindFullFname (p))
    {
	FcStrFree ((FcChar8 *)FcPatternFindFullFname (p));
	FcPatternAddFullFname (p, 0);
    }

    for (i = 0; i < p->num; i++)
	FcValueListDestroy ((FcPatternEltU(p->elts)+i)->values);

    p->num = 0;
    if (FcPatternEltU(p->elts) && p->elts.bank == FC_BANK_DYNAMIC)
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
static char	    FcValueListFrozenName[][8] = {
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
	if ((FcValueListPtrU(l)->value.type & ~FC_STORAGE_STATIC) == FcTypeString)
	{
	    new->value.type = FcTypeString;
	    new->value.u.s = FcStrStaticName
		(fc_value_string(&FcValueListPtrU(l)->value));
	}
	else
	{
	    new->value = FcValueSave (FcValueCanonicalize
				      (&FcValueListPtrU(l)->value));
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

    if (FcPatternFindElt (b, FC_FILE))
	FcPatternTransferFullFname (ep, b);

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

    if (FcPatternFindElt (p, FC_FILE))
	FcPatternTransferFullFname (b, p);

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

static int
FcPatternPosition (const FcPattern *p, const char *object)
{
    int	    low, high, mid, c;
    FcObjectPtr obj;

    obj = FcObjectToPtr(object);
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
		(FcPatternEltU(p->elts)+p->size)->object = 0;
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
	
	(FcPatternEltU(p->elts)+i)->object = FcObjectToPtr (object);
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
	     FcStringHash ((FcChar8 *)FcObjectPtrU ((FcPatternEltU(p->elts)+i)->object)) ^
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
	ea = FcPatternFindElt (pai, os->objects[i]);
	eb = FcPatternFindElt (pbi, os->objects[i]);
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
    FcValueList    *newp;
    FcObjectPtr    objectPtr;

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

    /* quick and dirty hack to enable FcCompareFamily/FcCompareString
     * speedup: only allow strings to be added under the FC_FAMILY,
     * FC_FOUNDRY, FC_STYLE, FC_RASTERIZER keys.  
     * and charsets under FC_CHARSET key.
     * This is slightly semantically different from the old behaviour,
     * but fonts shouldn't be getting non-strings here anyway.
     * a better hack would use FcBaseObjectTypes to check all objects. */
    objectPtr = FcObjectToPtr(object);
    if ((objectPtr == FcObjectToPtr(FC_FAMILY)
         || objectPtr == FcObjectToPtr(FC_FOUNDRY)
         || objectPtr == FcObjectToPtr(FC_STYLE)
         || objectPtr == FcObjectToPtr(FC_RASTERIZER))
        && value.type != FcTypeString)
        goto bail1;
    if (objectPtr == FcObjectToPtr(FC_CHARSET)
        && value.type != FcTypeCharSet)
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
	FcStrFree ((FcChar8 *) value.u.s);
	break;
    case FcTypeMatrix:
	FcMatrixFree ((FcMatrix *) value.u.m);
	break;
    case FcTypeCharSet:
	FcCharSetDestroy ((FcCharSet *) value.u.c);
	break;
    case FcTypeLangSet:
	FcLangSetDestroy ((FcLangSet *) value.u.l);
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
    (FcPatternEltU(p->elts)+p->num)->object = 0;
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
    v.u.s = FcStrStaticName(s);
    return FcPatternAdd (p, object, v, FcTrue);
}

FcBool
FcPatternAddMatrix (FcPattern *p, const char *object, const FcMatrix *s)
{
    FcValue	v;

    v.type = FcTypeMatrix;
    v.u.m = s;
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
    v.u.c = (FcCharSet *)c;
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
    v.u.l = (FcLangSet *)ls;
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
	    *v = FcValueCanonicalize(&FcValueListPtrU(l)->value);
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

    if (FcObjectToPtr(object) == FcObjectToPtr(FC_FILE))
    {
	const char *fn, *fpath;
	FcChar8 *fname;
	int size;

	fn = FcPatternFindFullFname(p);
	if (fn)
	{
	    *s = (FcChar8 *) fn;
	    return FcResultMatch;
	}

	if (!p->bank)
	{
	    *s = (FcChar8 *) v.u.s;
	    return FcResultMatch;
	}

	fpath = FcCacheFindBankDir (p->bank);
	size = strlen((char*)fpath) + 1 + strlen ((char *)v.u.s) + 1;
	fname = malloc (size);
	if (!fname)
	    return FcResultOutOfMemory;

	FcMemAlloc (FC_MEM_STRING, size);
	strcpy ((char *)fname, (char *)fpath);
	strcat ((char *)fname, "/");
	strcat ((char *)fname, (char *)v.u.s);
	
	FcPatternAddFullFname (p, (const char *)fname);
	*s = (FcChar8 *)fname;
	return FcResultMatch;
    }

    *s = (FcChar8 *) v.u.s;
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
    *m = (FcMatrix *)v.u.m;
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
    *c = (FcCharSet *)v.u.c;
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
    *ls = (FcLangSet *)v.u.l;
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
                               FcValueCanonicalize(&FcValueListPtrU(l)->value),
			       FcTrue))
		goto bail1;
    }
    FcPatternTransferFullFname (new, orig);

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
					  FcValueCanonicalize(&FcValueListPtrU(v)->value), 
					  FcValueListPtrU(v)->binding, FcTrue))
		return FcFalse;
	}
    }
    return FcTrue;
}

#define OBJECT_HASH_SIZE    31
static struct objectBucket {
    struct objectBucket	*next;
    FcChar32		hash;
} *FcObjectBuckets[OBJECT_HASH_SIZE];

const FcChar8 *
FcStrStaticName (const FcChar8 *name)
{
    FcChar32		hash = FcStringHash (name);
    struct objectBucket	**p;
    struct objectBucket	*b;
    int			size;

    for (p = &FcObjectBuckets[hash % OBJECT_HASH_SIZE]; (b = *p); p = &(b->next))
	if (b->hash == hash && !strcmp ((char *)name, (char *) (b + 1)))
	    return (FcChar8 *) (b + 1);
    size = sizeof (struct objectBucket) + strlen ((char *)name) + 1;
    b = malloc (size + sizeof (int));
    /* workaround glibc bug which reads strlen in groups of 4 */
    FcMemAlloc (FC_MEM_STATICSTR, size + sizeof (int));
    if (!b)
        return NULL;
    b->next = 0;
    b->hash = hash;
    strcpy ((char *) (b + 1), (char *)name);
    *p = b;
    return (FcChar8 *) (b + 1);
}

static void
FcStrStaticNameFini (void)
{
    int i, size;
    struct objectBucket *b, *next;
    char *name;

    for (i = 0; i < OBJECT_HASH_SIZE; i++)
    {
	for (b = FcObjectBuckets[i]; b; b = next)
	{
	    next = b->next;
	    name = (char *) (b + 1);
	    size = sizeof (struct objectBucket) + strlen (name) + 1;
	    FcMemFree (FC_MEM_STATICSTR, size);
	    free (b);
	}
	FcObjectBuckets[i] = 0;
    }
}

void
FcPatternFini (void)
{
    FcPatternBaseThawAll ();
    FcValueListThawAll ();
    FcStrStaticNameFini ();
    FcObjectStaticNameFini ();
}

FcPatternElt *
FcPatternEltU (FcPatternEltPtr pei)
{
    if (pei.bank == FC_BANK_DYNAMIC)
	return pei.u.dyn;

    return &fcpatternelts[FcCacheBankToIndex(pei.bank)][pei.u.stat];
}

static FcPatternEltPtr
FcPatternEltPtrCreateDynamic (FcPatternElt * e)
{
    FcPatternEltPtr new;
    new.bank = FC_BANK_DYNAMIC;
    new.u.dyn = e;
    return new;
}

static FcPatternEltPtr
FcPatternEltPtrCreateStatic (int bank, int i)
{
    FcPatternEltPtr new;
    new.bank = bank;
    new.u.stat = i;
    return new;
}

static void
FcStrNewBank (void);
static int
FcStrNeededBytes (const FcChar8 * s);
static int
FcStrNeededBytesAlign (void);
static void *
FcStrDistributeBytes (FcCache * metadata, void * block_ptr);
static const FcChar8 *
FcStrSerialize (int bank, const FcChar8 * s);
static void *
FcStrUnserialize (FcCache metadata, void *block_ptr);

static void
FcValueListNewBank (void);
static int
FcValueListNeededBytes (FcValueList * vl);
static int
FcValueListNeededBytesAlign (void);
static void *
FcValueListDistributeBytes (FcCache * metadata, void *block_ptr);
static FcValueListPtr
FcValueListSerialize(int bank, FcValueList *pi);
static void *
FcValueListUnserialize (FcCache metadata, void *block_ptr);


void
FcPatternNewBank (void)
{
    fcpattern_count = 0;
    fcpatternelt_count = 0;

    FcStrNewBank();
    FcValueListNewBank();
}

int
FcPatternNeededBytes (FcPattern * p)
{
    int i, cum = 0, c;

    fcpattern_count++;
    fcpatternelt_count += p->num;

    for (i = 0; i < p->num; i++)
    {
	c = FcValueListNeededBytes (FcValueListPtrU
				    (((FcPatternEltU(p->elts)+i)->values)));
	if (c < 0)
	    return c;
	cum += c;
    }

    return cum + sizeof (FcPattern) + sizeof(FcPatternElt)*p->num;
}

int
FcPatternNeededBytesAlign (void)
{
    return __alignof__ (FcPattern) + __alignof__ (FcPatternElt) + 
	FcValueListNeededBytesAlign ();
}

static FcBool
FcPatternEnsureBank (int bi)
{
    FcPattern **pp;
    FcPatternElt **ep;
    int i;

    if (!fcpatterns || fcpattern_bank_count <= bi)
    {
	int new_count = bi + 4;
	pp = realloc (fcpatterns, sizeof (FcPattern *) * new_count);
	if (!pp)
	    return 0;

	FcMemAlloc (FC_MEM_PATTERN, sizeof (FcPattern *) * new_count);
	fcpatterns = pp;

	ep = realloc (fcpatternelts, sizeof (FcPatternElt *) * new_count);
	if (!ep)
	    return 0;

	FcMemAlloc (FC_MEM_PATELT, sizeof (FcPatternElt *) * new_count);
	fcpatternelts = ep;

	for (i = fcpattern_bank_count; i < new_count; i++)
	{
	    fcpatterns[i] = 0;
	    fcpatternelts[i] = 0;
	}

	fcpattern_bank_count = new_count;
    }

    FcMemAlloc (FC_MEM_PATTERN, sizeof (FcPattern) * fcpattern_count);
    return FcTrue;
}

void *
FcPatternDistributeBytes (FcCache * metadata, void * block_ptr)
{
    int bi = FcCacheBankToIndex(metadata->bank);

    if (!FcPatternEnsureBank(bi))
	return 0;

    fcpattern_ptr = 0;
    block_ptr = ALIGN(block_ptr, FcPattern);
    fcpatterns[bi] = (FcPattern *)block_ptr;
    block_ptr = (void *)((char *)block_ptr + 
			 (sizeof (FcPattern) * fcpattern_count));
    
    FcMemAlloc (FC_MEM_PATELT, sizeof (FcPatternElt) * fcpatternelt_count);
    fcpatternelt_ptr = 0;
    block_ptr = ALIGN(block_ptr, FcPatternElt);
    fcpatternelts[bi] = (FcPatternElt *)block_ptr;
    block_ptr = (void *)((char *)block_ptr + 
			 (sizeof (FcPatternElt) * fcpatternelt_count));

    metadata->pattern_count = fcpattern_count;
    metadata->patternelt_count = fcpatternelt_count;

    block_ptr = FcStrDistributeBytes (metadata, block_ptr);
    block_ptr = FcValueListDistributeBytes (metadata, block_ptr);
    return block_ptr;
}

FcPattern *
FcPatternSerialize (int bank, FcPattern *old)
{
    FcPattern *p;
    FcPatternElt *e, *nep;
    FcValueList * nv;
    FcValueListPtr v, nv_head, nvp;
    int i, elts, bi = FcCacheBankToIndex(bank);

    p = &fcpatterns[bi][fcpattern_ptr++];
    p->bank = bank;
    elts = fcpatternelt_ptr;
    nep = &fcpatternelts[bi][elts];
    if (!nep)
	return FcFalse;

    fcpatternelt_ptr += old->num;

    for (e = FcPatternEltU(old->elts), i=0; i < old->num; i++, e++) 
    {
        v = e->values;
        nvp = nv_head = FcValueListSerialize(bank, FcValueListPtrU(v));
        if (!FcValueListPtrU(nv_head))
            return 0;
	nv = FcValueListPtrU(nvp);
	
        for (;
             FcValueListPtrU(v);
             v = FcValueListPtrU(v)->next, 
		 nv = FcValueListPtrU(nv->next))
        {
	    
	    if (FcValueListPtrU(FcValueListPtrU(v)->next))
	    {
                nvp = FcValueListSerialize
		    (bank, FcValueListPtrU(FcValueListPtrU(v)->next));
                nv->next = nvp;
	    }
        }
	
	nep[i].values = nv_head;
	nep[i].object = e->object;
    }

    p->elts = old->elts;
    p->elts = FcPatternEltPtrCreateStatic(bank, elts);
    p->size = old->num;
    p->num = old->num;
    p->ref = FC_REF_CONSTANT;
    return p;
}

void *
FcPatternUnserialize (FcCache metadata, void *block_ptr)
{
    int bi = FcCacheBankToIndex(metadata.bank);
    if (!FcPatternEnsureBank(bi))
	return FcFalse;

    FcMemAlloc (FC_MEM_PATTERN, sizeof (FcPattern) * metadata.pattern_count);
    block_ptr = ALIGN(block_ptr, FcPattern);
    fcpatterns[bi] = (FcPattern *)block_ptr;
    block_ptr = (void *)((char *)block_ptr + 
			 (sizeof (FcPattern) * metadata.pattern_count));
    
    FcMemAlloc (FC_MEM_PATELT, 
		sizeof (FcPatternElt) * metadata.patternelt_count);
    block_ptr = ALIGN(block_ptr, FcPatternElt);
    fcpatternelts[bi] = (FcPatternElt *)block_ptr;
    block_ptr = (void *)((char *)block_ptr + 
			 (sizeof (FcPatternElt) * metadata.patternelt_count));
	
    block_ptr = FcStrUnserialize (metadata, block_ptr);
    block_ptr = FcValueListUnserialize (metadata, block_ptr);

    return block_ptr;
}

static void
FcValueListNewBank (void)
{
    fcvaluelist_count = 0;

    FcCharSetNewBank();
    FcLangSetNewBank();
}

static int
FcValueListNeededBytes (FcValueList *p)
{
    FcValueList *vl;
    int cum = 0;

    for (vl = p;
	 vl; 
	 vl = FcValueListPtrU(vl->next))
    {
	FcValue v = FcValueCanonicalize(&vl->value); // unserialize just in case

	switch (v.type)
	{
	case FcTypeCharSet:
	    cum += FcCharSetNeededBytes(v.u.c);
	    break;
	case FcTypeLangSet:
	    cum += FcLangSetNeededBytes(v.u.l);
	    break;
	case FcTypeString:
	    cum += FcStrNeededBytes(v.u.s);
	default:
	    break;
	}
	fcvaluelist_count++;
	cum += sizeof (FcValueList);
    }
    
    return cum;
}

static int
FcValueListNeededBytesAlign (void)
{
    return FcCharSetNeededBytesAlign() + FcLangSetNeededBytesAlign() + 
	FcStrNeededBytesAlign() + __alignof__ (FcValueList);
}

static FcBool
FcValueListEnsureBank (int bi)
{
    FcValueList **pvl;

    if (!fcvaluelists || fcvaluelist_bank_count <= bi)
    {
	int new_count = bi + 2, i;

	pvl = realloc (fcvaluelists, sizeof (FcValueList *) * new_count);
	if (!pvl)
	    return FcFalse;

	FcMemAlloc (FC_MEM_VALLIST, sizeof (FcValueList *) * new_count);

	fcvaluelists = pvl;
	for (i = fcvaluelist_bank_count; i < new_count; i++)
	    fcvaluelists[i] = 0;

	fcvaluelist_bank_count = new_count;
    }
    return FcTrue;
}

static void *
FcValueListDistributeBytes (FcCache * metadata, void *block_ptr)
{
    int bi = FcCacheBankToIndex(metadata->bank);

    if (!FcValueListEnsureBank(bi))
	return 0;

    FcMemAlloc (FC_MEM_VALLIST, sizeof (FcValueList) * fcvaluelist_count);
    fcvaluelist_ptr = 0;
    block_ptr = ALIGN(block_ptr, FcValueList);
    fcvaluelists[bi] = (FcValueList *)block_ptr;
    block_ptr = (void *)((char *)block_ptr + 
			 (sizeof (FcValueList) * fcvaluelist_count));
    metadata->valuelist_count = fcvaluelist_count;

    block_ptr = FcCharSetDistributeBytes(metadata, block_ptr);
    block_ptr = FcLangSetDistributeBytes(metadata, block_ptr);

    return block_ptr;
}

static FcValueListPtr
FcValueListSerialize(int bank, FcValueList *pi)
{
    FcValueListPtr new; 
    FcValue * v;
    int bi = FcCacheBankToIndex(bank);

    if (!pi)
    {
	new.bank = FC_BANK_DYNAMIC;
	new.u.dyn = 0;
	return new;
    }

    fcvaluelists[bi][fcvaluelist_ptr] = *pi;
    new.bank = bank;
    new.u.stat = fcvaluelist_ptr++;
    fcvaluelists[bi][new.u.stat].value = FcValueCanonicalize (&pi->value);
    v = &fcvaluelists[bi][new.u.stat].value;
    switch (v->type)
    {
    case FcTypeString:
	if (v->u.s)
	{
	    const FcChar8 * s = FcStrSerialize(bank, v->u.s);
	    if (!s)
		return FcValueListPtrCreateDynamic(pi);
	    v->u.s_off = s - (const FcChar8 *)v;
	    v->type |= FC_STORAGE_STATIC;
	}
	break;
    case FcTypeMatrix:
	break;
    case FcTypeCharSet:
	if (v->u.c)
	{
	    FcCharSet * c = FcCharSetSerialize(bank, (FcCharSet *)v->u.c);
	    if (!c)
		return FcValueListPtrCreateDynamic(pi);
	    v->u.c_off = (char *)c - (char *)v;
	    v->type |= FC_STORAGE_STATIC;
	}
	break;
    case FcTypeLangSet:
	if (v->u.l)
	{
	    FcLangSet * l = FcLangSetSerialize(bank, (FcLangSet *)v->u.l);
	    if (!l)
		return FcValueListPtrCreateDynamic(pi);
	    v->u.l_off = (char *)l - (char *)v;
	    v->type |= FC_STORAGE_STATIC;
	}
	break;
    default:
	break;
    }
    return new;
}

static void *
FcValueListUnserialize (FcCache metadata, void *block_ptr)
{
    int bi = FcCacheBankToIndex(metadata.bank);

    if (!FcValueListEnsureBank(bi))
	return 0;

    FcMemAlloc (FC_MEM_VALLIST, 
		sizeof (FcValueList) * metadata.valuelist_count);
    block_ptr = ALIGN(block_ptr, FcValueList);
    fcvaluelists[bi] = (FcValueList *)block_ptr;
    block_ptr = (void *)((char *)block_ptr + 
			 (sizeof (FcValueList) * metadata.valuelist_count));

    block_ptr = FcCharSetUnserialize(metadata, block_ptr);
    block_ptr = FcLangSetUnserialize(metadata, block_ptr);

    return block_ptr;
}

FcValueList * 
FcValueListPtrU (FcValueListPtr pi)
{
    if (pi.bank == FC_BANK_DYNAMIC)
        return pi.u.dyn;

    return &fcvaluelists[FcCacheBankToIndex(pi.bank)][pi.u.stat];
}

FcValueListPtr
FcValueListPtrCreateDynamic(FcValueList * p)
{
    FcValueListPtr r; 

    r.bank = FC_BANK_DYNAMIC; 
    r.u.dyn = p;
    return r;
}

static FcChar8 ** static_strs;
static int static_str_bank_count = 0, fcstr_ptr, fcstr_count;

static struct objectBucket *FcStrBuckets[OBJECT_HASH_SIZE];

static void
FcStrNewBank (void)
{
    int i, size;
    struct objectBucket *b, *next;
    char *name;

    for (i = 0; i < OBJECT_HASH_SIZE; i++)
    {
	for (b = FcStrBuckets[i]; b; b = next)
	{
	    next = b->next;
	    name = (char *) (b + 1);
	    size = sizeof (struct objectBucket) + strlen (name) + 1;
	    FcMemFree (FC_MEM_STATICSTR, size);
	    free (b);
	}
	FcStrBuckets[i] = 0;
    }

    fcstr_count = 0;
}

static int
FcStrNeededBytes (const FcChar8 * s)
{
    FcChar32            hash = FcStringHash ((const FcChar8 *) s);
    struct objectBucket **p;
    struct objectBucket *b;
    int                 size;

    for (p = &FcStrBuckets[hash % OBJECT_HASH_SIZE]; (b = *p); p = &(b->next))
        if (b->hash == hash && !strcmp ((char *)s, (char *) (b + 1)))
            return 0;
    size = sizeof (struct objectBucket) + strlen ((char *)s) + 1 + sizeof(char *);
    b = malloc (size);
    FcMemAlloc (FC_MEM_STATICSTR, size);
    if (!b)
        return -1;
    b->next = 0;
    b->hash = hash;
    strcpy ((char *) (b + 1), (char *)s);

    /* Yes, the following line is convoluted.  However, it is
     * incorrect to replace the with a memset, because the C
     * specification doesn't guarantee that the null pointer is
     * the same as the zero bit pattern. */
    *(char **)((char *) (b + 1) + strlen((char *)s) + 1) = 0;
    *p = b;

    fcstr_count += strlen((char *)s) + 1;
    return strlen((char *)s) + 1;
}

static int
FcStrNeededBytesAlign (void)
{
    return __alignof__ (char);
}

static FcBool
FcStrEnsureBank (int bi)
{
    FcChar8 ** ss;

    if (!static_strs || static_str_bank_count <= bi)
    {
	int new_count = bi + 4, i;
	ss = realloc (static_strs, sizeof (const char *) * new_count);
	if (!ss)
	    return FcFalse;

	FcMemAlloc (FC_MEM_STRING, sizeof (const char *) * (new_count-static_str_bank_count));
	static_strs = ss;

	for (i = static_str_bank_count; i < new_count; i++)
	    static_strs[i] = 0;
	static_str_bank_count = new_count;
    }
    return FcTrue;
}

static void *
FcStrDistributeBytes (FcCache * metadata, void * block_ptr)
{
    int bi = FcCacheBankToIndex(metadata->bank);
    if (!FcStrEnsureBank(bi)) 
	return 0;

    FcMemAlloc (FC_MEM_STRING, sizeof (char) * fcstr_count);
    block_ptr = ALIGN (block_ptr, FcChar8);
    static_strs[bi] = (FcChar8 *)block_ptr;
    block_ptr = (void *)((char *)block_ptr + (sizeof (char) * fcstr_count));
    metadata->str_count = fcstr_count;
    fcstr_ptr = 0;

    return block_ptr;
}

static const FcChar8 *
FcStrSerialize (int bank, const FcChar8 * s)
{
    FcChar32            hash = FcStringHash ((const FcChar8 *) s);
    struct objectBucket **p;
    struct objectBucket *b;
    int bi = FcCacheBankToIndex(bank);

    for (p = &FcStrBuckets[hash % OBJECT_HASH_SIZE]; (b = *p); p = &(b->next))
        if (b->hash == hash && !strcmp ((char *)s, (char *) (b + 1)))
	{
	    FcChar8 * t = *(FcChar8 **)(((FcChar8 *)(b + 1)) + strlen ((char *)s) + 1);
	    if (!t)
	    {
		strcpy((char *)(static_strs[bi] + fcstr_ptr), (char *)s);
		*(FcChar8 **)((FcChar8 *) (b + 1) + strlen((char *)s) + 1) = (static_strs[bi] + fcstr_ptr);
		fcstr_ptr += strlen((char *)s) + 1;
		t = *(FcChar8 **)(((FcChar8 *)(b + 1)) + strlen ((char *)s) + 1);
	    }
	    return t;
	}
    return 0;
}

static void *
FcStrUnserialize (FcCache metadata, void *block_ptr)
{
    int bi = FcCacheBankToIndex(metadata.bank);
    if (!FcStrEnsureBank(bi))
	return 0;

    FcMemAlloc (FC_MEM_STRING, sizeof (char) * metadata.str_count);
    block_ptr = ALIGN (block_ptr, FcChar8);
    static_strs[bi] = (FcChar8 *)block_ptr;
    block_ptr = (void *)((char *)block_ptr + 
			 (sizeof (char) * metadata.str_count));

    return block_ptr;
}

/* we don't store these in the FcPattern itself because
 * we don't want to serialize the directory names */

/* I suppose this should be cleaned, too... */
typedef struct _FcPatternDirMapping {
    const FcPattern	*p;
    const char *fname;
} FcPatternDirMapping;

#define PATTERNDIR_HASH_SIZE    31
static struct patternDirBucket {
    struct patternDirBucket	*next;
    FcPatternDirMapping		m;
} FcPatternDirBuckets[PATTERNDIR_HASH_SIZE];

void
FcPatternAddFullFname (const FcPattern *p, const char *fname)
{
    struct patternDirBucket	*pb;

    /* N.B. FcPatternHash fails, since it's contents-based, not
     * address-based, and we're in the process of mutating the FcPattern. */
    for (pb = &FcPatternDirBuckets
             [((int)p / sizeof (FcPattern *)) % PATTERNDIR_HASH_SIZE];
         pb->m.p != p && pb->next; 
         pb = pb->next)
        ;

    if (pb->m.p == p)
    {
        pb->m.fname = fname;
        return;
    }

    pb->next = malloc (sizeof (struct patternDirBucket));
    if (!pb->next)
        return;
    FcMemAlloc (FC_MEM_CACHE, sizeof (struct patternDirBucket));

    pb->next->next = 0;
    pb->next->m.p = p;
    pb->next->m.fname = fname;
}

static const char *
FcPatternFindFullFname (const FcPattern *p)
{
    struct patternDirBucket	*pb;

    for (pb = &FcPatternDirBuckets
             [((int)p / sizeof (FcPattern *)) % PATTERNDIR_HASH_SIZE]; 
         pb; pb = pb->next)
	if (pb->m.p == p)
	    return pb->m.fname;

    return 0;
}

void
FcPatternTransferFullFname (const FcPattern *new, const FcPattern *orig)
{
    FcChar8 * s;
    FcPatternGetString (orig, FC_FILE, 0, &s);
    FcPatternAddFullFname (new, 
			   (char *)FcStrCopy 
			   ((FcChar8 *)FcPatternFindFullFname(orig)));
}
