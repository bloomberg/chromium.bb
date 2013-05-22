/*
 * Copyright © 2013 Richard Hughes
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_LCMS
#include <lcms2.h>
#endif

#include "compositor.h"
#include "cms-helper.h"

#ifdef HAVE_LCMS
static void
weston_cms_gamma_clear(struct weston_output *o)
{
	int i;
	uint16_t *red;

	if (!o->set_gamma)
		return;

	red = calloc(o->gamma_size, sizeof(uint16_t));
	for (i = 0; i < o->gamma_size; i++)
		red[i] = (uint32_t) 0xffff * (uint32_t) i / (uint32_t) (o->gamma_size - 1);
	o->set_gamma(o, o->gamma_size, red, red, red);
	free(red);
}
#endif

void
weston_cms_set_color_profile(struct weston_output *o,
			     struct weston_color_profile *p)
{
#ifdef HAVE_LCMS
	cmsFloat32Number in;
	const cmsToneCurve **vcgt;
	int i;
	int size;
	uint16_t *red = NULL;
	uint16_t *green = NULL;
	uint16_t *blue = NULL;

	if (!o->set_gamma)
		return;
	if (!p) {
		weston_cms_gamma_clear(o);
		return;
	}

	weston_log("Using ICC profile %s\n", p->filename);
	vcgt = cmsReadTag (p->lcms_handle, cmsSigVcgtTag);
	if (vcgt == NULL || vcgt[0] == NULL) {
		weston_cms_gamma_clear(o);
		return;
	}

	size = o->gamma_size;
	red = calloc(size, sizeof(uint16_t));
	green = calloc(size, sizeof(uint16_t));
	blue = calloc(size, sizeof(uint16_t));
	for (i = 0; i < size; i++) {
		in = (cmsFloat32Number) i / (cmsFloat32Number) (size - 1);
		red[i] = cmsEvalToneCurveFloat(vcgt[0], in) * (double) 0xffff;
		green[i] = cmsEvalToneCurveFloat(vcgt[1], in) * (double) 0xffff;
		blue[i] = cmsEvalToneCurveFloat(vcgt[2], in) * (double) 0xffff;
	}
	o->set_gamma(o, size, red, green, blue);
	free(red);
	free(green);
	free(blue);
#endif
}

void
weston_cms_destroy_profile(struct weston_color_profile *p)
{
	if (!p)
		return;
#ifdef HAVE_LCMS
	cmsCloseProfile(p->lcms_handle);
#endif
	free(p->filename);
	free(p);
}

struct weston_color_profile *
weston_cms_create_profile(const char *filename,
			  void *lcms_profile)
{
	struct weston_color_profile *p;
	p = calloc(1, sizeof(struct weston_color_profile));
	p->filename = strdup(filename);
	p->lcms_handle = lcms_profile;
	return p;
}

struct weston_color_profile *
weston_cms_load_profile(const char *filename)
{
	struct weston_color_profile *p = NULL;
#ifdef HAVE_LCMS
	cmsHPROFILE lcms_profile;
	lcms_profile = cmsOpenProfileFromFile(filename, "r");
	if (lcms_profile)
		p = weston_cms_create_profile(filename, lcms_profile);
#endif
	return p;
}
