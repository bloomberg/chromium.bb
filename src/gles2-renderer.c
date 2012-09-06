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

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "compositor.h"

static const char *
egl_error_string(EGLint code)
{
#define MYERRCODE(x) case x: return #x;
	switch (code) {
	MYERRCODE(EGL_SUCCESS)
	MYERRCODE(EGL_NOT_INITIALIZED)
	MYERRCODE(EGL_BAD_ACCESS)
	MYERRCODE(EGL_BAD_ALLOC)
	MYERRCODE(EGL_BAD_ATTRIBUTE)
	MYERRCODE(EGL_BAD_CONTEXT)
	MYERRCODE(EGL_BAD_CONFIG)
	MYERRCODE(EGL_BAD_CURRENT_SURFACE)
	MYERRCODE(EGL_BAD_DISPLAY)
	MYERRCODE(EGL_BAD_SURFACE)
	MYERRCODE(EGL_BAD_MATCH)
	MYERRCODE(EGL_BAD_PARAMETER)
	MYERRCODE(EGL_BAD_NATIVE_PIXMAP)
	MYERRCODE(EGL_BAD_NATIVE_WINDOW)
	MYERRCODE(EGL_CONTEXT_LOST)
	default:
		return "unknown";
	}
#undef MYERRCODE
}

static void
print_egl_error_state(void)
{
	EGLint code;

	code = eglGetError();
	weston_log("EGL error state: %s (0x%04lx)\n",
		egl_error_string(code), (long)code);
}

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) > (b)) ? (b) : (a))
#define clip(x, a, b)  min(max(x, a), b)
#define sign(x)   ((x) >= 0)

