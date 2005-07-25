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

FcBool
FcFontSetPrepareSerialize (FcFontSet *s)
{
    int i;

    for (i = 0; i < s->nfont; i++)
	if (!FcPatternPrepareSerialize(s->fonts[i]))
	    return FcFalse;

    return FcTrue;
}

FcBool
FcFontSetSerialize (FcFontSet * s)
{
    int i;
    FcPattern * p;

    for (i = 0; i < s->nfont; i++)
    {
	p = FcPatternSerialize (s->fonts[i]);
	if (!p) return FcFalse;
	FcPatternDestroy (s->fonts[i]);

	s->fonts[i] = p;
    }

    return FcTrue;
}

void
FcFontSetClearStatic (void)
{
    FcPatternClearStatic();
}

FcBool
FcFontSetRead(int fd, FcConfig * config, FcCache metadata)
{
    int i, mz, j;
    FcPattern * buf;

    lseek(fd, metadata.fontsets_offset, SEEK_SET);
    for (i = FcSetSystem; i <= FcSetApplication; i++)
    {
        if (config->fonts[i])
        {
            if (config->fonts[i]->nfont > 0 && config->fonts[i]->fonts)
                free (config->fonts[i]->fonts);
            free (config->fonts[i]);
        }
    }

    for (i = FcSetSystem; i <= FcSetApplication; i++)
    {
        read(fd, &mz, sizeof(int));
        if (mz != FC_CACHE_MAGIC)
            continue;

        config->fonts[i] = malloc(sizeof(FcFontSet));
        if (!config->fonts[i])
            return FcFalse;
        FcMemAlloc(FC_MEM_FONTSET, sizeof(FcFontSet));

        if (read(fd, config->fonts[i], sizeof(FcFontSet)) == -1)
            goto bail;
        if (config->fonts[i]->sfont > 0)
        {
            config->fonts[i]->fonts = malloc
                (config->fonts[i]->sfont*sizeof(FcPattern *));
	    buf = malloc (config->fonts[i]->sfont * sizeof(FcPattern));
	    if (!config->fonts[i]->fonts || !buf)
		goto bail;
	    for (j = 0; j < config->fonts[i]->nfont; j++)
	    {
		config->fonts[i]->fonts[j] = buf+j;
		if (read(fd, buf+j, sizeof(FcPattern)) == -1)
		    goto bail;
	    }
        }
    }

    return FcTrue;
 bail:
    for (i = FcSetSystem; i <= FcSetApplication; i++)
    {
        if (config->fonts[i])
        {
            if (config->fonts[i]->fonts)
                free (config->fonts[i]->fonts);
            free(config->fonts[i]);
        }
        config->fonts[i] = 0;
    }
    return FcFalse;
}

FcBool
FcFontSetWrite(int fd, FcConfig * config, FcCache * metadata)
{
    int c, t, i, j;
    int m = FC_CACHE_MAGIC, z = 0;

    metadata->fontsets_offset = FcCacheNextOffset(fd);
    lseek(fd, metadata->fontsets_offset, SEEK_SET);
    for (i = FcSetSystem; i <= FcSetApplication; i++)
    {
        if (!config->fonts[i])
        {
            write(fd, &z, sizeof(int));
            continue;
        }
        else
            write(fd, &m, sizeof(int));

        if ((c = write(fd, config->fonts[i], sizeof(FcFontSet))) == -1)
            return FcFalse;
        t = c;
        if (config->fonts[i]->nfont > 0)
        {
	    for (j = 0; j < config->fonts[i]->nfont; j++)
	    {
		if ((c = write(fd, config->fonts[i]->fonts[j],
			       sizeof(FcPattern))) == -1)
		    return FcFalse;
	    }
        }
    }
    return FcTrue;
}
