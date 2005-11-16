/*
 * $RCSId: xc/lib/fontconfig/src/fcname.c,v 1.15 2002/09/26 00:17:28 keithp Exp $
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fcint.h"

/* Please do not revoke any of these bindings. */
static const FcObjectType _FcBaseObjectTypes[] = {
    { "__DUMMY__",      FcTypeVoid, },
    { FC_FAMILY,	FcTypeString, },
    { FC_FAMILYLANG,	FcTypeString, },
    { FC_STYLE,		FcTypeString, },
    { FC_STYLELANG,	FcTypeString, },
    { FC_FULLNAME,	FcTypeString, },
    { FC_FULLNAMELANG,	FcTypeString, },
    { FC_SLANT,		FcTypeInteger, },
    { FC_WEIGHT,	FcTypeInteger, },
    { FC_WIDTH,		FcTypeInteger, },
    { FC_SIZE,		FcTypeDouble, },
    { FC_ASPECT,	FcTypeDouble, },
    { FC_PIXEL_SIZE,	FcTypeDouble, },
    { FC_SPACING,	FcTypeInteger, },
    { FC_FOUNDRY,	FcTypeString, },
/*    { FC_CORE,		FcTypeBool, }, */
    { FC_ANTIALIAS,	FcTypeBool, },
    { FC_HINT_STYLE,    FcTypeInteger, },
    { FC_HINTING,	FcTypeBool, },
    { FC_VERTICAL_LAYOUT,   FcTypeBool, },
    { FC_AUTOHINT,	FcTypeBool, },
    { FC_GLOBAL_ADVANCE,    FcTypeBool, },
/*    { FC_XLFD,		FcTypeString, }, */
    { FC_FILE,		FcTypeString, },
    { FC_INDEX,		FcTypeInteger, },
    { FC_RASTERIZER,	FcTypeString, },
    { FC_OUTLINE,	FcTypeBool, },
    { FC_SCALABLE,	FcTypeBool, },
    { FC_DPI,		FcTypeDouble },
    { FC_RGBA,		FcTypeInteger, },
    { FC_SCALE,		FcTypeDouble, },
    { FC_MINSPACE,	FcTypeBool, },
/*    { FC_RENDER,	FcTypeBool, },*/
    { FC_CHAR_WIDTH,	FcTypeInteger },
    { FC_CHAR_HEIGHT,	FcTypeInteger },
    { FC_MATRIX,	FcTypeMatrix },
    { FC_CHARSET,	FcTypeCharSet },
    { FC_LANG,		FcTypeLangSet },
    { FC_FONTVERSION,	FcTypeInteger },
    { FC_CAPABILITY,	FcTypeString },
    { FC_FONTFORMAT,	FcTypeString },
    { FC_EMBOLDEN,	FcTypeBool },
    { FC_EMBEDDED_BITMAP,   FcTypeBool },
};

#define NUM_OBJECT_TYPES    (sizeof _FcBaseObjectTypes / sizeof _FcBaseObjectTypes[0])

static FcObjectType * _FcUserObjectNames = 0;
static int user_obj_alloc = 0;
static int next_basic_offset = NUM_OBJECT_TYPES;

typedef struct _FcObjectTypeList    FcObjectTypeList;

struct _FcObjectTypeList {
    const FcObjectTypeList  *next;
    const FcObjectType	    *types;
    int			    ntypes;
    int			    basic_offset;
};

static const FcObjectTypeList _FcBaseObjectTypesList = {
    0,
    _FcBaseObjectTypes,
    NUM_OBJECT_TYPES
};

static const FcObjectTypeList	*_FcObjectTypes = &_FcBaseObjectTypesList;

FcBool
FcNameRegisterObjectTypes (const FcObjectType *types, int ntypes)
{
    FcObjectTypeList	*l;

    l = (FcObjectTypeList *) malloc (sizeof (FcObjectTypeList));
    if (!l)
	return FcFalse;
    FcMemAlloc (FC_MEM_OBJECTTYPE, sizeof (FcObjectTypeList));
    l->types = types;
    l->ntypes = ntypes;
    l->next = _FcObjectTypes;
    l->basic_offset = next_basic_offset;
    next_basic_offset += ntypes;
    _FcObjectTypes = l;
    return FcTrue;
}