static int
calculate_edges(struct weston_surface *es, pixman_box32_t *rect,
		pixman_box32_t *surf_rect, GLfloat *ex, GLfloat *ey)
{
	int i, n = 0;
	GLfloat min_x, max_x, min_y, max_y;
	GLfloat x[4] = {
			surf_rect->x1, surf_rect->x2, surf_rect->x2, surf_rect->x1,
	};
	GLfloat y[4] = {
			surf_rect->y1, surf_rect->y1, surf_rect->y2, surf_rect->y2,
	};
	GLfloat cx1 = rect->x1;
	GLfloat cx2 = rect->x2;
	GLfloat cy1 = rect->y1;
	GLfloat cy2 = rect->y2;

	GLfloat dist_squared(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
	{
		GLfloat dx = (x1 - x2);
		GLfloat dy = (y1 - y2);
		return dx * dx + dy * dy;
	}

	void append_vertex(GLfloat x, GLfloat y)
	{
		/* don't emit duplicate vertices: */
		if ((n > 0) && (ex[n-1] == x) && (ey[n-1] == y))
			return;
		ex[n] = x;
		ey[n] = y;
		n++;
	}

	/* transform surface to screen space: */
	for (i = 0; i < 4; i++)
		weston_surface_to_global_float(es, x[i], y[i], &x[i], &y[i]);

	/* find bounding box: */
	min_x = max_x = x[0];
	min_y = max_y = y[0];

	for (i = 1; i < 4; i++) {
		min_x = min(min_x, x[i]);
		max_x = max(max_x, x[i]);
		min_y = min(min_y, y[i]);
		max_y = max(max_y, y[i]);
	}

	/* First, simple bounding box check to discard early transformed
	 * surface rects that do not intersect with the clip region:
	 */
	if ((min_x > cx2) || (max_x < cx1) ||
			(min_y > cy2) || (max_y < cy1))
		return 0;

	/* Simple case, bounding box edges are parallel to surface edges,
	 * there will be only four edges.  We just need to clip the surface
	 * vertices to the clip rect bounds:
	 */
	if (!es->transform.enabled) {
		for (i = 0; i < 4; i++) {
			ex[n] = clip(x[i], cx1, cx2);
			ey[n] = clip(y[i], cy1, cy2);
			n++;
		}
		return 4;
	}

	/* Hard case, transformation applied.  We need to find the vertices
	 * of the shape that is the intersection of the clip rect and
	 * transformed surface.  This can be anything from 3 to 8 sides.
	 *
	 * Observation: all the resulting vertices will be the intersection
	 * points of the transformed surface and the clip rect, plus the
	 * vertices of the clip rect which are enclosed by the transformed
	 * surface and the vertices of the transformed surface which are
	 * enclosed by the clip rect.
	 *
	 * Observation: there will be zero, one, or two resulting vertices
	 * for each edge of the src rect.
	 *
	 * Loop over four edges of the transformed rect:
	 */
	for (i = 0; i < 4; i++) {
		GLfloat x1, y1, x2, y2;
		int last_n = n;

		x1 = x[i];
		y1 = y[i];

		/* if this vertex is contained in the clip rect, use it as-is: */
		if ((cx1 <= x1) && (x1 <= cx2) &&
				(cy1 <= y1) && (y1 <= cy2))
			append_vertex(x1, y1);

		/* for remaining, we consider the point as part of a line: */
		x2 = x[(i+1) % 4];
		y2 = y[(i+1) % 4];

		if (x1 == x2) {
			append_vertex(clip(x1, cx1, cx2), clip(y1, cy1, cy2));
			append_vertex(clip(x2, cx1, cx2), clip(y2, cy1, cy2));
		} else if (y1 == y2) {
			append_vertex(clip(x1, cx1, cx2), clip(y1, cy1, cy2));
			append_vertex(clip(x2, cx1, cx2), clip(y2, cy1, cy2));
		} else {
			GLfloat m, c, p;
			GLfloat tx[2], ty[2];
			int tn = 0;

			int intersect_horiz(GLfloat y, GLfloat *p)
			{
				GLfloat x;

				/* if y does not lie between y1 and y2, no
				 * intersection possible
				 */
				if (sign(y-y1) == sign(y-y2))
					return 0;

				x = (y - c) / m;

				/* if x does not lie between cx1 and cx2, no
				 * intersection:
				 */
				if (sign(x-cx1) == sign(x-cx2))
					return 0;

				*p = x;
				return 1;
			}

			int intersect_vert(GLfloat x, GLfloat *p)
			{
				GLfloat y;

				if (sign(x-x1) == sign(x-x2))
					return 0;

				y = m * x + c;

				if (sign(y-cy1) == sign(y-cy2))
					return 0;

				*p = y;
				return 1;
			}

			/* y = mx + c */
			m = (y2 - y1) / (x2 - x1);
			c = y1 - m * x1;

			/* check for up to two intersections with the four edges
			 * of the clip rect.  Note that we don't know the orientation
			 * of the transformed surface wrt. the clip rect.  So if when
			 * there are two intersection points, we need to put the one
			 * closest to x1,y1 first:
			 */

			/* check top clip rect edge: */
			if (intersect_horiz(cy1, &p)) {
				ty[tn] = cy1;
				tx[tn] = p;
				tn++;
			}

			/* check right clip rect edge: */
			if (intersect_vert(cx2, &p)) {
				ty[tn] = p;
				tx[tn] = cx2;
				tn++;
				if (tn == 2)
					goto edge_check_done;
			}

			/* check bottom clip rect edge: */
			if (intersect_horiz(cy2, &p)) {
				ty[tn] = cy2;
				tx[tn] = p;
				tn++;
				if (tn == 2)
					goto edge_check_done;
			}

			/* check left clip rect edge: */
			if (intersect_vert(cx1, &p)) {
				ty[tn] = p;
				tx[tn] = cx1;
				tn++;
			}

edge_check_done:
			if (tn == 1) {
				append_vertex(tx[0], ty[0]);
			} else if (tn == 2) {
				if (dist_squared(x1, y1, tx[0], ty[0]) <
						dist_squared(x1, y1, tx[1], ty[1])) {
					append_vertex(tx[0], ty[0]);
					append_vertex(tx[1], ty[1]);
				} else {
					append_vertex(tx[1], ty[1]);
					append_vertex(tx[0], ty[0]);
				}
			}

			if (n == last_n) {
				GLfloat best_x=0, best_y=0;
				uint32_t d, best_d = (unsigned int)-1; /* distance squared */
				uint32_t max_d = dist_squared(x2, y2,
						x[(i+2) % 4], y[(i+2) % 4]);

				/* if there are no vertices on this line, it could be that
				 * there is a vertex of the clip rect that is enclosed by
				 * the transformed surface.  Find the vertex of the clip
				 * rect that is reached by the shortest line perpendicular
				 * to the current edge, if any.
				 *
				 * slope of perpendicular is 1/m, so
				 *
				 *   cy = -cx/m + c2
				 *   c2 = cy + cx/m
				 *
				 */

				int perp_intersect(GLfloat cx, GLfloat cy, uint32_t *d)
				{
					GLfloat c2 = cy + cx/m;
					GLfloat x = (c2 - c) / (m + 1/m);

					/* if the x position of the intersection of the
					 * perpendicular with the transformed edge does
					 * not lie within the bounds of the edge, then
					 * no intersection:
					 */
					if (sign(x-x1) == sign(x-x2))
						return 0;

					*d = dist_squared(cx, cy, x, (m * x) + c);

					/* if intersection distance is further away than
					 * opposite edge of surface region, it is invalid:
					 */
					if (*d > max_d)
						return 0;

					return 1;
				}

				if (perp_intersect(cx1, cy1, &d)) {
					best_x = cx1;
					best_y = cy1;
					best_d = d;
				}

				if (perp_intersect(cx1, cy2, &d) && (d < best_d)) {
					best_x = cx1;
					best_y = cy2;
					best_d = d;
				}

				if (perp_intersect(cx2, cy2, &d) && (d < best_d)) {
					best_x = cx2;
					best_y = cy2;
					best_d = d;
				}

				if (perp_intersect(cx2, cy1, &d) && (d < best_d)) {
					best_x = cx2;
					best_y = cy1;
					best_d = d;
				}

				if (best_d != (unsigned int)-1)  // XXX can this happen?
					append_vertex(best_x, best_y);
			}
		}

	}

	return n;
}

static int
texture_region(struct weston_surface *es, pixman_region32_t *region,
		pixman_region32_t *surf_region)
{
	struct weston_compositor *ec = es->compositor;
	GLfloat *v, inv_width, inv_height;
	unsigned int *vtxcnt, nvtx = 0;
	pixman_box32_t *rects, *surf_rects;
	int i, j, k, nrects, nsurf;

	rects = pixman_region32_rectangles(region, &nrects);
	surf_rects = pixman_region32_rectangles(surf_region, &nsurf);

	/* worst case we can have 8 vertices per rect (ie. clipped into
	 * an octagon):
	 */
	v = wl_array_add(&ec->vertices, nrects * nsurf * 8 * 4 * sizeof *v);
	vtxcnt = wl_array_add(&ec->vtxcnt, nrects * nsurf * sizeof *vtxcnt);

	inv_width = 1.0 / es->pitch;
	inv_height = 1.0 / es->geometry.height;

	for (i = 0; i < nrects; i++) {
		pixman_box32_t *rect = &rects[i];
		for (j = 0; j < nsurf; j++) {
			pixman_box32_t *surf_rect = &surf_rects[j];
			GLfloat sx, sy;
			GLfloat ex[8], ey[8];          /* edge points in screen space */
			int n;

			/* The transformed surface, after clipping to the clip region,
			 * can have as many as eight sides, emitted as a triangle-fan.
			 * The first vertex in the triangle fan can be chosen arbitrarily,
			 * since the area is guaranteed to be convex.
			 *
			 * If a corner of the transformed surface falls outside of the
			 * clip region, instead of emitting one vertex for the corner
			 * of the surface, up to two are emitted for two corresponding
			 * intersection point(s) between the surface and the clip region.
			 *
			 * To do this, we first calculate the (up to eight) points that
			 * form the intersection of the clip rect and the transformed
			 * surface.
			 */
			n = calculate_edges(es, rect, surf_rect, ex, ey);
			if (n < 3)
				continue;

			/* emit edge points: */
			for (k = 0; k < n; k++) {
				weston_surface_from_global_float(es, ex[k], ey[k], &sx, &sy);
				/* position: */
				*(v++) = ex[k];
				*(v++) = ey[k];
				/* texcoord: */
				*(v++) = sx * inv_width;
				*(v++) = sy * inv_height;
			}

			vtxcnt[nvtx++] = n;
		}
	}

	return nvtx;
}

static void
triangle_fan_debug(struct weston_surface *surface, int first, int count)
{
	struct weston_compositor *compositor = surface->compositor;
	int i;
	GLushort *buffer;
	GLushort *index;
	int nelems;
	static int color_idx = 0;
	static const GLfloat color[][4] = {
			{ 1.0, 0.0, 0.0, 1.0 },
			{ 0.0, 1.0, 0.0, 1.0 },
			{ 0.0, 0.0, 1.0, 1.0 },
			{ 1.0, 1.0, 1.0, 1.0 },
	};

	nelems = (count - 1 + count - 2) * 2;

	buffer = malloc(sizeof(GLushort) * nelems);
	index = buffer;

	for (i = 1; i < count; i++) {
		*index++ = first;
		*index++ = first + i;
	}

	for (i = 2; i < count; i++) {
		*index++ = first + i - 1;
		*index++ = first + i;
	}

	glUseProgram(compositor->solid_shader.program);
	glUniform4fv(compositor->solid_shader.color_uniform, 1,
			color[color_idx++ % ARRAY_LENGTH(color)]);
	glDrawElements(GL_LINES, nelems, GL_UNSIGNED_SHORT, buffer);
	glUseProgram(compositor->current_shader->program);
	free(buffer);
}

static void
repaint_region(struct weston_surface *es, pixman_region32_t *region,
		pixman_region32_t *surf_region)
{
	struct weston_compositor *ec = es->compositor;
	GLfloat *v;
	unsigned int *vtxcnt;
	int i, first, nfans;

	/* The final region to be painted is the intersection of
	 * 'region' and 'surf_region'. However, 'region' is in the global
	 * coordinates, and 'surf_region' is in the surface-local
	 * coordinates. texture_region() will iterate over all pairs of
	 * rectangles from both regions, compute the intersection
	 * polygon for each pair, and store it as a triangle fan if
	 * it has a non-zero area (at least 3 vertices, actually).
	 */
	nfans = texture_region(es, region, surf_region);

	v = ec->vertices.data;
	vtxcnt = ec->vtxcnt.data;

	/* position: */
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof *v, &v[0]);
	glEnableVertexAttribArray(0);

	/* texcoord: */
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof *v, &v[2]);
	glEnableVertexAttribArray(1);

	for (i = 0, first = 0; i < nfans; i++) {
		glDrawArrays(GL_TRIANGLE_FAN, first, vtxcnt[i]);
		if (ec->fan_debug)
			triangle_fan_debug(es, first, vtxcnt[i]);
		first += vtxcnt[i];
	}

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);

	ec->vertices.size = 0;
	ec->vtxcnt.size = 0;
}

