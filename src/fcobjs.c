/*
 * fontconfig/src/fclist.c
 *
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "fcint.h"

#include "fcobjshash.h"

#include <string.h>

static int next_id = FC_MAX_BASE_OBJECT + 1;
struct FcObjectOtherTypeInfo {
    struct FcObjectOtherTypeInfo *next;
    FcObjectType object;
    int id;
} *other_types;

static FcObjectType *
_FcObjectLookupOtherTypeByName (const char *str, FcObject *id)
{
    struct FcObjectOtherTypeInfo *ots, *ot;

    /* XXX MT-unsafe */
    ots = other_types;

    for (ot = ots; ot; ot = ot->next)
	if (0 == strcmp (ot->object.object, str))
	    break;

    if (!ot)
    {
	ot = malloc (sizeof (*ot));
	if (!ot)
	    return NULL;

	ot->object.object = strdup (str);
	ot->object.type = -1;
	ot->id = next_id++; /* MT_unsafe */
	ot->next = ot;

	other_types = ot;
    }

    if (id)
      *id = ot->id;

    return &ot->object;
}

FcObject
FcObjectLookupBuiltinIdByName (const char *str)
{
    const struct FcObjectTypeInfo *o = FcObjectTypeLookup (str, strlen (str));
    FcObject id;
    if (o)
	return o->id;

    return 0;
}

FcObject
FcObjectLookupIdByName (const char *str)
{
    const struct FcObjectTypeInfo *o = FcObjectTypeLookup (str, strlen (str));
    FcObject id;
    if (o)
	return o->id;

    if (_FcObjectLookupOtherTypeByName (str, &id))
	return id;

    return 0;
}

const char *
FcObjectLookupOtherNameById (FcObject id)
{
    /* XXX MT-unsafe */
    struct FcObjectOtherTypeInfo *ot;

    for (ot = other_types; ot; ot = ot->next)
	if (ot->id == id)
	    return ot->object.object;

    return NULL;
}

const FcObjectType *
FcObjectLookupOtherTypeByName (const char *str)
{
    return _FcObjectLookupOtherTypeByName (str, NULL);
}

FcPrivate const FcObjectType *
FcObjectLookupOtherTypeById (FcObject id)
{
    /* XXX MT-unsafe */
    struct FcObjectOtherTypeInfo *ot;

    for (ot = other_types; ot; ot = ot->next)
	if (ot->id == id)
	    return &ot->object;

    return NULL;
}


#define __fcobjs__
#include "fcaliastail.h"
#undef __fcobjs__