static FcBool
FcNameUnregisterObjectTypesFree (const FcObjectType *types, int ntypes, 
				 FcBool do_free)
{
    const FcObjectTypeList	*l, **prev;

    for (prev = &_FcObjectTypes; 
	 (l = *prev); 
	 prev = (const FcObjectTypeList **) &(l->next))
    {
	if (l->types == types && l->ntypes == ntypes)
	{
	    *prev = l->next;
	    if (do_free) {
		FcMemFree (FC_MEM_OBJECTTYPE, sizeof (FcObjectTypeList));
		free ((void *) l);
	    }
	    return FcTrue;
	}
    }
    return FcFalse;
}

FcBool
FcNameUnregisterObjectTypes (const FcObjectType *types, int ntypes)
{
    return FcNameUnregisterObjectTypesFree (types, ntypes, FcTrue);
}

const FcObjectType *
FcNameGetObjectType (const char *object)
{
    int			    i;
    const FcObjectTypeList  *l;
    const FcObjectType	    *t;
    
    for (l = _FcObjectTypes; l; l = l->next)
    {
	for (i = 0; i < l->ntypes; i++)
	{
	    t = &l->types[i];
	    if (!strcmp (object, t->object))
		return t;
	}
    }
    return 0;
}

#define OBJECT_HASH_SIZE    31
struct objectBucket {
    struct objectBucket	*next;
    FcChar32		hash;
    int			id;
};
static struct objectBucket *FcObjectBuckets[OBJECT_HASH_SIZE];

/* Design constraint: biggest_known_ntypes must never change
 * after any call to FcNameRegisterObjectTypes. */
static const FcObjectType *biggest_known_types = _FcBaseObjectTypes; 
static FcBool allocated_biggest_known_types;
static int biggest_known_ntypes = NUM_OBJECT_TYPES;
static int biggest_known_count = 0;
static char * biggest_ptr;


static FcObjectPtr
FcObjectToPtrLookup (const char * object)
{
    FcObjectPtr		    i = 0, n;
    const FcObjectTypeList  *l;
    FcObjectType	    *t = _FcUserObjectNames;

    for (l = _FcObjectTypes; l; l = l->next)
    {
	for (i = 0; i < l->ntypes; i++)
	{
	    t = (FcObjectType *)&l->types[i];
	    if (!strcmp (object, t->object))
	    {
		if (l == (FcObjectTypeList*)_FcUserObjectNames)
                    return -i;
		else
		    return l->basic_offset + i;
	    }
	}
    }

    /* We didn't match.  Look for the correct FcObjectTypeList
     * to replace it in-place. */
    for (l = _FcObjectTypes; l; l = l->next)
    {
	if (l->types == _FcUserObjectNames)
	    break;
    }

    if (!_FcUserObjectNames || 
        (l && l->types == _FcUserObjectNames && user_obj_alloc < l->ntypes))
    {
	int nt = user_obj_alloc + 4;
        FcObjectType * t = realloc (_FcUserObjectNames, 
				    nt * sizeof (FcObjectType));
        if (!t)
            return 0;
	_FcUserObjectNames = t;
	user_obj_alloc = nt;
    }

    if (l && l->types == _FcUserObjectNames)
    {
	n = l->ntypes;
	FcNameUnregisterObjectTypesFree (l->types, l->ntypes, FcFalse);
    }
    else
	n = 0;

    FcNameRegisterObjectTypes (_FcUserObjectNames, n+1);

    for (l = _FcObjectTypes; l; l = l->next)
    {
	if (l->types == _FcUserObjectNames)
	{
	    t = (FcObjectType *)l->types;
	    break;
	}
    }

    if (!t)
	return 0;

    t[n].object = object;
    t[n].type = FcTypeVoid;

    return -n;
}

