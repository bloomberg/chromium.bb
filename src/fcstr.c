/*
 * $RCSId: xc/lib/fontconfig/src/fcstr.c,v 1.10 2002/08/31 22:17:32 keithp Exp $
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
#include <ctype.h>
#include <string.h>
#include "fcint.h"

FcChar8 *
FcStrCopy (const FcChar8 *s)
{
    FcChar8	*r;

    if (!s)
	return 0;
    r = (FcChar8 *) malloc (strlen ((char *) s) + 1);
    if (!r)
	return 0;
    FcMemAlloc (FC_MEM_STRING, strlen ((char *) s) + 1);
    strcpy ((char *) r, (char *) s);
    return r;
}

FcChar8 *
FcStrPlus (const FcChar8 *s1, const FcChar8 *s2)
{
    int	    l = strlen ((char *)s1) + strlen ((char *) s2) + 1;
    FcChar8 *s = malloc (l);

    if (!s)
	return 0;
    FcMemAlloc (FC_MEM_STRING, l);
    strcpy ((char *) s, (char *) s1);
    strcat ((char *) s, (char *) s2);
    return s;
}

void
FcStrFree (FcChar8 *s)
{
    FcMemFree (FC_MEM_STRING, strlen ((char *) s) + 1);
    free (s);
}

int
FcStrCmpIgnoreCase (const FcChar8 *s1, const FcChar8 *s2)
{
    FcChar8 c1, c2;
    
    for (;;) 
    {
	c1 = *s1++;
	c2 = *s2++;
	if (!c1 || (c1 != c2 && (c1 = FcToLower(c1)) != (c2 = FcToLower(c2))))
	    break;
    }
    return (int) c1 - (int) c2;
}

int
FcStrCmpIgnoreBlanksAndCase (const FcChar8 *s1, const FcChar8 *s2)
{
    FcChar8 c1, c2;
    
    for (;;) 
    {
	do
	    c1 = *s1++;
	while (c1 == ' ');
	do
	    c2 = *s2++;
	while (c2 == ' ');
	if (!c1 || (c1 != c2 && (c1 = FcToLower(c1)) != (c2 = FcToLower(c2))))
	    break;
    }
    return (int) c1 - (int) c2;
}

int
FcStrCmp (const FcChar8 *s1, const FcChar8 *s2)
{
    FcChar8 c1, c2;
    
    if (s1 == s2)
	return 0;
    for (;;) 
    {
	c1 = *s1++;
	c2 = *s2++;
	if (!c1 || c1 != c2)
	    break;
    }
    return (int) c1 - (int) c2;
}

int
FcUtf8ToUcs4 (const FcChar8 *src_orig,
	      FcChar32	    *dst,
	      int	    len)
{
    const FcChar8   *src = src_orig;
    FcChar8	    s;
    int		    extra;
    FcChar32	    result;

    if (len == 0)
	return 0;
    
    s = *src++;
    len--;
    
    if (!(s & 0x80))
    {
	result = s;
	extra = 0;
    } 
    else if (!(s & 0x40))
    {
	return -1;
    }
    else if (!(s & 0x20))
    {
	result = s & 0x1f;
	extra = 1;
    }
    else if (!(s & 0x10))
    {
	result = s & 0xf;
	extra = 2;
    }
    else if (!(s & 0x08))
    {
	result = s & 0x07;
	extra = 3;
    }
    else if (!(s & 0x04))
    {
	result = s & 0x03;
	extra = 4;
    }
    else if ( ! (s & 0x02))
    {
	result = s & 0x01;
	extra = 5;
    }
    else
    {
	return -1;
    }
    if (extra > len)
	return -1;
    
    while (extra--)
    {
	result <<= 6;
	s = *src++;
	
	if ((s & 0xc0) != 0x80)
	    return -1;
	
	result |= s & 0x3f;
    }
    *dst = result;
    return src - src_orig;
}

FcBool
FcUtf8Len (const FcChar8    *string,
	   int		    len,
	   int		    *nchar,
	   int		    *wchar)
{
    int		n;
    int		clen;
    FcChar32	c;
    FcChar32	max;
    
    n = 0;
    max = 0;
    while (len)
    {
	clen = FcUtf8ToUcs4 (string, &c, len);
	if (clen <= 0)	/* malformed UTF8 string */
	    return FcFalse;
	if (c > max)
	    max = c;
	string += clen;
	len -= clen;
	n++;
    }
    *nchar = n;
    if (max >= 0x10000)
	*wchar = 4;
    else if (max > 0x100)
	*wchar = 2;
    else
	*wchar = 1;
    return FcTrue;
}

