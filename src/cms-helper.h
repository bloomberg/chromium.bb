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

#ifndef _WESTON_CMS_H_
#define _WESTON_CMS_H_

#include "config.h"

#include "compositor.h"

/* General overview on how to be a CMS plugin:
 *
 * First, some nomenclature:
 *
 *  CMF:       Color management framework, i.e. "Use foo.icc for device $bar"
 *  CMM:       Color management module that converts pixel colors, which is
 *             usually lcms2 on any modern OS.
 *  CMS:       Color management system that encompasses both a CMF and CMM.
 *  ICC:       International Color Consortium, the people that define the
 *             binary encoding of a .icc file.
 *  VCGT:      Video Card Gamma Tag. An Apple extension to the ICC specification
 *             that allows the calibration state to be stored in the ICC profile
 *  Output:    Physical port with a display attached, e.g. LVDS1
 *
 * As a CMF is probably something you don't want or need on an embeded install
 * these functions will not be called if the icc_profile key is set for a
 * specific [output] section in weston.ini
 *
 * Most desktop environments want the CMF to decide what profile to use in
 * different situations, so that displays can be profiled and also so that
 * the ICC profiles can be changed at runtime depending on the task or ambient
 * environment.
 *
 * The CMF can be selected using the 'modules' key in the [core] section.
 */

struct weston_color_profile {
	char	*filename;
	void	*lcms_handle;
};

void
weston_cms_set_color_profile(struct weston_output *o,
			     struct weston_color_profile *p);
struct weston_color_profile *
weston_cms_create_profile(const char *filename,
			  void *lcms_profile);
struct weston_color_profile *
weston_cms_load_profile(const char *filename);
void
weston_cms_destroy_profile(struct weston_color_profile *p);

#endif