FcObjectPtr
FcObjectToPtr (const char * name)
{
    FcChar32            hash = FcStringHash ((const FcChar8 *) name);
    struct objectBucket **p;
    struct objectBucket *b;
    int                 size;

    for (p = &FcObjectBuckets[hash % OBJECT_HASH_SIZE]; (b = *p); p = &(b->next)
)
        if (b->hash == hash && !strcmp (name, (char *) (b + 1)))
            return b->id;
    size = sizeof (struct objectBucket) + strlen (name) + 1;
    /* workaround glibc bug which reads strlen in groups of 4 */
    b = malloc (size + sizeof (int));
    FcMemAlloc (FC_MEM_STATICSTR, size + sizeof(int));
    if (!b)
        return 0;
    b->next = 0;
    b->hash = hash;
    b->id = FcObjectToPtrLookup (name);
    strcpy ((char *) (b + 1), name);
    *p = b;
    return b->id;
}

void
FcObjectStaticNameFini (void)
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

const char *
FcObjectPtrU (FcObjectPtr si)
{
    const FcObjectTypeList  *l;
    int i, j;

    if (si > 0)
    {
        if (si < biggest_known_ntypes)
            return biggest_known_types[si].object;

        j = 0;
        for (l = _FcObjectTypes; l; l = l->next)
            for (i = 0; i < l->ntypes; i++, j++)
                if (j == si)
                    return l->types[i].object;
    }

    return _FcUserObjectNames[-si].object;
}

int
FcObjectNeededBytes ()
{
    int num = 0, i;
    for (i = 0; i < biggest_known_ntypes; i++)
    {
	const char * t = biggest_known_types[i].object;
	num = num + strlen(t) + 1;
    }
    biggest_known_count = num;
    return num + sizeof(int);
}

int
FcObjectNeededBytesAlign (void)
{
    return __alignof__ (int) + __alignof__ (char);
}

void *
FcObjectDistributeBytes (FcCache * metadata, void * block_ptr)
{
    *(int *)block_ptr = biggest_known_ntypes;
    block_ptr = ALIGN (block_ptr, int);
    block_ptr = (int *) block_ptr + 1;
    biggest_ptr = block_ptr;
    block_ptr = ALIGN (block_ptr, char);
    block_ptr = (char *) block_ptr + biggest_known_count;
    return block_ptr;
}

void
FcObjectSerialize ()
{
    int i;
    for (i = 0; i < biggest_known_ntypes; i++)
    {
	const char * t = biggest_known_types[i].object;
	strcpy (biggest_ptr, t);
	biggest_ptr = biggest_ptr + strlen(t) + 1;
    }
}

void *
FcObjectUnserialize (FcCache metadata, void *block_ptr)
{
    int new_biggest;
    new_biggest = *(int *)block_ptr;
    block_ptr = (int *) block_ptr + 1;
    if (biggest_known_ntypes < new_biggest)
    {
	int i;
	char * bp = (char *)block_ptr;
	FcObjectType * bn;
	FcObjectTypeList * bnl;

	bn = malloc (sizeof (const FcObjectType) * (new_biggest + 1));
	if (!bn)
	    return 0;

	bnl = malloc (sizeof (FcObjectTypeList));
	if (!bnl)
	{
	    free (bn);
	    return 0;
	}

	for (i = 0; i < new_biggest; i++)
	{
	    const FcObjectType * t = FcNameGetObjectType(bp);
	    if (t)
		bn[i].type = t->type;
	    else
		bn[i].type = FcTypeVoid;
	    bn[i].object = bp;
	    bp = bp + strlen(bp) + 1;
	}

	FcNameUnregisterObjectTypesFree (biggest_known_types, biggest_known_ntypes, FcFalse);
	if (allocated_biggest_known_types)
	{
	    free ((FcObjectTypeList *)biggest_known_types);
	}
	else
	    allocated_biggest_known_types = FcTrue;

	FcNameRegisterObjectTypes (bn, new_biggest);
	biggest_known_ntypes = new_biggest;
	biggest_known_types = (const FcObjectType *)bn;
    }
    block_ptr = (char *) block_ptr + biggest_known_count;
    return block_ptr;
}