int
FcUcs4ToUtf8 (FcChar32	ucs4,
	      FcChar8	dest[FC_UTF8_MAX_LEN])
{
    int	bits;
    FcChar8 *d = dest;
    
    if      (ucs4 <       0x80) {  *d++=  ucs4;                         bits= -6; }
    else if (ucs4 <      0x800) {  *d++= ((ucs4 >>  6) & 0x1F) | 0xC0;  bits=  0; }
    else if (ucs4 <    0x10000) {  *d++= ((ucs4 >> 12) & 0x0F) | 0xE0;  bits=  6; }
    else if (ucs4 <   0x200000) {  *d++= ((ucs4 >> 18) & 0x07) | 0xF0;  bits= 12; }
    else if (ucs4 <  0x4000000) {  *d++= ((ucs4 >> 24) & 0x03) | 0xF8;  bits= 18; }
    else if (ucs4 < 0x80000000) {  *d++= ((ucs4 >> 30) & 0x01) | 0xFC;  bits= 24; }
    else return 0;

    for ( ; bits >= 0; bits-= 6) {
	*d++= ((ucs4 >> bits) & 0x3F) | 0x80;
    }
    return d - dest;
}

#define GetUtf16(src,endian) \
    ((FcChar16) ((src)[endian == FcEndianBig ? 0 : 1] << 8) | \
     (FcChar16) ((src)[endian == FcEndianBig ? 1 : 0]))

int
FcUtf16ToUcs4 (const FcChar8	*src_orig,
	       FcEndian		endian,
	       FcChar32		*dst,
	       int		len)	/* in bytes */
{
    const FcChar8   *src = src_orig;
    FcChar16	    a, b;
    FcChar32	    result;

    if (len < 2)
	return 0;
    
    a = GetUtf16 (src, endian); src += 2; len -= 2;
    
    /* 
     * Check for surrogate 
     */
    if ((a & 0xfc00) == 0xd800)
    {
	if (len < 2)
	    return 0;
	b = GetUtf16 (src, endian); src += 2; len -= 2;
	/*
	 * Check for invalid surrogate sequence
	 */
	if ((b & 0xfc00) != 0xdc00)
	    return 0;
	result = ((((FcChar32) a & 0x3ff) << 10) |
		  ((FcChar32) b & 0x3ff)) + 0x10000;
    }
    else
	result = a;
    *dst = result;
    return src - src_orig;
}

FcBool
FcUtf16Len (const FcChar8   *string,
	    FcEndian	    endian,
	    int		    len,	/* in bytes */
	    int		    *nchar,
	    int		    *wchar)
{
    int		n;
    int		clen;
    FcChar32	c;
    FcChar32	max;
    
    n = 0;
    max = 0;
    while (len)
    {
	clen = FcUtf16ToUcs4 (string, endian, &c, len);
	if (clen <= 0)	/* malformed UTF8 string */
	    return FcFalse;
	if (c > max)
	    max = c;
	string += clen;
	len -= clen;
	n++;
    }
    *nchar = n;
    if (max >= 0x10000)
	*wchar = 4;
    else if (max > 0x100)
	*wchar = 2;
    else
	*wchar = 1;
    return FcTrue;
}

void
FcStrBufInit (FcStrBuf *buf, FcChar8 *init, int size)
{
    buf->buf = init;
    buf->allocated = FcFalse;
    buf->failed = FcFalse;
    buf->len = 0;
    buf->size = size;
}

void
FcStrBufDestroy (FcStrBuf *buf)
{
    if (buf->allocated)
    {
	FcMemFree (FC_MEM_STRBUF, buf->size);
	free (buf->buf);
	FcStrBufInit (buf, 0, 0);
    }
}

