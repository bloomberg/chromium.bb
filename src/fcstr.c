/*
 * $XFree86: $
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
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

char *
FcStrCopy (const char *s)
{
    char	*r;

    if (!s)
	return 0;
    r = (char *) malloc (strlen (s) + 1);
    if (!r)
	return 0;
    FcMemAlloc (FC_MEM_STRING, strlen (s) + 1);
    strcpy (r, s);
    return r;
}

char *
FcStrPlus (const char *s1, const char *s2)
{
    int	    l = strlen (s1) + strlen (s2) + 1;
    char    *s = malloc (l);

    if (!s)
	return 0;
    FcMemAlloc (FC_MEM_STRING, l);
    strcpy (s, s1);
    strcat (s, s2);
    return s;
}

void
FcStrFree (char *s)
{
    FcMemFree (FC_MEM_STRING, strlen (s) + 1);
    free (s);
}

int
FcStrCmpIgnoreCase (const char *s1, const char *s2)
{
    char    c1, c2;
    
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
    return (int) c2 - (int) c1;
}

int
FcUtf8ToUcs4 (FcChar8   *src_orig,
	      FcChar32  *dst,
	      int	len)
{
    FcChar8	*src = src_orig;
    FcChar8	s;
    int		extra;
    FcChar32	result;

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
FcUtf8Len (FcChar8	*string,
	    int		len,
	    int		*nchar,
	    int		*wchar)
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
