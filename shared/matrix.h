/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2012 Collabora, Ltd.
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

#ifndef WESTON_MATRIX_H
#define WESTON_MATRIX_H

#ifdef  __cplusplus
extern "C" {
#endif

enum weston_matrix_transform_type {
	WESTON_MATRIX_TRANSFORM_TRANSLATE	= (1 << 0),
	WESTON_MATRIX_TRANSFORM_SCALE		= (1 << 1),
	WESTON_MATRIX_TRANSFORM_ROTATE		= (1 << 2),
	WESTON_MATRIX_TRANSFORM_OTHER		= (1 << 3),
};

struct weston_matrix {
	float d[16];
	unsigned int type;
};

struct weston_vector {
	float f[4];
};

void
weston_matrix_init(struct weston_matrix *matrix);
void
weston_matrix_multiply(struct weston_matrix *m, const struct weston_matrix *n);
void
weston_matrix_scale(struct weston_matrix *matrix, float x, float y, float z);
void
weston_matrix_translate(struct weston_matrix *matrix,
			float x, float y, float z);
void
weston_matrix_rotate_xy(struct weston_matrix *matrix, float cos, float sin);
void
weston_matrix_transform(struct weston_matrix *matrix, struct weston_vector *v);

int
weston_matrix_invert(struct weston_matrix *inverse,
		     const struct weston_matrix *matrix);

#ifdef UNIT_TEST
#  define MATRIX_TEST_EXPORT WL_EXPORT

int
matrix_invert(double *A, unsigned *p, const struct weston_matrix *matrix);

void
inverse_transform(const double *LU, const unsigned *p, float *v);

#else
#  define MATRIX_TEST_EXPORT static
#endif

#ifdef  __cplusplus
}
#endif

#endif /* WESTON_MATRIX_H */