FcChar8 *
FcStrBufDone (FcStrBuf *buf)
{
    FcChar8 *ret;

    ret = malloc (buf->len + 1);
    if (ret)
    {
	FcMemAlloc (FC_MEM_STRING, buf->len + 1);
	memcpy (ret, buf->buf, buf->len);
	ret[buf->len] = '\0';
    }
    FcStrBufDestroy (buf);
    return ret;
}

FcBool
FcStrBufChar (FcStrBuf *buf, FcChar8 c)
{
    if (buf->len == buf->size)
    {
	FcChar8	    *new;
	int	    size;

	if (buf->allocated)
	{
	    size = buf->size * 2;
	    new = realloc (buf->buf, size);
	}
	else
	{
	    size = buf->size + 1024;
	    new = malloc (size);
	    if (new)
	    {
		buf->allocated = FcTrue;
		memcpy (new, buf->buf, buf->len);
	    }
	}
	if (!new)
	{
	    buf->failed = FcTrue;
	    return FcFalse;
	}
	if (buf->size)
	    FcMemFree (FC_MEM_STRBUF, buf->size);
	FcMemAlloc (FC_MEM_STRBUF, size);
	buf->size = size;
	buf->buf = new;
    }
    buf->buf[buf->len++] = c;
    return FcTrue;
}

FcBool
FcStrBufString (FcStrBuf *buf, const FcChar8 *s)
{
    FcChar8 c;
    while ((c = *s++))
	if (!FcStrBufChar (buf, c))
	    return FcFalse;
    return FcTrue;
}

FcBool
FcStrBufData (FcStrBuf *buf, const FcChar8 *s, int len)
{
    while (len-- > 0)
	if (!FcStrBufChar (buf, *s++))
	    return FcFalse;
    return FcTrue;
}

FcBool
FcStrUsesHome (const FcChar8 *s)
{
    return *s == '~';
}

FcChar8 *
FcStrCopyFilename (const FcChar8 *s)
{
    FcChar8 *new;
    
    if (*s == '~')
    {
	FcChar8	*home = FcConfigHome ();
	int	size;
	if (!home)
	    return 0;
	size = strlen ((char *) home) + strlen ((char *) s);
	new = (FcChar8 *) malloc (size);
	if (!new)
	    return 0;
	FcMemAlloc (FC_MEM_STRING, size);
	strcpy ((char *) new, (char *) home);
	strcat ((char *) new, (char *) s + 1);
    }
    else
    {
	int	size = strlen ((char *) s) + 1;
	new = (FcChar8 *) malloc (size);
	if (!new)
	    return 0;
	FcMemAlloc (FC_MEM_STRING, size);
	strcpy ((char *) new, (const char *) s);
    }
    return new;
}

FcChar8 *
FcStrLastSlash (const FcChar8  *path)
{
    FcChar8	    *slash;

    slash = (FcChar8 *) strrchr ((const char *) path, '/');
#ifdef _WIN32
    {
        FcChar8     *backslash;

	backslash = (FcChar8 *) strrchr ((const char *) path, '\\');
	if (!slash || (backslash && backslash > slash))
	    slash = backslash;
    }
#endif

    return slash;
}
  
FcChar8 *
FcStrDirname (const FcChar8 *file)
{
    FcChar8 *slash;
    FcChar8 *dir;

    slash = FcStrLastSlash (file);
    if (!slash)
	return FcStrCopy ((FcChar8 *) ".");
    dir = malloc ((slash - file) + 1);
    if (!dir)
	return 0;
    FcMemAlloc (FC_MEM_STRING, (slash - file) + 1);
    strncpy ((char *) dir, (const char *) file, slash - file);
    dir[slash - file] = '\0';
    return dir;
}

FcChar8 *
FcStrBasename (const FcChar8 *file)
{
    FcChar8 *slash;

    slash = FcStrLastSlash (file);
    if (!slash)
	return FcStrCopy (file);
    return FcStrCopy (slash + 1);
}