static void
weston_compositor_use_shader(struct weston_compositor *compositor,
			     struct weston_shader *shader)
{
	if (compositor->current_shader == shader)
		return;

	glUseProgram(shader->program);
	compositor->current_shader = shader;
}

static void
weston_shader_uniforms(struct weston_shader *shader,
		       struct weston_surface *surface,
		       struct weston_output *output)
{
	int i;

	glUniformMatrix4fv(shader->proj_uniform,
			   1, GL_FALSE, output->matrix.d);
	glUniform4fv(shader->color_uniform, 1, surface->color);
	glUniform1f(shader->alpha_uniform, surface->alpha);

	for (i = 0; i < surface->num_textures; i++)
		glUniform1i(shader->tex_uniforms[i], i);
}

static void
draw_surface(struct weston_surface *es, struct weston_output *output,
	     pixman_region32_t *damage) /* in global coordinates */
{
	struct weston_compositor *ec = es->compositor;
	/* repaint bounding region in global coordinates: */
	pixman_region32_t repaint;
	/* non-opaque region in surface coordinates: */
	pixman_region32_t surface_blend;
	GLint filter;
	int i;

	pixman_region32_init(&repaint);
	pixman_region32_intersect(&repaint,
				  &es->transform.boundingbox, damage);
	pixman_region32_subtract(&repaint, &repaint, &es->clip);

	if (!pixman_region32_not_empty(&repaint))
		goto out;

	pixman_region32_subtract(&ec->primary_plane.damage,
				 &ec->primary_plane.damage, &repaint);

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	if (ec->fan_debug) {
		weston_compositor_use_shader(ec, &ec->solid_shader);
		weston_shader_uniforms(&ec->solid_shader, es, output);
	}

	weston_compositor_use_shader(ec, es->shader);
	weston_shader_uniforms(es->shader, es, output);

	if (es->transform.enabled || output->zoom.active)
		filter = GL_LINEAR;
	else
		filter = GL_NEAREST;

	for (i = 0; i < es->num_textures; i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(es->target, es->textures[i]);
		glTexParameteri(es->target, GL_TEXTURE_MIN_FILTER, filter);
		glTexParameteri(es->target, GL_TEXTURE_MAG_FILTER, filter);
	}

	/* blended region is whole surface minus opaque region: */
	pixman_region32_init_rect(&surface_blend, 0, 0,
				  es->geometry.width, es->geometry.height);
	pixman_region32_subtract(&surface_blend, &surface_blend, &es->opaque);

	if (pixman_region32_not_empty(&es->opaque)) {
		if (es->shader == &ec->texture_shader_rgba) {
			/* Special case for RGBA textures with possibly
			 * bad data in alpha channel: use the shader
			 * that forces texture alpha = 1.0.
			 * Xwayland surfaces need this.
			 */
			weston_compositor_use_shader(ec, &ec->texture_shader_rgbx);
			weston_shader_uniforms(&ec->texture_shader_rgbx, es, output);
		}

		if (es->alpha < 1.0)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);

		repaint_region(es, &repaint, &es->opaque);
	}

	if (pixman_region32_not_empty(&surface_blend)) {
		weston_compositor_use_shader(ec, es->shader);
		glEnable(GL_BLEND);
		repaint_region(es, &repaint, &surface_blend);
	}

	pixman_region32_fini(&surface_blend);