int
FcObjectPtrCompare (const FcObjectPtr a, const FcObjectPtr b)
{
    return a - b;
}

static const FcConstant _FcBaseConstants[] = {
    { (FcChar8 *) "thin",	    "weight",   FC_WEIGHT_THIN, },
    { (FcChar8 *) "extralight",	    "weight",   FC_WEIGHT_EXTRALIGHT, },
    { (FcChar8 *) "ultralight",	    "weight",   FC_WEIGHT_EXTRALIGHT, },
    { (FcChar8 *) "light",	    "weight",   FC_WEIGHT_LIGHT, },
    { (FcChar8 *) "book",	    "weight",	FC_WEIGHT_BOOK, },
    { (FcChar8 *) "regular",	    "weight",   FC_WEIGHT_REGULAR, },
    { (FcChar8 *) "medium",	    "weight",   FC_WEIGHT_MEDIUM, },
    { (FcChar8 *) "demibold",	    "weight",   FC_WEIGHT_DEMIBOLD, },
    { (FcChar8 *) "semibold",	    "weight",   FC_WEIGHT_DEMIBOLD, },
    { (FcChar8 *) "bold",	    "weight",   FC_WEIGHT_BOLD, },
    { (FcChar8 *) "extrabold",	    "weight",   FC_WEIGHT_EXTRABOLD, },
    { (FcChar8 *) "ultrabold",	    "weight",   FC_WEIGHT_EXTRABOLD, },
    { (FcChar8 *) "black",	    "weight",   FC_WEIGHT_BLACK, },

    { (FcChar8 *) "roman",	    "slant",    FC_SLANT_ROMAN, },
    { (FcChar8 *) "italic",	    "slant",    FC_SLANT_ITALIC, },
    { (FcChar8 *) "oblique",	    "slant",    FC_SLANT_OBLIQUE, },

    { (FcChar8 *) "ultracondensed", "width",	FC_WIDTH_ULTRACONDENSED },
    { (FcChar8 *) "extracondensed", "width",	FC_WIDTH_EXTRACONDENSED },
    { (FcChar8 *) "condensed",	    "width",	FC_WIDTH_CONDENSED },
    { (FcChar8 *) "semicondensed", "width",	FC_WIDTH_SEMICONDENSED },
    { (FcChar8 *) "normal",	    "width",	FC_WIDTH_NORMAL },
    { (FcChar8 *) "semiexpanded",   "width",	FC_WIDTH_SEMIEXPANDED },
    { (FcChar8 *) "expanded",	    "width",	FC_WIDTH_EXPANDED },
    { (FcChar8 *) "extraexpanded",  "width",	FC_WIDTH_EXTRAEXPANDED },
    { (FcChar8 *) "ultraexpanded",  "width",	FC_WIDTH_ULTRAEXPANDED },
    
    { (FcChar8 *) "proportional",   "spacing",  FC_PROPORTIONAL, },
    { (FcChar8 *) "dual",	    "spacing",  FC_DUAL, },
    { (FcChar8 *) "mono",	    "spacing",  FC_MONO, },
    { (FcChar8 *) "charcell",	    "spacing",  FC_CHARCELL, },

    { (FcChar8 *) "unknown",	    "rgba",	    FC_RGBA_UNKNOWN },
    { (FcChar8 *) "rgb",	    "rgba",	    FC_RGBA_RGB, },
    { (FcChar8 *) "bgr",	    "rgba",	    FC_RGBA_BGR, },
    { (FcChar8 *) "vrgb",	    "rgba",	    FC_RGBA_VRGB },
    { (FcChar8 *) "vbgr",	    "rgba",	    FC_RGBA_VBGR },
    { (FcChar8 *) "none",	    "rgba",	    FC_RGBA_NONE },

    { (FcChar8 *) "hintnone",	    "hintstyle",   FC_HINT_NONE },
    { (FcChar8 *) "hintslight",	    "hintstyle",   FC_HINT_SLIGHT },
    { (FcChar8 *) "hintmedium",	    "hintstyle",   FC_HINT_MEDIUM },
    { (FcChar8 *) "hintfull",	    "hintstyle",   FC_HINT_FULL },
};

