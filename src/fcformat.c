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


/*
 * Some ideas for future syntax extensions:
 *
 * - number of values for element using '%{#elt}'
 * - allow indexing subexprs using '%{[idx]elt1,elt2{subexpr}}'
 * - allow indexing simple tags using '%{elt[idx]}'
 * - conditional/filtering/deletion on binding (using '(w)'/'(s)' notation)
 */

static void
message (const char *fmt, ...)
{
    va_list	args;
    va_start (args, fmt);
    fprintf (stderr, "Fontconfig: Pattern format error: ");
    vfprintf (stderr, fmt, args);
    fprintf (stderr, ".\n");
    va_end (args);
}


typedef struct _FcFormatContext
{
    const FcChar8 *format_orig;
    const FcChar8 *format;
    int            format_len;
    FcChar8       *word;
} FcFormatContext;

static FcBool
FcFormatContextInit (FcFormatContext *c,
		     const FcChar8   *format)
{
    c->format_orig = c->format = format;
    c->format_len = strlen ((const char *) format);
    c->word = malloc (c->format_len + 1);

    return c->word != NULL;
}

static void
FcFormatContextDone (FcFormatContext *c)
{
    if (c)
    {
	free (c->word);
    }
}

static FcBool
consume_char (FcFormatContext *c,
	      FcChar8          term)
{
    if (*c->format != term)
	return FcFalse;

    c->format++;
    return FcTrue;
}

static FcBool
expect_char (FcFormatContext *c,
	      FcChar8          term)
{
    FcBool res = consume_char (c, term);
    if (!res)
    {
	if (c->format == c->format_orig + c->format_len)
	    message ("format ended while expecting '%c'",
		     term);
	else
	    message ("expected '%c' at %d",
		     term, c->format - c->format_orig + 1);
    }
    return res;
}

static FcBool
FcCharIsPunct (const FcChar8 c)
{
    if (c < '0')
	return FcTrue;
    if (c <= '9')
	return FcFalse;
    if (c < 'A')
	return FcTrue;
    if (c <= 'Z')
	return FcFalse;
    if (c < 'a')
	return FcTrue;
    if (c <= 'z')
	return FcFalse;
    if (c <= '~')
	return FcTrue;
    return FcFalse;
}

static FcBool
read_word (FcFormatContext *c)
{
    FcChar8 *p;

    p = c->word;

    while (*c->format)
    {
	if (*c->format == '\\')
	{
	    c->format++;
	    if (*c->format)
		c->format++;
	    continue;
	}
	else if (FcCharIsPunct (*c->format))
	    break;

	*p++ = *c->format++;
    }
    *p = '\0';

    if (p == c->word)
    {
	message ("expected element name at %d",
		 c->format - c->format_orig + 1);
	return FcFalse;
    }

    return FcTrue;
}

static FcBool
interpret_expr (FcFormatContext *c,
		FcPattern       *pat,
		FcStrBuf        *buf,
		FcChar8          term);

static FcBool
interpret_subexpr (FcFormatContext *c,
		   FcPattern       *pat,
		   FcStrBuf        *buf)
{
    return expect_char (c, '{') &&
	   interpret_expr (c, pat, buf, '}') &&
	   expect_char (c, '}');
}

static FcBool
maybe_interpret_subexpr (FcFormatContext *c,
			 FcPattern       *pat,
			 FcStrBuf        *buf)
{
    return (*c->format == '{') ?
	   interpret_subexpr (c, pat, buf) :
	   FcTrue;
}

static FcBool
skip_subexpr (FcFormatContext *c);

static FcBool
skip_percent (FcFormatContext *c)
{
    int width;

    if (!expect_char (c, '%'))
	return FcFalse;

    /* skip an optional width specifier */
    width = strtol ((const char *) c->format, (char **) &c->format, 10);

    if (!expect_char (c, '{'))
	return FcFalse;

    while(*c->format && *c->format != '}')
    {
	switch (*c->format)
	{
	case '\\':
	    c->format++; /* skip over '\\' */
	    if (*c->format)
		c->format++;
	    continue;
	case '{':
	    if (!skip_subexpr (c))
		return FcFalse;
	    continue;
	}
	c->format++;
    }

    return expect_char (c, '}');
}

static FcBool
skip_expr (FcFormatContext *c)
{
    while(*c->format && *c->format != '}')
    {
	switch (*c->format)
	{
	case '\\':
	    c->format++; /* skip over '\\' */
	    if (*c->format)
		c->format++;
	    continue;
	case '%':
	    if (!skip_percent (c))
		return FcFalse;
	    continue;
	}
	c->format++;
    }

    return FcTrue;
}

static FcBool
skip_subexpr (FcFormatContext *c)
{
    return expect_char (c, '{') &&
	   skip_expr (c) &&
	   expect_char (c, '}');
}