out:
	pixman_region32_fini(&repaint);
}

static void
repaint_surfaces(struct weston_output *output, pixman_region32_t *damage)
{
	struct weston_compositor *compositor = output->compositor;
	struct weston_surface *surface;

	wl_list_for_each_reverse(surface, &compositor->surface_list, link)
		if (surface->plane == &compositor->primary_plane)
			draw_surface(surface, output, damage);
}

WL_EXPORT void
gles2_renderer_repaint_output(struct weston_output *output,
			      pixman_region32_t *output_damage)
{
	struct weston_compositor *compositor = output->compositor;
	EGLBoolean ret;
	static int errored;
	int32_t width, height;

	width = output->current->width +
		output->border.left + output->border.right;
	height = output->current->height +
		output->border.top + output->border.bottom;

	glViewport(0, 0, width, height);

	ret = eglMakeCurrent(compositor->egl_display, output->egl_surface,
			     output->egl_surface, compositor->egl_context);
	if (ret == EGL_FALSE) {
		if (errored)
			return;
		errored = 1;
		weston_log("Failed to make EGL context current.\n");
		print_egl_error_state();
		return;
	}

	/* if debugging, redraw everything outside the damage to clean up
	 * debug lines from the previous draw on this buffer:
	 */
	if (compositor->fan_debug) {
		pixman_region32_t undamaged;
		pixman_region32_init(&undamaged);
		pixman_region32_subtract(&undamaged, &output->region,
					 output_damage);
		compositor->fan_debug = 0;
		repaint_surfaces(output, &undamaged);
		compositor->fan_debug = 1;
		pixman_region32_fini(&undamaged);
	}

	repaint_surfaces(output, output_damage);

	wl_signal_emit(&output->frame_signal, output);

	ret = eglSwapBuffers(compositor->egl_display, output->egl_surface);
	if (ret == EGL_FALSE && !errored) {
		errored = 1;
		weston_log("Failed in eglSwapBuffers.\n");
		print_egl_error_state();
	}

}