FcStrSet *
FcStrSetCreate (void)
{
    FcStrSet	*set = malloc (sizeof (FcStrSet));
    if (!set)
	return 0;
    FcMemAlloc (FC_MEM_STRSET, sizeof (FcStrSet));
    set->ref = 1;
    set->num = 0;
    set->size = 0;
    set->strs = 0;
    return set;
}

static FcBool
_FcStrSetAppend (FcStrSet *set, FcChar8 *s)
{
    if (FcStrSetMember (set, s))
    {
	FcStrFree (s);
	return FcTrue;
    }
    if (set->num == set->size)
    {
	FcChar8	**strs = malloc ((set->size + 2) * sizeof (FcChar8 *));

	if (!strs)
	    return FcFalse;
	FcMemAlloc (FC_MEM_STRSET, (set->size + 2) * sizeof (FcChar8 *));
	set->size = set->size + 1;
	if (set->num)
	    memcpy (strs, set->strs, set->num * sizeof (FcChar8 *));
	if (set->strs)
	    free (set->strs);
	set->strs = strs;
    }
    set->strs[set->num++] = s;
    set->strs[set->num] = 0;
    return FcTrue;
}

FcBool
FcStrSetMember (FcStrSet *set, const FcChar8 *s)
{
    int	i;

    for (i = 0; i < set->num; i++)
	if (!FcStrCmp (set->strs[i], s))
	    return FcTrue;
    return FcFalse;
}

FcBool
FcStrSetEqual (FcStrSet *sa, FcStrSet *sb)
{
    int	i;
    if (sa->num != sb->num)
	return FcFalse;
    for (i = 0; i < sa->num; i++)
	if (!FcStrSetMember (sb, sa->strs[i]))
	    return FcFalse;
    return FcTrue;
}

FcBool
FcStrSetAdd (FcStrSet *set, const FcChar8 *s)
{
    FcChar8 *new = FcStrCopy (s);
    if (!new)
	return FcFalse;
    if (!_FcStrSetAppend (set, new))
    {
	FcStrFree (new);
	return FcFalse;
    }
    return FcTrue;
}

FcBool
FcStrSetAddFilename (FcStrSet *set, const FcChar8 *s)
{
    FcChar8 *new = FcStrCopyFilename (s);
    if (!new)
	return FcFalse;
    if (!_FcStrSetAppend (set, new))
    {
	FcStrFree (new);
	return FcFalse;
    }
    return FcTrue;
}

FcBool
FcStrSetDel (FcStrSet *set, const FcChar8 *s)
{
    int	i;

    for (i = 0; i < set->num; i++)
	if (!FcStrCmp (set->strs[i], s))
	{
	    FcStrFree (set->strs[i]);
	    /*
	     * copy remaining string pointers and trailing
	     * NULL
	     */
	    memmove (&set->strs[i], &set->strs[i+1], 
		     (set->num - i) * sizeof (FcChar8 *));
	    set->num--;
	    return FcTrue;
	}
    return FcFalse;
}

void
FcStrSetDestroy (FcStrSet *set)
{
    if (--set->ref == 0)
    {
	int	i;
    
	for (i = 0; i < set->num; i++)
	    FcStrFree (set->strs[i]);
	FcMemFree (FC_MEM_STRSET, (set->size) * sizeof (FcChar8 *));
	if (set->strs)
	    free (set->strs);
	FcMemFree (FC_MEM_STRSET, sizeof (FcStrSet));
	free (set);
    }
}

FcStrList *
FcStrListCreate (FcStrSet *set)
{
    FcStrList	*list;

    list = malloc (sizeof (FcStrList));
    if (!list)
	return 0;
    FcMemAlloc (FC_MEM_STRLIST, sizeof (FcStrList));
    list->set = set;
    set->ref++;
    list->n = 0;
    return list;
}

FcChar8 *
FcStrListNext (FcStrList *list)
{
    if (list->n >= list->set->num)
	return 0;
    return list->set->strs[list->n++];
}

void
FcStrListDone (FcStrList *list)
{
    FcStrSetDestroy (list->set);
    FcMemFree (FC_MEM_STRLIST, sizeof (FcStrList));
    free (list);
}