static FcBool
maybe_skip_subexpr (FcFormatContext *c)
{
    return (*c->format == '{') ?
	   skip_subexpr (c) :
	   FcTrue;
}

static FcBool
interpret_simple (FcFormatContext *c,
		  FcPattern       *pat,
		  FcStrBuf        *buf)
{
    FcPatternElt *e;
    FcBool        add_colon = FcFalse;
    FcBool        add_elt_name = FcFalse;

    if (consume_char (c, ':'))
	add_colon = FcTrue;

    if (!read_word (c))
	return FcFalse;

    if (consume_char (c, '='))
	add_elt_name = FcTrue;

    e = FcPatternObjectFindElt (pat,
				FcObjectFromName ((const char *) c->word));
    if (e)
    {
	FcValueListPtr l;

	if (add_colon)
	    FcStrBufChar (buf, ':');
	if (add_elt_name)
	{
	    FcStrBufString (buf, c->word);
	    FcStrBufChar (buf, '=');
	}

	l = FcPatternEltValues(e);
	FcNameUnparseValueList (buf, l, '\0');
    }

    return FcTrue;
}

static FcBool
interpret_filter (FcFormatContext *c,
		  FcPattern       *pat,
		  FcStrBuf        *buf)
{
    FcObjectSet  *os;
    FcPattern    *subpat;

    if (!expect_char (c, '+'))
	return FcFalse;

    os = FcObjectSetCreate ();
    if (!os)
	return FcFalse;

    do
    {
	if (!read_word (c) ||
	    !FcObjectSetAdd (os, (const char *) c->word))
	{
	    FcObjectSetDestroy (os);
	    return FcFalse;
	}
    }
    while (consume_char (c, ','));

    subpat = FcPatternFilter (pat, os);
    FcObjectSetDestroy (os);

    if (!subpat ||
	!interpret_subexpr (c, subpat, buf))
	return FcFalse;

    FcPatternDestroy (subpat);
    return FcTrue;
}

static FcBool
interpret_delete (FcFormatContext *c,
		  FcPattern       *pat,
		  FcStrBuf        *buf)
{
    FcPattern    *subpat;

    if (!expect_char (c, '-'))
	return FcFalse;

    subpat = FcPatternDuplicate (pat);
    if (!subpat)
	return FcFalse;

    do
    {
	if (!read_word (c))
	{
	    FcPatternDestroy (subpat);
	    return FcFalse;
	}

	FcPatternDel (subpat, (const char *) c->word);
    }
    while (consume_char (c, ','));

    if (!interpret_subexpr (c, subpat, buf))
	return FcFalse;

    FcPatternDestroy (subpat);
    return FcTrue;
}

static FcBool
interpret_cond (FcFormatContext *c,
		FcPattern       *pat,
		FcStrBuf        *buf)
{
    FcBool pass;

    if (!expect_char (c, '?'))
	return FcFalse;

    pass = FcTrue;

    do
    {
	FcBool negate;
	FcValue v;

	negate = consume_char (c, '!');

	if (!read_word (c))
	    return FcFalse;

	pass = pass &&
	       (negate ^
		(FcResultMatch == FcPatternGet (pat,
						(const char *) c->word,
						0, &v)));
    }
    while (consume_char (c, ','));

    if (pass)
    {
	if (!interpret_subexpr  (c, pat, buf) ||
	    !maybe_skip_subexpr (c))
	    return FcFalse;
    }
    else
    {
	if (!skip_subexpr (c) ||
	    !maybe_interpret_subexpr  (c, pat, buf))
	    return FcFalse;
    }

    return FcTrue;
}

static FcChar8 *
cescape (const FcChar8 *str)
{
    FcStrBuf buf;
    FcChar8         buf_static[8192];

    FcStrBufInit (&buf, buf_static, sizeof (buf_static));
    while(*str)
    {
	switch (*str)
	{
	case '\\':
	case '"':
	    FcStrBufChar (&buf, '\\');
	    break;
	}
	FcStrBufChar (&buf, *str++);
    }
    return FcStrBufDone (&buf);
}

static FcChar8 *
shescape (const FcChar8 *str)
{
    FcStrBuf buf;
    FcChar8         buf_static[8192];

    FcStrBufInit (&buf, buf_static, sizeof (buf_static));
    FcStrBufChar (&buf, '\'');
    while(*str)
    {
	if (*str == '\'')
	    FcStrBufString (&buf, (const FcChar8 *) "'\\''");
	else
	    FcStrBufChar (&buf, *str);
	str++;
    }
    FcStrBufChar (&buf, '\'');
    return FcStrBufDone (&buf);
}