WL_EXPORT void
gles2_renderer_flush_damage(struct weston_surface *surface)
{
#ifdef GL_UNPACK_ROW_LENGTH
	pixman_box32_t *rectangles;
	void *data;
	int i, n;
#endif

	glBindTexture(GL_TEXTURE_2D, surface->textures[0]);

	if (!surface->compositor->has_unpack_subimage) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT,
			     surface->pitch, surface->buffer->height, 0,
			     GL_BGRA_EXT, GL_UNSIGNED_BYTE,
			     wl_shm_buffer_get_data(surface->buffer));

		return;
	}

#ifdef GL_UNPACK_ROW_LENGTH
	/* Mesa does not define GL_EXT_unpack_subimage */
	glPixelStorei(GL_UNPACK_ROW_LENGTH, surface->pitch);
	data = wl_shm_buffer_get_data(surface->buffer);
	rectangles = pixman_region32_rectangles(&surface->damage, &n);
	for (i = 0; i < n; i++) {
		glPixelStorei(GL_UNPACK_SKIP_PIXELS, rectangles[i].x1);
		glPixelStorei(GL_UNPACK_SKIP_ROWS, rectangles[i].y1);
		glTexSubImage2D(GL_TEXTURE_2D, 0,
				rectangles[i].x1, rectangles[i].y1,
				rectangles[i].x2 - rectangles[i].x1,
				rectangles[i].y2 - rectangles[i].y1,
				GL_BGRA_EXT, GL_UNSIGNED_BYTE, data);
	}
