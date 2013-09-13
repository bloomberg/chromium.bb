/*
 * Copyright © 2012 Intel Corporation
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
#ifndef _WESTON_VERTEX_CLIPPING_H
#define _WESTON_VERTEX_CLIPPING_H

#include <GLES2/gl2.h>

struct polygon8 {
	GLfloat x[8];
	GLfloat y[8];
	int n;
};

struct clip_context {
	struct {
		GLfloat x;
		GLfloat y;
	} prev;

	struct {
		GLfloat x1, y1;
		GLfloat x2, y2;
	} clip;

	struct {
		GLfloat *x;
		GLfloat *y;
	} vertices;
};

GLfloat
float_difference(GLfloat a, GLfloat b);

int
clip_simple(struct clip_context *ctx,
	    struct polygon8 *surf,
	    GLfloat *ex,
	    GLfloat *ey);

int
clip_transformed(struct clip_context *ctx,
		 struct polygon8 *surf,
		 GLfloat *ex,
		 GLfloat *ey);\

#endif