#define NUM_FC_CONSTANTS   (sizeof _FcBaseConstants/sizeof _FcBaseConstants[0])

typedef struct _FcConstantList FcConstantList;

struct _FcConstantList {
    const FcConstantList    *next;
    const FcConstant	    *consts;
    int			    nconsts;
};

static const FcConstantList _FcBaseConstantList = {
    0,
    _FcBaseConstants,
    NUM_FC_CONSTANTS
};

static const FcConstantList	*_FcConstants = &_FcBaseConstantList;

FcBool
FcNameRegisterConstants (const FcConstant *consts, int nconsts)
{
    FcConstantList	*l;

    l = (FcConstantList *) malloc (sizeof (FcConstantList));
    if (!l)
	return FcFalse;
    FcMemAlloc (FC_MEM_CONSTANT, sizeof (FcConstantList));
    l->consts = consts;
    l->nconsts = nconsts;
    l->next = _FcConstants;
    _FcConstants = l;
    return FcTrue;
}

FcBool
FcNameUnregisterConstants (const FcConstant *consts, int nconsts)
{
    const FcConstantList	*l, **prev;

    for (prev = &_FcConstants; 
	 (l = *prev); 
	 prev = (const FcConstantList **) &(l->next))
    {
	if (l->consts == consts && l->nconsts == nconsts)
	{
	    *prev = l->next;
	    FcMemFree (FC_MEM_CONSTANT, sizeof (FcConstantList));
	    free ((void *) l);
	    return FcTrue;
	}
    }
    return FcFalse;
}

const FcConstant *
FcNameGetConstant (FcChar8 *string)
{
    const FcConstantList    *l;
    int			    i;

    for (l = _FcConstants; l; l = l->next)
    {
	for (i = 0; i < l->nconsts; i++)
	    if (!FcStrCmpIgnoreCase (string, l->consts[i].name))
		return &l->consts[i];
    }
    return 0;
}

FcBool
FcNameConstant (FcChar8 *string, int *result)
{
    const FcConstant	*c;

    if ((c = FcNameGetConstant(string)))
    {
	*result = c->value;
	return FcTrue;
    }
    return FcFalse;
}

FcBool
FcNameBool (const FcChar8 *v, FcBool *result)
{
    char    c0, c1;

    c0 = *v;
    c0 = FcToLower (c0);
    if (c0 == 't' || c0 == 'y' || c0 == '1')
    {
	*result = FcTrue;
	return FcTrue;
    }
    if (c0 == 'f' || c0 == 'n' || c0 == '0')
    {
	*result = FcFalse;
	return FcTrue;
    }
    if (c0 == 'o')
    {
	c1 = v[1];
	c1 = FcToLower (c1);
	if (c1 == 'n')
	{
	    *result = FcTrue;
	    return FcTrue;
	}
	if (c1 == 'f')
	{
	    *result = FcFalse;
	    return FcTrue;
	}
    }
    return FcFalse;
}

static FcValue
FcNameConvert (FcType type, FcChar8 *string, FcMatrix *m)
{
    FcValue	v;

    v.type = type;
    switch (v.type) {
    case FcTypeInteger:
	if (!FcNameConstant (string, &v.u.i))
	    v.u.i = atoi ((char *) string);
	break;
    case FcTypeString:
	v.u.s = FcStrStaticName(string);
	break;
    case FcTypeBool:
	if (!FcNameBool (string, &v.u.b))
	    v.u.b = FcFalse;
	break;
    case FcTypeDouble:
	v.u.d = strtod ((char *) string, 0);
	break;
    case FcTypeMatrix:
	v.u.m = m;
	sscanf ((char *) string, "%lg %lg %lg %lg", &m->xx, &m->xy, &m->yx, &m->yy);
	break;
    case FcTypeCharSet:
	v.u.c = FcNameParseCharSet (string);
	break;
    case FcTypeLangSet:
	v.u.l = FcNameParseLangSet (string);
	break;
    default:
	break;
    }
    return v;
}