#endif
}

static const char vertex_shader[] =
	"uniform mat4 proj;\n"
	"attribute vec2 position;\n"
	"attribute vec2 texcoord;\n"
	"varying vec2 v_texcoord;\n"
	"void main()\n"
	"{\n"
	"   gl_Position = proj * vec4(position, 0.0, 1.0);\n"
	"   v_texcoord = texcoord;\n"
	"}\n";

/* Declare common fragment shader uniforms */
#define FRAGMENT_CONVERT_YUV						\
	"  y *= alpha;\n"						\
	"  u *= alpha;\n"						\
	"  v *= alpha;\n"						\
	"  gl_FragColor.r = y + 1.59602678 * v;\n"			\
	"  gl_FragColor.g = y - 0.39176229 * u - 0.81296764 * v;\n"	\
	"  gl_FragColor.b = y + 2.01723214 * u;\n"			\
	"  gl_FragColor.a = alpha;\n"

static const char texture_fragment_shader_rgba[] =
	"precision mediump float;\n"
	"varying vec2 v_texcoord;\n"
	"uniform sampler2D tex;\n"
	"uniform float alpha;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = alpha * texture2D(tex, v_texcoord)\n;"
	"}\n";

static const char texture_fragment_shader_rgbx[] =
	"precision mediump float;\n"
	"varying vec2 v_texcoord;\n"
	"uniform sampler2D tex;\n"
	"uniform float alpha;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor.rgb = alpha * texture2D(tex, v_texcoord).rgb\n;"
	"   gl_FragColor.a = alpha;\n"
	"}\n";

static const char texture_fragment_shader_egl_external[] =
	"#extension GL_OES_EGL_image_external : require\n"
	"precision mediump float;\n"
	"varying vec2 v_texcoord;\n"
	"uniform samplerExternalOES tex;\n"
	"uniform float alpha;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = alpha * texture2D(tex, v_texcoord)\n;"
	"}\n";

static const char texture_fragment_shader_y_uv[] =
	"precision mediump float;\n"
	"uniform sampler2D tex;\n"
	"uniform sampler2D tex1;\n"
	"varying vec2 v_texcoord;\n"
	"uniform float alpha;\n"
	"void main() {\n"
	"  float y = 1.16438356 * (texture2D(tex, v_texcoord).x - 0.0625);\n"
	"  float u = texture2D(tex1, v_texcoord).r - 0.5;\n"
	"  float v = texture2D(tex1, v_texcoord).g - 0.5;\n"
	FRAGMENT_CONVERT_YUV
	"}\n";

