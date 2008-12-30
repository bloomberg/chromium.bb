/*
 * Copyright © 2008 Red Hat, Inc.
 *
 * Red Hat Author(s): Behdad Esfahbod
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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


static void
message (const char *fmt, ...)
{
    va_list	args;
    va_start (args, fmt);
    fprintf (stderr, "Fontconfig: ");
    vfprintf (stderr, fmt, args);
    fprintf (stderr, "\n");
    va_end (args);
}


static FcChar8 *scratch1;
static FcChar8 *scratch2;
static const FcChar8 *format_orig;

static const FcChar8 *
interpret_percent (FcPattern *pat,
		   FcStrBuf *buf,
		   const FcChar8 *format)
{
    switch (*format) {
    case '{':
    {
	FcChar8 *p;
	FcPatternElt *e;

	format++; /* skip over '{' */

	p = (FcChar8 *) strpbrk ((const char *) format, "}");
	if (!p)
	{
	    message ("Pattern format missing closing brace");
	    return format;
	}
	/* extract the element name */
	memcpy (scratch1, format, p - format);
	scratch1[p - format] = '\0';

	e = FcPatternObjectFindElt (pat, FcObjectFromName ((const char *) scratch1));
	if (e)
	{
	    FcValueListPtr l;
	    l = FcPatternEltValues(e);
	    FcNameUnparseValueList (buf, l, '\0');
	}

	p++; /* skip over '}' */
	return p;
    }
    default:
	message ("Pattern format has invalid character after '%%' at %d",
		 format - format_orig);
	return format;
    }
}

static char escaped_char(const char ch)
{
    switch (ch) {
    case 'a':   return '\a';
    case 'b':   return '\b';
    case 'f':   return '\f';
    case 'n':   return '\n';
    case 'r':   return '\r';
    case 't':   return '\t';
    case 'v':   return '\v';
    default:    return ch;
    }
}

static const FcChar8 *
interpret (FcPattern *pat,
	   FcStrBuf *buf,
	   const FcChar8 *format,
	   FcChar8 term)
{
    const FcChar8 *end;

    for (end = format; *end && *end != term;)
    {
	switch (*end)
	{
	case '\\':
	    end++; /* skip over '\\' */
	    FcStrBufChar (buf, escaped_char (*end++));
	    continue;
	case '%':
	    end++; /* skip over '%' */
	    if (*end == '%')
		break;
	    end = interpret_percent (pat, buf, end);
	    continue;
	}
	FcStrBufChar (buf, *end);
	end++;
    }
    if (*end != term)
	message ("Pattern format ended while looking for '%c'", term);

    return end;
}

FcChar8 *
FcPatternFormat (FcPattern *pat, const FcChar8 *format)
{
    int len;
    FcStrBuf buf;

    FcStrBufInit (&buf, 0, 0);
    len = strlen ((const char *) format);
    scratch1 = malloc (len);
    scratch2 = malloc (len);
    format_orig = format;

    interpret (pat, &buf, format, '\0');

    free (scratch1);
    free (scratch2);
    return FcStrBufDone (&buf);
}

#define __fcformat__
#include "fcaliastail.h"
#undef __fcformat__