static FcChar8 *
xmlescape (const FcChar8 *str)
{
    FcStrBuf buf;
    FcChar8         buf_static[8192];

    FcStrBufInit (&buf, buf_static, sizeof (buf_static));
    while(*str)
    {
	switch (*str)
	{
	case '&': FcStrBufString (&buf, (const FcChar8 *) "&amp;"); break;
	case '<': FcStrBufString (&buf, (const FcChar8 *) "&lt;");  break;
	case '>': FcStrBufString (&buf, (const FcChar8 *) "&gt;");  break;
	default:  FcStrBufChar   (&buf, *str);                      break;
	}
	str++;
    }
    return FcStrBufDone (&buf);
}

static FcChar8 *
convert (FcFormatContext *c,
	 const FcChar8   *str)
{
    if (!read_word (c))
	return NULL;
#define CONVERTER(name, func) \
    else if (0 == strcmp ((const char *) c->word, name))\
	return func (str)
    CONVERTER ("downcase",  FcStrDowncase);
    CONVERTER ("basename",  FcStrBasename);
    CONVERTER ("dirname",   FcStrDirname);
    CONVERTER ("cescape",   cescape);
    CONVERTER ("shescape",  shescape);
    CONVERTER ("xmlescape", xmlescape);

    message ("unknown converter \"%s\"",
	     c->word);
    return NULL;
}

static FcBool
maybe_interpret_converts (FcFormatContext *c,
			   FcStrBuf        *buf,
			   int              start)
{
    while (consume_char (c, '|'))
    {
	const FcChar8 *str;
	FcChar8       *new_str;

	/* nul-terminate the buffer */
	FcStrBufChar (buf, '\0');
	if (buf->failed)
	    return FcFalse;
	str = buf->buf + start;

	if (!(new_str = convert (c, str)))
	    return FcFalse;

	/* replace in the buffer */
	buf->len = start;
	FcStrBufString (buf, new_str);
	free (new_str);
    }

    return FcTrue;
}

static FcBool
align_to_width (FcStrBuf *buf,
		int       start,
		int       width)
{
    int len;

    if (buf->failed)
	return FcFalse;

    len = buf->len - start;
    if (len < -width)
    {
	/* left align */
	while (len++ < -width)
	    FcStrBufChar (buf, ' ');
    }
    else if (len < width)
    {
	int old_len;
	old_len = len;
	/* right align */
	while (len++ < width)
	    FcStrBufChar (buf, ' ');
	if (buf->failed)
	    return FcFalse;
	len = old_len;
	memmove (buf->buf + buf->len - len,
		 buf->buf + buf->len - width,
		 len);
	memset (buf->buf + buf->len - width,
		' ',
		width - len);
    }

    return !buf->failed;
}
static FcBool
interpret_percent (FcFormatContext *c,
		   FcPattern       *pat,
		   FcStrBuf        *buf)
{
    int width, start;
    FcBool ret;

    if (!expect_char (c, '%'))
	return FcFalse;

    if (consume_char (c, '%')) /* "%%" */
    {
	FcStrBufChar (buf, '%');
	return FcTrue;
    }

    /* parse an optional width specifier */
    width = strtol ((const char *) c->format, (char **) &c->format, 10);

    if (!expect_char (c, '{'))
	return FcFalse;

    start = buf->len;

    switch (*c->format) {
    case '{': ret = interpret_subexpr (c, pat, buf); break;
    case '+': ret = interpret_filter  (c, pat, buf); break;
    case '-': ret = interpret_delete  (c, pat, buf); break;
    case '?': ret = interpret_cond    (c, pat, buf); break;
    default:  ret = interpret_simple  (c, pat, buf); break;
    }

    return ret &&
	   maybe_interpret_converts (c, buf, start) &&
	   align_to_width (buf, start, width) &&
	   expect_char (c, '}');
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

static FcBool
interpret_expr (FcFormatContext *c,
		FcPattern       *pat,
		FcStrBuf        *buf,
		FcChar8          term)
{
    while (*c->format && *c->format != term)
    {
	switch (*c->format)
	{
	case '\\':
	    c->format++; /* skip over '\\' */
	    if (*c->format)
		FcStrBufChar (buf, escaped_char (*c->format++));
	    continue;
	case '%':
	    if (!interpret_percent (c, pat, buf))
		return FcFalse;
	    continue;
	}
	FcStrBufChar (buf, *c->format++);
    }
    return FcTrue;
}

FcChar8 *
FcPatternFormat (FcPattern *pat, const FcChar8 *format)
{
    FcStrBuf        buf;
    FcChar8         buf_static[8192];
    FcFormatContext c;
    FcBool          ret;

    FcStrBufInit (&buf, buf_static, sizeof (buf_static));
    if (!FcFormatContextInit (&c, format))
	return NULL;

    ret = interpret_expr (&c, pat, &buf, '\0');

    FcFormatContextDone (&c);
    if (ret)
	return FcStrBufDone (&buf);
    else
    {
	FcStrBufDestroy (&buf);
	return NULL;
    }
}

#define __fcformat__
#include "fcaliastail.h"
#undef __fcformat__