static const char texture_fragment_shader_y_u_v[] =
	"precision mediump float;\n"
	"uniform sampler2D tex;\n"
	"uniform sampler2D tex1;\n"
	"uniform sampler2D tex2;\n"
	"varying vec2 v_texcoord;\n"
	"uniform float alpha;\n"
	"void main() {\n"
	"  float y = 1.16438356 * (texture2D(tex, v_texcoord).x - 0.0625);\n"
	"  float u = texture2D(tex1, v_texcoord).x - 0.5;\n"
	"  float v = texture2D(tex2, v_texcoord).x - 0.5;\n"
	FRAGMENT_CONVERT_YUV
	"}\n";

static const char texture_fragment_shader_y_xuxv[] =
	"precision mediump float;\n"
	"uniform sampler2D tex;\n"
	"uniform sampler2D tex1;\n"
	"varying vec2 v_texcoord;\n"
	"uniform float alpha;\n"
	"void main() {\n"
	"  float y = 1.16438356 * (texture2D(tex, v_texcoord).x - 0.0625);\n"
	"  float u = texture2D(tex1, v_texcoord).g - 0.5;\n"
	"  float v = texture2D(tex1, v_texcoord).a - 0.5;\n"
	FRAGMENT_CONVERT_YUV
	"}\n";

static const char solid_fragment_shader[] =
	"precision mediump float;\n"
	"uniform vec4 color;\n"
	"uniform float alpha;\n"
	"void main()\n"
	"{\n"
	"   gl_FragColor = alpha * color\n;"
	"}\n";

static int
compile_shader(GLenum type, const char *source)
{
	GLuint s;
	char msg[512];
	GLint status;

	s = glCreateShader(type);
	glShaderSource(s, 1, &source, NULL);
	glCompileShader(s);
	glGetShaderiv(s, GL_COMPILE_STATUS, &status);
	if (!status) {
		glGetShaderInfoLog(s, sizeof msg, NULL, msg);
		weston_log("shader info: %s\n", msg);
		return GL_NONE;
	}

	return s;
}

static int
weston_shader_init(struct weston_shader *shader,
		   const char *vertex_source, const char *fragment_source)
{
	char msg[512];
	GLint status;

	shader->vertex_shader =
		compile_shader(GL_VERTEX_SHADER, vertex_source);
	shader->fragment_shader =
		compile_shader(GL_FRAGMENT_SHADER, fragment_source);

	shader->program = glCreateProgram();
	glAttachShader(shader->program, shader->vertex_shader);
	glAttachShader(shader->program, shader->fragment_shader);
	glBindAttribLocation(shader->program, 0, "position");
	glBindAttribLocation(shader->program, 1, "texcoord");

	glLinkProgram(shader->program);
	glGetProgramiv(shader->program, GL_LINK_STATUS, &status);
	if (!status) {
		glGetProgramInfoLog(shader->program, sizeof msg, NULL, msg);
		weston_log("link info: %s\n", msg);
		return -1;
	}

	shader->proj_uniform = glGetUniformLocation(shader->program, "proj");
	shader->tex_uniforms[0] = glGetUniformLocation(shader->program, "tex");
	shader->tex_uniforms[1] = glGetUniformLocation(shader->program, "tex1");
	shader->tex_uniforms[2] = glGetUniformLocation(shader->program, "tex2");
	shader->alpha_uniform = glGetUniformLocation(shader->program, "alpha");
	shader->color_uniform = glGetUniformLocation(shader->program, "color");

	return 0;
}

static void
log_extensions(const char *name, const char *extensions)
{
	const char *p, *end;
	int l;
	int len;

	l = weston_log("%s:", name);
	p = extensions;
	while (*p) {
		end = strchrnul(p, ' ');
		len = end - p;
		if (l + len > 78)
			l = weston_log_continue("\n" STAMP_SPACE "%.*s",
						len, p);
		else
			l += weston_log_continue(" %.*s", len, p);
		for (p = end; isspace(*p); p++)
			;
	}
	weston_log_continue("\n");
}

