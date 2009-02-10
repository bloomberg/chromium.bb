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


typedef struct _FcFormatContext
{
    const FcChar8 *format_orig;
    const FcChar8 *format;
    int            format_len;
    FcChar8       *scratch;
} FcFormatContext;

static void
FcFormatContextInit (FcFormatContext *c,
		     const FcChar8   *format)
{
    c->format_orig = c->format = format;
    c->format_len = strlen ((const char *) format);
    c->scratch = malloc (c->format_len + 1);
}

static void
FcFormatContextDone (FcFormatContext *c)
{
    if (c)
    {
	free (c->scratch);
    }
}

static FcBool
consume_char (FcFormatContext *c,
	      FcChar8          term)
{
    if (*c->format != term)
    {
	message ("Pattern format error: expected '%c' at %d",
		 term, c->format - c->format_orig + 1);
	return FcFalse;
    }

    c->format++;
    return FcTrue;
}

static void
interpret_percent (FcFormatContext *c,
		   FcPattern       *pat,
		   FcStrBuf        *buf)
{
    int width, before;
    FcChar8 *p;
    FcPatternElt *e;

    if (!consume_char (c, '%'))
	return;

    if (*c->format == '%') /* "%%" */
    {
	FcStrBufChar (buf, *c->format++);
	return;
    }

    /* parse an optional width specifier */
    width = strtol ((const char *) c->format, (char **) &c->format, 10);

    before = buf->len;

    if (!consume_char (c, '{'))
	return;

    p = (FcChar8 *) strpbrk ((const char *) c->format, "}" /* "=?:}" */);
    if (!p)
    {
	message ("Pattern format missing closing brace for opening brace at %d",
		 c->format-1 - c->format_orig + 1);
	return;
    }
    /* extract the element name */
    memcpy (c->scratch, c->format, p - c->format);
    c->scratch[p - c->format] = '\0';
    c->format = p;

    e = FcPatternObjectFindElt (pat, FcObjectFromName ((const char *) c->scratch));
    if (e)
    {
	FcValueListPtr l;
	l = FcPatternEltValues(e);
	FcNameUnparseValueList (buf, l, '\0');
    }

    /* handle filters, if any */
    /* XXX */

    /* align to width */
    if (!buf->failed)
    {
	int after, len;

	after = buf->len;

	len = after - before;

	if (len < -width)
	{
	    /* left align */
	    while (len++ < -width)
		FcStrBufChar (buf, ' ');
	}
	else if (len < width)
	{
	    /* right align */
	    while (len++ < width)
		FcStrBufChar (buf, ' ');
	    len = after - before;
	    memmove (buf->buf + buf->len - len,
		     buf->buf + buf->len - width,
		     len);
	    memset (buf->buf + buf->len - width,
		    ' ',
		    width - len);
	}
    }

    consume_char (c, '}');
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

static void
interpret (FcFormatContext *c,
	   FcPattern       *pat,
	   FcStrBuf        *buf,
	   FcChar8          term)
{
    for (; *c->format && *c->format != term;)
    {
	switch (*c->format)
	{
	case '\\':
	    c->format++; /* skip over '\\' */
	    if (*c->format)
		FcStrBufChar (buf, escaped_char (*c->format++));
	    continue;
	case '%':
	    interpret_percent (c, pat, buf);
	    continue;
	}
	FcStrBufChar (buf, *c->format++);
    }
    if (*c->format != term)
	message ("Pattern format ended while looking for '%c'", term);
}

FcChar8 *
FcPatternFormat (FcPattern *pat, const FcChar8 *format)
{
    FcStrBuf buf;
    FcFormatContext c;

    FcStrBufInit (&buf, 0, 0);
    FcFormatContextInit (&c, format);

    interpret (&c, pat, &buf, '\0');

    FcFormatContextDone (&c);
    return FcStrBufDone (&buf);
}

#define __fcformat__
#include "fcaliastail.h"
#undef __fcformat__
