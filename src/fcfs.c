/*
 * $RCSId: $
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
#include "fcint.h"

FcFontSet *
FcFontSetCreate (void)
{
    FcFontSet	*s;

    s = (FcFontSet *) malloc (sizeof (FcFontSet));
    if (!s)
	return 0;
    FcMemAlloc (FC_MEM_FONTSET, sizeof (FcFontSet));
    s->nfont = 0;
    s->sfont = 0;
    s->fonts = 0;
    return s;
}

void
FcFontSetDestroy (FcFontSet *s)
{
    int	    i;

    for (i = 0; i < s->nfont; i++)
	FcPatternDestroy (s->fonts[i]);
    if (s->fonts)
    {
	FcMemFree (FC_MEM_FONTPTR, s->sfont * sizeof (FcPattern *));
	free (s->fonts);
    }
    FcMemFree (FC_MEM_FONTSET, sizeof (FcFontSet));
    free (s);
}

FcBool
FcFontSetAdd (FcFontSet *s, FcPattern *font)
{
    FcPattern	**f;
    int		sfont;
    
    if (s->nfont == s->sfont)
    {
	sfont = s->sfont + 32;
	if (s->fonts)
	    f = (FcPattern **) realloc (s->fonts, sfont * sizeof (FcPattern *));
	else
	    f = (FcPattern **) malloc (sfont * sizeof (FcPattern *));
	if (!f)
	    return FcFalse;
	if (s->sfont)
	    FcMemFree (FC_MEM_FONTPTR, s->sfont * sizeof (FcPattern *));
	FcMemAlloc (FC_MEM_FONTPTR, sfont * sizeof (FcPattern *));
	s->sfont = sfont;
	s->fonts = f;
    }
    s->fonts[s->nfont++] = font;
    return FcTrue;
}

static int * fcfs_pat_count;

void
FcFontSetNewBank (void)
{
    FcPatternNewBank();
}

int
FcFontSetNeededBytes (FcFontSet *s)
{
    int i, c, cum = 0;

    for (i = 0; i < s->nfont; i++)
    {
	c = FcPatternNeededBytes(s->fonts[i]);
	if (c < 0)
	    return c;
	cum += c;
    }

    if (cum > 0)
	return cum + sizeof(int) + FcObjectNeededBytes();
    else
	return 0;
}

/* Returns an overestimate of the number of bytes that
 * might later get eaten up by padding in the ALIGN macro. */
int
FcFontSetNeededBytesAlign (void)
{
    return __alignof__(int) + 
	FcPatternNeededBytesAlign () + FcObjectNeededBytesAlign ();
}

void *
FcFontSetDistributeBytes (FcCache * metadata, void * block_ptr)
{
    block_ptr = ALIGN (block_ptr, int);
    fcfs_pat_count = (int *)block_ptr;
    block_ptr = (int *)block_ptr + 1;
    // we don't consume any bytes for the fontset itself,
    // since we don't allocate it statically.
    block_ptr = FcPatternDistributeBytes (metadata, block_ptr);

    // for good measure, write out the object ids used for
    // this bank to the file.
    return FcObjectDistributeBytes (metadata, block_ptr);
}

FcBool
FcFontSetSerialize (int bank, FcFontSet * s)
{
    int i;
    FcPattern * p;
    *fcfs_pat_count = s->nfont;

    for (i = 0; i < s->nfont; i++)
    {
	p = FcPatternSerialize (bank, s->fonts[i]);
	if (!p) return FcFalse;
    }
    FcObjectSerialize();

    return FcTrue;
}

FcBool
FcFontSetUnserialize(FcCache * metadata, FcFontSet * s, void * block_ptr)
{
    int nfont;
    int i, n;

    block_ptr = ALIGN (block_ptr, int);
    nfont = *(int *)block_ptr;
    block_ptr = (int *)block_ptr + 1;

    if (s->sfont < s->nfont + nfont)
    {
	int sfont = s->nfont + nfont;
	FcPattern ** pp;
	pp = realloc (s->fonts, sfont * sizeof (FcPattern));
	if (!pp)
	    return FcFalse;
	s->fonts = pp;
	s->sfont = sfont;
    }
    n = s->nfont;
    s->nfont += nfont;

    if (nfont > 0)
    {
	FcPattern * p = (FcPattern *)block_ptr;

        /* The following line is a bit counterintuitive.  The usual
         * convention is that FcPatternUnserialize is responsible for
         * aligning the FcPattern.  However, the FontSet also stores
         * the FcPatterns in its own array, so we need to align here
         * too. */
        p = ALIGN(p, FcPattern);
	for (i = 0; i < nfont; i++)
	    s->fonts[n + i] = p+i;

	block_ptr = FcPatternUnserialize (metadata, block_ptr);
	block_ptr = FcObjectUnserialize (metadata, block_ptr);
    }

    return block_ptr != 0;
}