static void
log_egl_gl_info(EGLDisplay egldpy)
{
	const char *str;

	str = eglQueryString(egldpy, EGL_VERSION);
	weston_log("EGL version: %s\n", str ? str : "(null)");

	str = eglQueryString(egldpy, EGL_VENDOR);
	weston_log("EGL vendor: %s\n", str ? str : "(null)");

	str = eglQueryString(egldpy, EGL_CLIENT_APIS);
	weston_log("EGL client APIs: %s\n", str ? str : "(null)");

	str = eglQueryString(egldpy, EGL_EXTENSIONS);
	log_extensions("EGL extensions", str ? str : "(null)");

	str = (char *)glGetString(GL_VERSION);
	weston_log("GL version: %s\n", str ? str : "(null)");

	str = (char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
	weston_log("GLSL version: %s\n", str ? str : "(null)");

	str = (char *)glGetString(GL_VENDOR);
	weston_log("GL vendor: %s\n", str ? str : "(null)");

	str = (char *)glGetString(GL_RENDERER);
	weston_log("GL renderer: %s\n", str ? str : "(null)");

	str = (char *)glGetString(GL_EXTENSIONS);
	log_extensions("GL extensions", str ? str : "(null)");
}

WL_EXPORT int
gles2_renderer_init(struct weston_compositor *ec)
{
	const char *extensions;
	int has_egl_image_external = 0;

	log_egl_gl_info(ec->egl_display);

	ec->image_target_texture_2d =
		(void *) eglGetProcAddress("glEGLImageTargetTexture2DOES");
	ec->image_target_renderbuffer_storage = (void *)
		eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
	ec->create_image = (void *) eglGetProcAddress("eglCreateImageKHR");
	ec->destroy_image = (void *) eglGetProcAddress("eglDestroyImageKHR");
	ec->bind_display =
		(void *) eglGetProcAddress("eglBindWaylandDisplayWL");
	ec->unbind_display =
		(void *) eglGetProcAddress("eglUnbindWaylandDisplayWL");
	ec->query_buffer =
		(void *) eglGetProcAddress("eglQueryWaylandBufferWL");

	extensions = (const char *) glGetString(GL_EXTENSIONS);
	if (!extensions) {
		weston_log("Retrieving GL extension string failed.\n");
		return -1;
	}

	if (!strstr(extensions, "GL_EXT_texture_format_BGRA8888")) {
		weston_log("GL_EXT_texture_format_BGRA8888 not available\n");
		return -1;
	}

	if (strstr(extensions, "GL_EXT_read_format_bgra"))
		ec->read_format = GL_BGRA_EXT;
	else
		ec->read_format = GL_RGBA;

	if (strstr(extensions, "GL_EXT_unpack_subimage"))
		ec->has_unpack_subimage = 1;

	if (strstr(extensions, "GL_OES_EGL_image_external"))
		has_egl_image_external = 1;

	extensions =
		(const char *) eglQueryString(ec->egl_display, EGL_EXTENSIONS);
	if (!extensions) {
		weston_log("Retrieving EGL extension string failed.\n");
		return -1;
	}

	if (strstr(extensions, "EGL_WL_bind_wayland_display"))
		ec->has_bind_display = 1;
	if (ec->has_bind_display)
		ec->bind_display(ec->egl_display, ec->wl_display);

	glActiveTexture(GL_TEXTURE0);

	if (weston_shader_init(&ec->texture_shader_rgba,
			     vertex_shader, texture_fragment_shader_rgba) < 0)
		return -1;
	if (weston_shader_init(&ec->texture_shader_rgbx,
			     vertex_shader, texture_fragment_shader_rgbx) < 0)
		return -1;
	if (has_egl_image_external &&
			weston_shader_init(&ec->texture_shader_egl_external,
				vertex_shader, texture_fragment_shader_egl_external) < 0)
		return -1;
	if (weston_shader_init(&ec->texture_shader_y_uv,
			       vertex_shader, texture_fragment_shader_y_uv) < 0)
		return -1;
	if (weston_shader_init(&ec->texture_shader_y_u_v,
			       vertex_shader, texture_fragment_shader_y_u_v) < 0)
		return -1;
	if (weston_shader_init(&ec->texture_shader_y_xuxv,
			       vertex_shader, texture_fragment_shader_y_xuxv) < 0)
		return -1;
	if (weston_shader_init(&ec->solid_shader,
			     vertex_shader, solid_fragment_shader) < 0)
		return -1;

	return 0;
}