static const FcChar8 *
FcNameFindNext (const FcChar8 *cur, const char *delim, FcChar8 *save, FcChar8 *last)
{
    FcChar8    c;
    
    while ((c = *cur))
    {
	if (c == '\\')
	{
	    ++cur;
	    if (!(c = *cur))
		break;
	}
	else if (strchr (delim, c))
	    break;
	++cur;
	*save++ = c;
    }
    *save = 0;
    *last = *cur;
    if (*cur)
	cur++;
    return cur;
}

FcPattern *
FcNameParse (const FcChar8 *name)
{
    FcChar8		*save;
    FcPattern		*pat;
    double		d;
    FcChar8		*e;
    FcChar8		delim;
    FcValue		v;
    FcMatrix		m;
    const FcObjectType	*t;
    const FcConstant	*c;

    /* freed below */
    save = malloc (strlen ((char *) name) + 1);
    if (!save)
	goto bail0;
    pat = FcPatternCreate ();
    if (!pat)
	goto bail1;

    for (;;)
    {
	name = FcNameFindNext (name, "-,:", save, &delim);
	if (save[0])
	{
	    if (!FcPatternAddString (pat, FC_FAMILY, save))
		goto bail2;
	}
	if (delim != ',')
	    break;
    }
    if (delim == '-')
    {
	for (;;)
	{
	    name = FcNameFindNext (name, "-,:", save, &delim);
	    d = strtod ((char *) save, (char **) &e);
	    if (e != save)
	    {
		if (!FcPatternAddDouble (pat, FC_SIZE, d))
		    goto bail2;
	    }
	    if (delim != ',')
		break;
	}
    }
    while (delim == ':')
    {
	name = FcNameFindNext (name, "=_:", save, &delim);
	if (save[0])
	{
	    if (delim == '=' || delim == '_')
	    {
		t = FcNameGetObjectType ((char *) save);
		for (;;)
		{
		    name = FcNameFindNext (name, ":,", save, &delim);
		    if (t)
		    {
			v = FcNameConvert (t->type, save, &m);
			if (!FcPatternAdd (pat, t->object, v, FcTrue))
			{
			    switch (v.type) {
			    case FcTypeCharSet:
				FcCharSetDestroy ((FcCharSet *) v.u.c);
				break;
			    case FcTypeLangSet:
				FcLangSetDestroy ((FcLangSet *) v.u.l);
				break;
			    default:
				break;
			    }
			    goto bail2;
			}
			switch (v.type) {
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
		    if (delim != ',')
			break;
		}
	    }
	    else
	    {
		if ((c = FcNameGetConstant (save)))
		{
		    if (!FcPatternAddInteger (pat, c->object, c->value))
			goto bail2;
		}
	    }
	}
    }

    free (save);
    return pat;

bail2:
    FcPatternDestroy (pat);
bail1:
    free (save);
bail0:
    return 0;
}
static FcBool
FcNameUnparseString (FcStrBuf	    *buf, 
		     const FcChar8  *string,
		     const FcChar8  *escape)
{
    FcChar8 c;
    while ((c = *string++))
    {
	if (escape && strchr ((char *) escape, (char) c))
	{
	    if (!FcStrBufChar (buf, escape[0]))
		return FcFalse;
	}
	if (!FcStrBufChar (buf, c))
	    return FcFalse;
    }
    return FcTrue;
}

static FcBool
FcNameUnparseValue (FcStrBuf	*buf,
		    int		bank,
		    FcValue	*v0,
		    FcChar8	*escape)
{
    FcChar8	temp[1024];
    FcValue v = FcValueCanonicalize(v0);
    
    switch (v.type) {
    case FcTypeVoid:
	return FcTrue;
    case FcTypeInteger:
	sprintf ((char *) temp, "%d", v.u.i);
	return FcNameUnparseString (buf, temp, 0);
    case FcTypeDouble:
	sprintf ((char *) temp, "%g", v.u.d);
	return FcNameUnparseString (buf, temp, 0);
    case FcTypeString:
	return FcNameUnparseString (buf, v.u.s, escape);
    case FcTypeBool:
	return FcNameUnparseString (buf, v.u.b ? (FcChar8 *) "True" : (FcChar8 *) "False", 0);
    case FcTypeMatrix:
	sprintf ((char *) temp, "%g %g %g %g", 
		 v.u.m->xx, v.u.m->xy, v.u.m->yx, v.u.m->yy);
	return FcNameUnparseString (buf, temp, 0);
    case FcTypeCharSet:
	return FcNameUnparseCharSet (buf, v.u.c);
    case FcTypeLangSet:
	return FcNameUnparseLangSet (buf, v.u.l);
    case FcTypeFTFace:
	return FcTrue;
    }
    return FcFalse;
}

static FcBool
FcNameUnparseValueList (FcStrBuf	*buf,
			FcValueListPtr	v,
			FcChar8		*escape)
{
    while (FcValueListPtrU(v))
    {
	if (!FcNameUnparseValue (buf, v.bank, &FcValueListPtrU(v)->value, escape))
	    return FcFalse;
	if (FcValueListPtrU(v = FcValueListPtrU(v)->next))
	    if (!FcNameUnparseString (buf, (FcChar8 *) ",", 0))
		return FcFalse;
    }
    return FcTrue;
}

#define FC_ESCAPE_FIXED    "\\-:,"
#define FC_ESCAPE_VARIABLE "\\=_:,"

FcChar8 *
FcNameUnparse (FcPattern *pat)
{
    return FcNameUnparseEscaped (pat, FcTrue);
}

FcChar8 *
FcNameUnparseEscaped (FcPattern *pat, FcBool escape)
{
    FcStrBuf		    buf;
    FcChar8		    buf_static[8192];
    int			    i;
    FcPatternElt	    *e;
    const FcObjectTypeList  *l;
    const FcObjectType	    *o;

    FcStrBufInit (&buf, buf_static, sizeof (buf_static));
    e = FcPatternFindElt (pat, FC_FAMILY);
    if (e)
    {
        if (!FcNameUnparseValueList (&buf, e->values, escape ? (FcChar8 *) FC_ESCAPE_FIXED : 0))
	    goto bail0;
    }
    e = FcPatternFindElt (pat, FC_SIZE);
    if (e)
    {
	if (!FcNameUnparseString (&buf, (FcChar8 *) "-", 0))
	    goto bail0;
	if (!FcNameUnparseValueList (&buf, e->values, escape ? (FcChar8 *) FC_ESCAPE_FIXED : 0))
	    goto bail0;
    }
    for (l = _FcObjectTypes; l; l = l->next)
    {
	for (i = 0; i < l->ntypes; i++)
	{
	    o = &l->types[i];
	    if (!strcmp (o->object, FC_FAMILY) || 
		!strcmp (o->object, FC_SIZE) ||
		!strcmp (o->object, FC_FILE))
		continue;
	    
	    e = FcPatternFindElt (pat, o->object);
	    if (e)
	    {
		if (!FcNameUnparseString (&buf, (FcChar8 *) ":", 0))
		    goto bail0;
		if (!FcNameUnparseString (&buf, (FcChar8 *) o->object, escape ? (FcChar8 *) FC_ESCAPE_VARIABLE : 0))
		    goto bail0;
		if (!FcNameUnparseString (&buf, (FcChar8 *) "=", 0))
		    goto bail0;
		if (!FcNameUnparseValueList (&buf, e->values, escape ? 
					     (FcChar8 *) FC_ESCAPE_VARIABLE : 0))
		    goto bail0;
	    }
	}
    }
    return FcStrBufDone (&buf);
bail0:
    FcStrBufDestroy (&buf);
    return 0;
}
