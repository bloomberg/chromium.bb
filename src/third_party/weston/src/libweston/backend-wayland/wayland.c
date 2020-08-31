/*
 * Copyright © 2010-2011 Benjamin Franzke
 * Copyright © 2013 Jason Ekstrand
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <linux/input.h>

#include <wayland-client.h>
#include <wayland-cursor.h>

#ifdef ENABLE_EGL
#include <wayland-egl.h>
#endif

#include <libweston/libweston.h>
#include <libweston/backend-wayland.h>
#include "renderer-gl/gl-renderer.h"
#include "weston-egl-ext.h"
#include "pixman-renderer.h"
#include "shared/helpers.h"
#include "shared/image-loader.h"
#include "shared/os-compatibility.h"
#include "shared/cairo-util.h"
#include "shared/timespec-util.h"
#include "fullscreen-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "presentation-time-server-protocol.h"
#include "linux-dmabuf.h"
#include <libweston/windowed-output-api.h>

#define WINDOW_TITLE "Weston Compositor"

struct wayland_backend {
	struct weston_backend base;
	struct weston_compositor *compositor;

	struct {
		struct wl_display *wl_display;
		struct wl_registry *registry;
		struct wl_compositor *compositor;
		struct wl_shell *shell;
		struct xdg_wm_base *xdg_wm_base;
		struct zwp_fullscreen_shell_v1 *fshell;
		struct wl_shm *shm;

		struct wl_list output_list;

		struct wl_event_source *wl_source;
		uint32_t event_mask;
	} parent;

	bool use_pixman;
	bool sprawl_across_outputs;
	bool fullscreen;

	struct theme *theme;
	cairo_device_t *frame_device;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor *cursor;

	struct wl_list input_list;
};

struct wayland_output {
	struct weston_output base;

	struct {
		bool draw_initial_frame;
		struct wl_surface *surface;

		struct wl_output *output;
		uint32_t global_id;

		struct wl_shell_surface *shell_surface;
		struct xdg_surface *xdg_surface;
		struct xdg_toplevel *xdg_toplevel;
		int configure_width, configure_height;
		bool wait_for_configure;
	} parent;

	int keyboard_count;

	char *title;
	struct frame *frame;

	struct {
		struct wl_egl_window *egl_window;
		struct {
			cairo_surface_t *top;
			cairo_surface_t *left;
			cairo_surface_t *right;
			cairo_surface_t *bottom;
		} border;
	} gl;

	struct {
		struct wl_list buffers;
		struct wl_list free_buffers;
	} shm;

	struct weston_mode mode;

	struct wl_callback *frame_cb;
};

struct wayland_parent_output {
	struct wayland_backend *backend;	/**< convenience */
	struct wayland_head *head;
	struct wl_list link;

	struct wl_output *global;
	uint32_t id;

	struct {
		char *make;
		char *model;
		int32_t width, height;
		uint32_t subpixel;
	} physical;

	int32_t x, y;
	uint32_t transform;
	uint32_t scale;

	struct wl_callback *sync_cb;	/**< wl_output < 2 done replacement */

	struct wl_list mode_list;
	struct weston_mode *preferred_mode;
	struct weston_mode *current_mode;
};

struct wayland_head {
	struct weston_head base;
	struct wayland_parent_output *parent_output;
};

struct wayland_shm_buffer {
	struct wayland_output *output;
	struct wl_list link;
	struct wl_list free_link;

	struct wl_buffer *buffer;
	void *data;
	size_t size;
	pixman_region32_t damage;		/**< in global coords */
	int frame_damaged;

	pixman_image_t *pm_image;
	cairo_surface_t *c_surface;
};

struct wayland_input {
	struct weston_seat base;
	struct wayland_backend *backend;
	struct wl_list link;

	struct {
		struct wl_seat *seat;
		struct wl_pointer *pointer;
		struct wl_keyboard *keyboard;
		struct wl_touch *touch;

		struct {
			struct wl_surface *surface;
			int32_t hx, hy;
		} cursor;
	} parent;

	struct weston_touch_device *touch_device;

	enum weston_key_state_update keyboard_state_update;
	uint32_t key_serial;
	uint32_t enter_serial;
	uint32_t touch_points;
	bool touch_active;
	bool has_focus;
	int seat_version;

	struct wayland_output *output;
	struct wayland_output *touch_focus;
	struct wayland_output *keyboard_focus;

	struct weston_pointer_axis_event vert, horiz;
};

struct gl_renderer_interface *gl_renderer;

static inline struct wayland_head *
to_wayland_head(struct weston_head *base)
{
	return container_of(base, struct wayland_head, base);
}

static inline struct wayland_output *
to_wayland_output(struct weston_output *base)
{
	return container_of(base, struct wayland_output, base);
}

static inline struct wayland_backend *
to_wayland_backend(struct weston_compositor *base)
{
	return container_of(base->backend, struct wayland_backend, base);
}

static void
wayland_shm_buffer_destroy(struct wayland_shm_buffer *buffer)
{
	cairo_surface_destroy(buffer->c_surface);
	pixman_image_unref(buffer->pm_image);

	wl_buffer_destroy(buffer->buffer);
	munmap(buffer->data, buffer->size);

	pixman_region32_fini(&buffer->damage);

	wl_list_remove(&buffer->link);
	wl_list_remove(&buffer->free_link);
	free(buffer);
}

static void
buffer_release(void *data, struct wl_buffer *buffer)
{
	struct wayland_shm_buffer *sb = data;

	if (sb->output) {
		wl_list_insert(&sb->output->shm.free_buffers, &sb->free_link);
	} else {
		wayland_shm_buffer_destroy(sb);
	}
}

static const struct wl_buffer_listener buffer_listener = {
	buffer_release
};

static struct wayland_shm_buffer *
wayland_output_get_shm_buffer(struct wayland_output *output)
{
	struct wayland_backend *b =
		to_wayland_backend(output->base.compositor);
	struct wl_shm *shm = b->parent.shm;
	struct wayland_shm_buffer *sb;

	struct wl_shm_pool *pool;
	int width, height, stride;
	int32_t fx, fy;
	int fd;
	unsigned char *data;

	if (!wl_list_empty(&output->shm.free_buffers)) {
		sb = container_of(output->shm.free_buffers.next,
				  struct wayland_shm_buffer, free_link);
		wl_list_remove(&sb->free_link);
		wl_list_init(&sb->free_link);

		return sb;
	}

	if (output->frame) {
		width = frame_width(output->frame);
		height = frame_height(output->frame);
	} else {
		width = output->base.current_mode->width;
		height = output->base.current_mode->height;
	}

	stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);

	fd = os_create_anonymous_file(height * stride);
	if (fd < 0) {
		weston_log("could not create an anonymous file buffer: %s\n",
			   strerror(errno));
		return NULL;
	}

	data = mmap(NULL, height * stride, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		weston_log("could not mmap %d memory for data: %s\n", height * stride,
			   strerror(errno));
		close(fd);
		return NULL;
	}

	sb = zalloc(sizeof *sb);
	if (sb == NULL) {
		weston_log("could not zalloc %zu memory for sb: %s\n", sizeof *sb,
			   strerror(errno));
		close(fd);
		munmap(data, height * stride);
		return NULL;
	}

	sb->output = output;
	wl_list_init(&sb->free_link);
	wl_list_insert(&output->shm.buffers, &sb->link);

	pixman_region32_init(&sb->damage);
	pixman_region32_copy(&sb->damage, &output->base.region);
	sb->frame_damaged = 1;

	sb->data = data;
	sb->size = height * stride;

	pool = wl_shm_create_pool(shm, fd, sb->size);

	sb->buffer = wl_shm_pool_create_buffer(pool, 0,
					       width, height,
					       stride,
					       WL_SHM_FORMAT_ARGB8888);
	wl_buffer_add_listener(sb->buffer, &buffer_listener, sb);
	wl_shm_pool_destroy(pool);
	close(fd);

	memset(data, 0, sb->size);

	sb->c_surface =
		cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32,
						    width, height, stride);

	fx = 0;
	fy = 0;
	if (output->frame)
		frame_interior(output->frame, &fx, &fy, 0, 0);
	sb->pm_image =
		pixman_image_create_bits(PIXMAN_a8r8g8b8, width, height,
					 (uint32_t *)(data + fy * stride) + fx,
					 stride);

	return sb;
}

static void
frame_done(void *data, struct wl_callback *callback, uint32_t time)
{
	struct wayland_output *output = data;
	struct timespec ts;

	assert(callback == output->frame_cb);
	wl_callback_destroy(callback);
	output->frame_cb = NULL;

	/* XXX: use the presentation extension for proper timings */

	/*
	 * This is the fallback case, where Presentation extension is not
	 * available from the parent compositor. We do not know the base for
	 * 'time', so we cannot feed it to finish_frame(). Do the only thing
	 * we can, and pretend finish_frame time is when we process this
	 * event.
	 */
	weston_compositor_read_presentation_clock(output->base.compositor, &ts);
	weston_output_finish_frame(&output->base, &ts, 0);
}

static const struct wl_callback_listener frame_listener = {
	frame_done
};

static void
draw_initial_frame(struct wayland_output *output)
{
	struct wayland_shm_buffer *sb;

	sb = wayland_output_get_shm_buffer(output);

	/* If we are rendering with GL, then orphan it so that it gets
	 * destroyed immediately */
	if (output->gl.egl_window)
		sb->output = NULL;

	wl_surface_attach(output->parent.surface, sb->buffer, 0, 0);
	wl_surface_damage(output->parent.surface, 0, 0,
			  output->base.current_mode->width,
			  output->base.current_mode->height);
}

#ifdef ENABLE_EGL
static void
wayland_output_update_gl_border(struct wayland_output *output)
{
	int32_t ix, iy, iwidth, iheight, fwidth, fheight;
	cairo_t *cr;

	if (!output->frame)
		return;
	if (!(frame_status(output->frame) & FRAME_STATUS_REPAINT))
		return;

	fwidth = frame_width(output->frame);
	fheight = frame_height(output->frame);
	frame_interior(output->frame, &ix, &iy, &iwidth, &iheight);

	if (!output->gl.border.top)
		output->gl.border.top =
			cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						   fwidth, iy);
	cr = cairo_create(output->gl.border.top);
	frame_repaint(output->frame, cr);
	cairo_destroy(cr);
	gl_renderer->output_set_border(&output->base, GL_RENDERER_BORDER_TOP,
				       fwidth, iy,
				       cairo_image_surface_get_stride(output->gl.border.top) / 4,
				       cairo_image_surface_get_data(output->gl.border.top));


	if (!output->gl.border.left)
		output->gl.border.left =
			cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						   ix, 1);
	cr = cairo_create(output->gl.border.left);
	cairo_translate(cr, 0, -iy);
	frame_repaint(output->frame, cr);
	cairo_destroy(cr);
	gl_renderer->output_set_border(&output->base, GL_RENDERER_BORDER_LEFT,
				       ix, 1,
				       cairo_image_surface_get_stride(output->gl.border.left) / 4,
				       cairo_image_surface_get_data(output->gl.border.left));


	if (!output->gl.border.right)
		output->gl.border.right =
			cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						   fwidth - (ix + iwidth), 1);
	cr = cairo_create(output->gl.border.right);
	cairo_translate(cr, -(iwidth + ix), -iy);
	frame_repaint(output->frame, cr);
	cairo_destroy(cr);
	gl_renderer->output_set_border(&output->base, GL_RENDERER_BORDER_RIGHT,
				       fwidth - (ix + iwidth), 1,
				       cairo_image_surface_get_stride(output->gl.border.right) / 4,
				       cairo_image_surface_get_data(output->gl.border.right));


	if (!output->gl.border.bottom)
		output->gl.border.bottom =
			cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
						   fwidth, fheight - (iy + iheight));
	cr = cairo_create(output->gl.border.bottom);
	cairo_translate(cr, 0, -(iy + iheight));
	frame_repaint(output->frame, cr);
	cairo_destroy(cr);
	gl_renderer->output_set_border(&output->base, GL_RENDERER_BORDER_BOTTOM,
				       fwidth, fheight - (iy + iheight),
				       cairo_image_surface_get_stride(output->gl.border.bottom) / 4,
				       cairo_image_surface_get_data(output->gl.border.bottom));
}
#endif

static int
wayland_output_start_repaint_loop(struct weston_output *output_base)
{
	struct wayland_output *output = to_wayland_output(output_base);
	struct wayland_backend *wb =
		to_wayland_backend(output->base.compositor);

	/* If this is the initial frame, we need to attach a buffer so that
	 * the compositor can map the surface and include it in its render
	 * loop. If the surface doesn't end up in the render loop, the frame
	 * callback won't be invoked. The buffer is transparent and of the
	 * same size as the future real output buffer. */
	if (output->parent.draw_initial_frame) {
		output->parent.draw_initial_frame = false;

		draw_initial_frame(output);
	}

	output->frame_cb = wl_surface_frame(output->parent.surface);
	wl_callback_add_listener(output->frame_cb, &frame_listener, output);
	wl_surface_commit(output->parent.surface);
	wl_display_flush(wb->parent.wl_display);

	return 0;
}

#ifdef ENABLE_EGL
static int
wayland_output_repaint_gl(struct weston_output *output_base,
			  pixman_region32_t *damage,
			  void *repaint_data)
{
	struct wayland_output *output = to_wayland_output(output_base);
	struct weston_compositor *ec = output->base.compositor;

	output->frame_cb = wl_surface_frame(output->parent.surface);
	wl_callback_add_listener(output->frame_cb, &frame_listener, output);

	wayland_output_update_gl_border(output);

	ec->renderer->repaint_output(&output->base, damage);

	pixman_region32_subtract(&ec->primary_plane.damage,
				 &ec->primary_plane.damage, damage);
	return 0;
}
#endif

static void
wayland_output_update_shm_border(struct wayland_shm_buffer *buffer)
{
	int32_t ix, iy, iwidth, iheight, fwidth, fheight;
	cairo_t *cr;

	if (!buffer->output->frame || !buffer->frame_damaged)
		return;

	cr = cairo_create(buffer->c_surface);

	frame_interior(buffer->output->frame, &ix, &iy, &iwidth, &iheight);
	fwidth = frame_width(buffer->output->frame);
	fheight = frame_height(buffer->output->frame);

	/* Set the clip so we don't unnecisaraly damage the surface */
	cairo_move_to(cr, ix, iy);
	cairo_rel_line_to(cr, iwidth, 0);
	cairo_rel_line_to(cr, 0, iheight);
	cairo_rel_line_to(cr, -iwidth, 0);
	cairo_line_to(cr, ix, iy);
	cairo_line_to(cr, 0, iy);
	cairo_line_to(cr, 0, fheight);
	cairo_line_to(cr, fwidth, fheight);
	cairo_line_to(cr, fwidth, 0);
	cairo_line_to(cr, 0, 0);
	cairo_line_to(cr, 0, iy);
	cairo_close_path(cr);
	cairo_clip(cr);

	/* Draw using a pattern so that the final result gets clipped */
	cairo_push_group(cr);
	frame_repaint(buffer->output->frame, cr);
	cairo_pop_group_to_source(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);

	cairo_destroy(cr);
}

static void
wayland_shm_buffer_attach(struct wayland_shm_buffer *sb)
{
	pixman_region32_t damage;
	pixman_box32_t *rects;
	int32_t ix, iy, iwidth, iheight, fwidth, fheight;
	int i, n;

	pixman_region32_init(&damage);
	pixman_region32_copy(&damage, &sb->damage);
	pixman_region32_translate(&damage, -sb->output->base.x,
				  -sb->output->base.y);

	weston_transformed_region(sb->output->base.width,
				  sb->output->base.height,
				  sb->output->base.transform,
				  sb->output->base.current_scale,
				  &damage, &damage);

	if (sb->output->frame) {
		frame_interior(sb->output->frame, &ix, &iy, &iwidth, &iheight);
		fwidth = frame_width(sb->output->frame);
		fheight = frame_height(sb->output->frame);

		pixman_region32_translate(&damage, ix, iy);

		if (sb->frame_damaged) {
			pixman_region32_union_rect(&damage, &damage,
						   0, 0, fwidth, iy);
			pixman_region32_union_rect(&damage, &damage,
						   0, iy, ix, iheight);
			pixman_region32_union_rect(&damage, &damage,
						   ix + iwidth, iy,
						   fwidth - (ix + iwidth), iheight);
			pixman_region32_union_rect(&damage, &damage,
						   0, iy + iheight,
						   fwidth, fheight - (iy + iheight));
		}
	}

	rects = pixman_region32_rectangles(&damage, &n);
	wl_surface_attach(sb->output->parent.surface, sb->buffer, 0, 0);
	for (i = 0; i < n; ++i)
		wl_surface_damage(sb->output->parent.surface, rects[i].x1,
				  rects[i].y1, rects[i].x2 - rects[i].x1,
				  rects[i].y2 - rects[i].y1);

	if (sb->output->frame)
		pixman_region32_fini(&damage);
}

static int
wayland_output_repaint_pixman(struct weston_output *output_base,
			      pixman_region32_t *damage,
			      void *repaint_data)
{
	struct wayland_output *output = to_wayland_output(output_base);
	struct wayland_backend *b =
		to_wayland_backend(output->base.compositor);
	struct wayland_shm_buffer *sb;

	if (output->frame) {
		if (frame_status(output->frame) & FRAME_STATUS_REPAINT)
			wl_list_for_each(sb, &output->shm.buffers, link)
				sb->frame_damaged = 1;
	}

	wl_list_for_each(sb, &output->shm.buffers, link)
		pixman_region32_union(&sb->damage, &sb->damage, damage);

	sb = wayland_output_get_shm_buffer(output);

	wayland_output_update_shm_border(sb);
	pixman_renderer_output_set_buffer(output_base, sb->pm_image);
	b->compositor->renderer->repaint_output(output_base, &sb->damage);

	wayland_shm_buffer_attach(sb);

	output->frame_cb = wl_surface_frame(output->parent.surface);
	wl_callback_add_listener(output->frame_cb, &frame_listener, output);
	wl_surface_commit(output->parent.surface);
	wl_display_flush(b->parent.wl_display);

	pixman_region32_fini(&sb->damage);
	pixman_region32_init(&sb->damage);
	sb->frame_damaged = 0;

	pixman_region32_subtract(&b->compositor->primary_plane.damage,
				 &b->compositor->primary_plane.damage, damage);
	return 0;
}

static void
wayland_backend_destroy_output_surface(struct wayland_output *output)
{
	assert(output->parent.surface);

	if (output->parent.xdg_toplevel) {
		xdg_toplevel_destroy(output->parent.xdg_toplevel);
		output->parent.xdg_toplevel = NULL;
	}

	if (output->parent.xdg_surface) {
		xdg_surface_destroy(output->parent.xdg_surface);
		output->parent.xdg_surface = NULL;
	}

	if (output->parent.shell_surface) {
		wl_shell_surface_destroy(output->parent.shell_surface);
		output->parent.shell_surface = NULL;
	}

	wl_surface_destroy(output->parent.surface);
	output->parent.surface = NULL;
}

static void
wayland_output_destroy_shm_buffers(struct wayland_output *output)
{
	struct wayland_shm_buffer *buffer, *next;

	/* Throw away any remaining SHM buffers */
	wl_list_for_each_safe(buffer, next, &output->shm.free_buffers, free_link)
		wayland_shm_buffer_destroy(buffer);
	/* These will get thrown away when they get released */
	wl_list_for_each(buffer, &output->shm.buffers, link)
		buffer->output = NULL;
}

static int
wayland_output_disable(struct weston_output *base)
{
	struct wayland_output *output = to_wayland_output(base);
	struct wayland_backend *b = to_wayland_backend(base->compositor);

	if (!output->base.enabled)
		return 0;

	if (b->use_pixman) {
		pixman_renderer_output_destroy(&output->base);
#ifdef ENABLE_EGL
	} else {
		gl_renderer->output_destroy(&output->base);
		wl_egl_window_destroy(output->gl.egl_window);
#endif
	}

	wayland_output_destroy_shm_buffers(output);

	wayland_backend_destroy_output_surface(output);

	if (output->frame)
		frame_destroy(output->frame);

	cairo_surface_destroy(output->gl.border.top);
	cairo_surface_destroy(output->gl.border.left);
	cairo_surface_destroy(output->gl.border.right);
	cairo_surface_destroy(output->gl.border.bottom);

	return 0;
}

static void
wayland_output_destroy(struct weston_output *base)
{
	struct wayland_output *output = to_wayland_output(base);

	wayland_output_disable(&output->base);

	weston_output_release(&output->base);

	if (output->frame_cb)
		wl_callback_destroy(output->frame_cb);

	free(output->title);
	free(output);
}

static const struct wl_shell_surface_listener shell_surface_listener;

#ifdef ENABLE_EGL
static int
wayland_output_init_gl_renderer(struct wayland_output *output)
{
	int32_t fwidth = 0, fheight = 0;

	if (output->frame) {
		fwidth = frame_width(output->frame);
		fheight = frame_height(output->frame);
	} else {
		fwidth = output->base.current_mode->width;
		fheight = output->base.current_mode->height;
	}

	output->gl.egl_window =
		wl_egl_window_create(output->parent.surface,
				     fwidth, fheight);
	if (!output->gl.egl_window) {
		weston_log("failure to create wl_egl_window\n");
		return -1;
	}

	if (gl_renderer->output_window_create(&output->base,
					      output->gl.egl_window,
					      output->gl.egl_window,
					      gl_renderer->alpha_attribs,
					      NULL,
					      0) < 0)
		goto cleanup_window;

	return 0;

cleanup_window:
	wl_egl_window_destroy(output->gl.egl_window);
	return -1;
}
#endif

static int
wayland_output_init_pixman_renderer(struct wayland_output *output)
{
	return pixman_renderer_output_create(&output->base,
				     PIXMAN_RENDERER_OUTPUT_USE_SHADOW);
}

static void
wayland_output_resize_surface(struct wayland_output *output)
{
	struct wayland_backend *b =
		to_wayland_backend(output->base.compositor);
	int32_t ix, iy, iwidth, iheight;
	int32_t width, height;
	struct wl_region *region;

	width = output->base.current_mode->width;
	height = output->base.current_mode->height;

	if (output->frame) {
		frame_resize_inside(output->frame, width, height);

		frame_input_rect(output->frame, &ix, &iy, &iwidth, &iheight);
		region = wl_compositor_create_region(b->parent.compositor);
		wl_region_add(region, ix, iy, iwidth, iheight);
		wl_surface_set_input_region(output->parent.surface, region);
		wl_region_destroy(region);

		if (output->parent.xdg_surface) {
			xdg_surface_set_window_geometry(output->parent.xdg_surface,
							ix,
							iy,
							iwidth,
							iheight);
		}

		frame_opaque_rect(output->frame, &ix, &iy, &iwidth, &iheight);
		region = wl_compositor_create_region(b->parent.compositor);
		wl_region_add(region, ix, iy, iwidth, iheight);
		wl_surface_set_opaque_region(output->parent.surface, region);
		wl_region_destroy(region);

		width = frame_width(output->frame);
		height = frame_height(output->frame);
	} else {
		region = wl_compositor_create_region(b->parent.compositor);
		wl_region_add(region, 0, 0, width, height);
		wl_surface_set_input_region(output->parent.surface, region);
		wl_region_destroy(region);

		region = wl_compositor_create_region(b->parent.compositor);
		wl_region_add(region, 0, 0, width, height);
		wl_surface_set_opaque_region(output->parent.surface, region);
		wl_region_destroy(region);

		if (output->parent.xdg_surface) {
			xdg_surface_set_window_geometry(output->parent.xdg_surface,
							0,
							0,
							width,
							height);
		}
	}

#ifdef ENABLE_EGL
	if (output->gl.egl_window) {
		wl_egl_window_resize(output->gl.egl_window,
				     width, height, 0, 0);

		/* These will need to be re-created due to the resize */
		gl_renderer->output_set_border(&output->base,
					       GL_RENDERER_BORDER_TOP,
					       0, 0, 0, NULL);
		cairo_surface_destroy(output->gl.border.top);
		output->gl.border.top = NULL;
		gl_renderer->output_set_border(&output->base,
					       GL_RENDERER_BORDER_LEFT,
					       0, 0, 0, NULL);
		cairo_surface_destroy(output->gl.border.left);
		output->gl.border.left = NULL;
		gl_renderer->output_set_border(&output->base,
					       GL_RENDERER_BORDER_RIGHT,
					       0, 0, 0, NULL);
		cairo_surface_destroy(output->gl.border.right);
		output->gl.border.right = NULL;
		gl_renderer->output_set_border(&output->base,
					       GL_RENDERER_BORDER_BOTTOM,
					       0, 0, 0, NULL);
		cairo_surface_destroy(output->gl.border.bottom);
		output->gl.border.bottom = NULL;
	}
#endif

	wayland_output_destroy_shm_buffers(output);
}

static int
wayland_output_set_windowed(struct wayland_output *output)
{
	struct wayland_backend *b =
		to_wayland_backend(output->base.compositor);

	if (output->frame)
		return 0;

	if (!b->theme) {
		b->theme = theme_create();
		if (!b->theme)
			return -1;
	}
	output->frame = frame_create(b->theme, 100, 100,
	                             FRAME_BUTTON_CLOSE, output->title, NULL);
	if (!output->frame)
		return -1;

	if (output->keyboard_count)
		frame_set_flag(output->frame, FRAME_FLAG_ACTIVE);

	wayland_output_resize_surface(output);

	if (output->parent.xdg_toplevel) {
		xdg_toplevel_unset_fullscreen(output->parent.xdg_toplevel);
	} else if (output->parent.shell_surface) {
		wl_shell_surface_set_toplevel(output->parent.shell_surface);
	} else {
		abort();
	}

	return 0;
}

static void
wayland_output_set_fullscreen(struct wayland_output *output,
			      enum wl_shell_surface_fullscreen_method method,
			      uint32_t framerate, struct wl_output *target)
{
	if (output->frame) {
		frame_destroy(output->frame);
		output->frame = NULL;
	}

	wayland_output_resize_surface(output);

	if (output->parent.xdg_toplevel) {
		xdg_toplevel_set_fullscreen(output->parent.xdg_toplevel, target);
	} else if (output->parent.shell_surface) {
		wl_shell_surface_set_fullscreen(output->parent.shell_surface,
						method, framerate, target);
	} else {
		abort();
	}
}

static struct weston_mode *
wayland_output_choose_mode(struct wayland_output *output,
			   struct weston_mode *ref_mode)
{
	struct weston_mode *mode;

	/* First look for an exact match */
	wl_list_for_each(mode, &output->base.mode_list, link)
		if (mode->width == ref_mode->width &&
		    mode->height == ref_mode->height &&
		    mode->refresh == ref_mode->refresh)
			return mode;

	/* If we can't find an exact match, ignore refresh and try again */
	wl_list_for_each(mode, &output->base.mode_list, link)
		if (mode->width == ref_mode->width &&
		    mode->height == ref_mode->height)
			return mode;

	/* Yeah, we failed */
	return NULL;
}

enum mode_status {
	MODE_STATUS_UNKNOWN,
	MODE_STATUS_SUCCESS,
	MODE_STATUS_FAIL,
	MODE_STATUS_CANCEL,
};

static void
mode_feedback_successful(void *data,
			 struct zwp_fullscreen_shell_mode_feedback_v1 *fb)
{
	enum mode_status *value = data;

	printf("Mode switch successful\n");

	*value = MODE_STATUS_SUCCESS;
}

static void
mode_feedback_failed(void *data, struct zwp_fullscreen_shell_mode_feedback_v1 *fb)
{
	enum mode_status *value = data;

	printf("Mode switch failed\n");

	*value = MODE_STATUS_FAIL;
}

static void
mode_feedback_cancelled(void *data, struct zwp_fullscreen_shell_mode_feedback_v1 *fb)
{
	enum mode_status *value = data;

	printf("Mode switch cancelled\n");

	*value = MODE_STATUS_CANCEL;
}

struct zwp_fullscreen_shell_mode_feedback_v1_listener mode_feedback_listener = {
	mode_feedback_successful,
	mode_feedback_failed,
	mode_feedback_cancelled,
};

static enum mode_status
wayland_output_fullscreen_shell_mode_feedback(struct wayland_output *output,
					      struct weston_mode *mode)
{
	struct wayland_backend *b = to_wayland_backend(output->base.compositor);
	struct zwp_fullscreen_shell_mode_feedback_v1 *mode_feedback;
	enum mode_status mode_status;
	int ret = 0;

	mode_feedback =
		zwp_fullscreen_shell_v1_present_surface_for_mode(b->parent.fshell,
								 output->parent.surface,
								 output->parent.output,
								 mode->refresh);

	zwp_fullscreen_shell_mode_feedback_v1_add_listener(mode_feedback,
							   &mode_feedback_listener,
							   &mode_status);

	output->parent.draw_initial_frame = false;
	draw_initial_frame(output);
	wl_surface_commit(output->parent.surface);

	mode_status = MODE_STATUS_UNKNOWN;
	while (mode_status == MODE_STATUS_UNKNOWN && ret >= 0)
		ret = wl_display_dispatch(b->parent.wl_display);

	zwp_fullscreen_shell_mode_feedback_v1_destroy(mode_feedback);

	return mode_status;
}

static int
wayland_output_switch_mode(struct weston_output *output_base,
			   struct weston_mode *mode)
{
	struct wayland_output *output = to_wayland_output(output_base);
	struct wayland_backend *b;
	struct wl_surface *old_surface;
	struct weston_mode *old_mode;
	enum mode_status mode_status;

	if (output_base == NULL) {
		weston_log("output is NULL.\n");
		return -1;
	}

	if (mode == NULL) {
		weston_log("mode is NULL.\n");
		return -1;
	}

	b = to_wayland_backend(output_base->compositor);

	if (output->parent.xdg_surface || output->parent.shell_surface || !b->parent.fshell)
		return -1;

	mode = wayland_output_choose_mode(output, mode);
	if (mode == NULL)
		return -1;

	if (output->base.current_mode == mode)
		return 0;

	old_mode = output->base.current_mode;
	old_surface = output->parent.surface;
	output->base.current_mode = mode;
	output->parent.surface =
		wl_compositor_create_surface(b->parent.compositor);
	wl_surface_set_user_data(output->parent.surface, output);

	/* Blow the old buffers because we changed size/surfaces */
	wayland_output_resize_surface(output);

	mode_status = wayland_output_fullscreen_shell_mode_feedback(output, mode);

	/* This should kick-start things again */
	wayland_output_start_repaint_loop(&output->base);

	if (mode_status == MODE_STATUS_FAIL) {
		output->base.current_mode = old_mode;
		wl_surface_destroy(output->parent.surface);
		output->parent.surface = old_surface;
		wayland_output_resize_surface(output);

		return -1;
	}

	old_mode->flags &= ~WL_OUTPUT_MODE_CURRENT;
	output->base.current_mode->flags |= WL_OUTPUT_MODE_CURRENT;

	if (b->use_pixman) {
		pixman_renderer_output_destroy(output_base);
		if (wayland_output_init_pixman_renderer(output) < 0)
			goto err_output;
#ifdef ENABLE_EGL
	} else {
		gl_renderer->output_destroy(output_base);
		wl_egl_window_destroy(output->gl.egl_window);
		if (wayland_output_init_gl_renderer(output) < 0)
			goto err_output;
#endif
	}
	wl_surface_destroy(old_surface);

	weston_output_schedule_repaint(&output->base);

	return 0;

err_output:
	/* XXX */
	return -1;
}

static void
handle_xdg_surface_configure(void *data, struct xdg_surface *surface,
			 uint32_t serial)
{
	xdg_surface_ack_configure(surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	handle_xdg_surface_configure
};

static void
handle_xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel,
			  int32_t width, int32_t height,
			  struct wl_array *states)
{
	struct wayland_output *output = data;

	output->parent.configure_width = width;
	output->parent.configure_height = height;

	output->parent.wait_for_configure = false;
	/* FIXME: implement resizing */
}

static void
handle_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel)
{
	struct wayland_output *output = data;
	struct weston_compositor *compositor = output->base.compositor;

	wayland_output_destroy(&output->base);

	if (wl_list_empty(&compositor->output_list))
		weston_compositor_exit(compositor);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	handle_xdg_toplevel_configure,
	handle_xdg_toplevel_close,
};

static int
wayland_backend_create_output_surface(struct wayland_output *output)
{
	struct wayland_backend *b = to_wayland_backend(output->base.compositor);

	assert(!output->parent.surface);

	output->parent.surface =
		wl_compositor_create_surface(b->parent.compositor);
	if (!output->parent.surface)
		return -1;

	wl_surface_set_user_data(output->parent.surface, output);

	output->parent.draw_initial_frame = true;

	if (b->parent.xdg_wm_base) {
		output->parent.xdg_surface =
		xdg_wm_base_get_xdg_surface(b->parent.xdg_wm_base,
					    output->parent.surface);
		xdg_surface_add_listener(output->parent.xdg_surface,
					 &xdg_surface_listener, output);

		output->parent.xdg_toplevel =
			xdg_surface_get_toplevel(output->parent.xdg_surface);
		xdg_toplevel_add_listener(output->parent.xdg_toplevel,
					  &xdg_toplevel_listener, output);

		xdg_toplevel_set_title(output->parent.xdg_toplevel, output->title);

		wl_surface_commit(output->parent.surface);

		output->parent.wait_for_configure = true;

		while (output->parent.wait_for_configure)
			wl_display_dispatch(b->parent.wl_display);

		weston_log("wayland-backend: Using xdg_wm_base\n");
	}
	else if (b->parent.shell) {
		output->parent.shell_surface =
			wl_shell_get_shell_surface(b->parent.shell,
						   output->parent.surface);
		if (!output->parent.shell_surface) {
			wl_surface_destroy(output->parent.surface);
			return -1;
		}

		wl_shell_surface_add_listener(output->parent.shell_surface,
					      &shell_surface_listener, output);

		weston_log("wayland-backend: Using wl_shell\n");
	}

	return 0;
}

static int
wayland_output_enable(struct weston_output *base)
{
	struct wayland_output *output = to_wayland_output(base);
	struct wayland_backend *b = to_wayland_backend(base->compositor);
	enum mode_status mode_status;
	int ret = 0;

	weston_log("Creating %dx%d wayland output at (%d, %d)\n",
		   output->base.current_mode->width,
		   output->base.current_mode->height,
		   output->base.x, output->base.y);

	if (!output->parent.surface)
		ret = wayland_backend_create_output_surface(output);

	if (ret < 0)
		return -1;

	wl_list_init(&output->shm.buffers);
	wl_list_init(&output->shm.free_buffers);

	if (b->use_pixman) {
		if (wayland_output_init_pixman_renderer(output) < 0)
			goto err_output;

		output->base.repaint = wayland_output_repaint_pixman;
#ifdef ENABLE_EGL
	} else {
		if (wayland_output_init_gl_renderer(output) < 0)
			goto err_output;

		output->base.repaint = wayland_output_repaint_gl;
#endif
	}

	output->base.start_repaint_loop = wayland_output_start_repaint_loop;
	output->base.assign_planes = NULL;
	output->base.set_backlight = NULL;
	output->base.set_dpms = NULL;
	output->base.switch_mode = wayland_output_switch_mode;

	if (b->sprawl_across_outputs) {
		if (b->parent.fshell) {
			wayland_output_resize_surface(output);

			mode_status = wayland_output_fullscreen_shell_mode_feedback(output, &output->mode);

			if (mode_status == MODE_STATUS_FAIL) {
				zwp_fullscreen_shell_v1_present_surface(b->parent.fshell,
									output->parent.surface,
									ZWP_FULLSCREEN_SHELL_V1_PRESENT_METHOD_CENTER,
									output->parent.output);

				output->parent.draw_initial_frame = true;
			}
		} else {
			wayland_output_set_fullscreen(output,
						      WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER,
						      output->mode.refresh, output->parent.output);
		}
	} else if (b->fullscreen) {
		wayland_output_set_fullscreen(output, 0, 0, NULL);
	} else {
		wayland_output_set_windowed(output);
	}

	return 0;

err_output:
	wayland_backend_destroy_output_surface(output);

	return -1;
}

static int
wayland_output_setup_for_parent_output(struct wayland_output *output,
				       struct wayland_parent_output *poutput);

static int
wayland_output_setup_fullscreen(struct wayland_output *output,
				struct wayland_head *head);

static int
wayland_output_attach_head(struct weston_output *output_base,
			   struct weston_head *head_base)
{
	struct wayland_backend *b = to_wayland_backend(output_base->compositor);
	struct wayland_output *output = to_wayland_output(output_base);
	struct wayland_head *head = to_wayland_head(head_base);

	if (!wl_list_empty(&output->base.head_list))
		return -1;

	if (head->parent_output) {
		if (wayland_output_setup_for_parent_output(output,
							   head->parent_output) < 0)
			return -1;
	} else if (b->fullscreen) {
		if (wayland_output_setup_fullscreen(output, head) < 0)
			return -1;
	} else {
		/* A floating window, nothing to do. */
	}

	return 0;
}

static void
wayland_output_detach_head(struct weston_output *output_base,
			   struct weston_head *head)
{
	struct wayland_output *output = to_wayland_output(output_base);

	/* Rely on the disable hook if the output was enabled. We do not
	 * support cloned heads, so detaching is guaranteed to disable the
	 * output.
	 */
	if (output->base.enabled)
		return;

	/* undo setup fullscreen */
	if (output->parent.surface)
		wayland_backend_destroy_output_surface(output);
}

static struct weston_output *
wayland_output_create(struct weston_compositor *compositor, const char *name)
{
	struct wayland_output *output;
	char *title;

	/* name can't be NULL. */
	assert(name);

	output = zalloc(sizeof *output);
	if (output == NULL) {
		perror("zalloc");
		return NULL;
	}

	if (asprintf(&title, "%s - %s", WINDOW_TITLE, name) < 0) {
		free(output);
		return NULL;
	}
	output->title = title;

	weston_output_init(&output->base, compositor, name);

	output->base.destroy = wayland_output_destroy;
	output->base.disable = wayland_output_disable;
	output->base.enable = wayland_output_enable;
	output->base.attach_head = wayland_output_attach_head;
	output->base.detach_head = wayland_output_detach_head;

	weston_compositor_add_pending_output(&output->base, compositor);

	return &output->base;
}

static struct wayland_head *
wayland_head_create(struct weston_compositor *compositor, const char *name)
{
	struct wayland_head *head;

	assert(name);

	head = zalloc(sizeof *head);
	if (!head)
		return NULL;

	weston_head_init(&head->base, name);
	weston_head_set_connection_status(&head->base, true);
	weston_compositor_add_head(compositor, &head->base);

	return head;
}

static int
wayland_head_create_windowed(struct weston_compositor *compositor,
			     const char *name)
{
	 if (!wayland_head_create(compositor, name))
		return -1;

	return 0;
}

static int
wayland_head_create_for_parent_output(struct weston_compositor *compositor,
				      struct wayland_parent_output *poutput)
{
	struct wayland_head *head;
	char name[100];
	int ret;

	ret = snprintf(name, sizeof(name), "wlparent-%d", poutput->id);
	if (ret < 1 || (unsigned)ret >= sizeof(name))
		return -1;

	head = wayland_head_create(compositor, name);
	if (!head)
		return -1;

	assert(!poutput->head);
	head->parent_output = poutput;
	poutput->head = head;

	weston_head_set_monitor_strings(&head->base,
					poutput->physical.make,
					poutput->physical.model, NULL);
	weston_head_set_physical_size(&head->base,
				      poutput->physical.width,
				      poutput->physical.height);

	return 0;
}

static void
wayland_head_destroy(struct wayland_head *head)
{
	if (head->parent_output)
		head->parent_output->head = NULL;

	weston_head_release(&head->base);
	free(head);
}

static int
wayland_output_set_size(struct weston_output *base, int width, int height)
{
	struct wayland_output *output = to_wayland_output(base);
	struct weston_head *head;
	int output_width, output_height;

	/* We can only be called once. */
	assert(!output->base.current_mode);

	/* Make sure we have scale set. */
	assert(output->base.scale);

	if (width < 1) {
		weston_log("Invalid width \"%d\" for output %s\n",
			   width, output->base.name);
		return -1;
	}

	if (height < 1) {
		weston_log("Invalid height \"%d\" for output %s\n",
			   height, output->base.name);
		return -1;
	}

	wl_list_for_each(head, &output->base.head_list, output_link) {
		weston_head_set_monitor_strings(head, "wayland", "none", NULL);

		/* XXX: Calculate proper size. */
		weston_head_set_physical_size(head, width, height);
	}

	output_width = width * output->base.scale;
	output_height = height * output->base.scale;

	output->mode.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;

	output->mode.width = output_width;
	output->mode.height = output_height;
	output->mode.refresh = 60000;
	wl_list_insert(&output->base.mode_list, &output->mode.link);

	output->base.current_mode = &output->mode;

	return 0;
}

static int
wayland_output_setup_for_parent_output(struct wayland_output *output,
				       struct wayland_parent_output *poutput)
{
	struct weston_mode *mode;

	if (poutput->current_mode) {
		mode = poutput->current_mode;
	} else if (poutput->preferred_mode) {
		mode = poutput->preferred_mode;
	} else if (!wl_list_empty(&poutput->mode_list)) {
		mode = container_of(poutput->mode_list.next,
				    struct weston_mode, link);
	} else {
		weston_log("No valid modes found. Skipping output.\n");
		return -1;
	}

	output->base.scale = 1;
	output->base.transform = WL_OUTPUT_TRANSFORM_NORMAL;

	output->parent.output = poutput->global;

	wl_list_insert_list(&output->base.mode_list, &poutput->mode_list);
	wl_list_init(&poutput->mode_list);

	/* No other mode should have CURRENT already. */
	mode->flags |= WL_OUTPUT_MODE_CURRENT;
	output->base.current_mode = mode;

	/* output->mode is unused in this path. */

	return 0;
}

static int
wayland_output_setup_fullscreen(struct wayland_output *output,
				struct wayland_head *head)
{
	struct wayland_backend *b = to_wayland_backend(output->base.compositor);
	int width = 0, height = 0;

	output->base.scale = 1;
	output->base.transform = WL_OUTPUT_TRANSFORM_NORMAL;

	if (wayland_backend_create_output_surface(output) < 0)
		return -1;

	/* What should size be set if conditional is false? */
	if (b->parent.xdg_wm_base || b->parent.shell) {
		if (output->parent.xdg_toplevel)
			xdg_toplevel_set_fullscreen(output->parent.xdg_toplevel,
						    output->parent.output);
		else if (output->parent.shell_surface)
			wl_shell_surface_set_fullscreen(output->parent.shell_surface,
							0, 0, NULL);

		wl_display_roundtrip(b->parent.wl_display);

		width = output->parent.configure_width;
		height = output->parent.configure_height;
	}

	if (wayland_output_set_size(&output->base, width, height) < 0)
		goto err_set_size;

	/* The head is not attached yet, so set_size did not set these. */
	weston_head_set_monitor_strings(&head->base, "wayland", "none", NULL);
	/* XXX: Calculate proper size. */
	weston_head_set_physical_size(&head->base, width, height);

	return 0;

err_set_size:
	wayland_backend_destroy_output_surface(output);

	return -1;
}

static void
shell_surface_ping(void *data, struct wl_shell_surface *shell_surface,
		   uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static void
shell_surface_configure(void *data, struct wl_shell_surface *shell_surface,
			uint32_t edges, int32_t width, int32_t height)
{
	struct wayland_output *output = data;

	output->parent.configure_width = width;
	output->parent.configure_height = height;

	/* FIXME: implement resizing */
}

static void
shell_surface_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
	shell_surface_ping,
	shell_surface_configure,
	shell_surface_popup_done
};

/* Events received from the wayland-server this compositor is client of: */

/* parent input interface */
static void
input_set_cursor(struct wayland_input *input)
{

	struct wl_buffer *buffer;
	struct wl_cursor_image *image;

	if (!input->backend->cursor)
		return; /* Couldn't load the cursor. Can't set it */

	image = input->backend->cursor->images[0];
	buffer = wl_cursor_image_get_buffer(image);
	if (!buffer)
		return;

	wl_pointer_set_cursor(input->parent.pointer, input->enter_serial,
			      input->parent.cursor.surface,
			      image->hotspot_x, image->hotspot_y);

	wl_surface_attach(input->parent.cursor.surface, buffer, 0, 0);
	wl_surface_damage(input->parent.cursor.surface, 0, 0,
			  image->width, image->height);
	wl_surface_commit(input->parent.cursor.surface);
}

static void
input_handle_pointer_enter(void *data, struct wl_pointer *pointer,
			   uint32_t serial, struct wl_surface *surface,
			   wl_fixed_t fixed_x, wl_fixed_t fixed_y)
{
	struct wayland_input *input = data;
	int32_t fx, fy;
	enum theme_location location;
	double x, y;

	if (!surface) {
		input->output = NULL;
		input->has_focus = false;
		notify_pointer_focus(&input->base, NULL, 0, 0);
		return;
	}

	x = wl_fixed_to_double(fixed_x);
	y = wl_fixed_to_double(fixed_y);

	/* XXX: If we get a modifier event immediately before the focus,
	 *      we should try to keep the same serial. */
	input->enter_serial = serial;
	input->output = wl_surface_get_user_data(surface);

	if (input->output->frame) {
		location = frame_pointer_enter(input->output->frame, input,
					       x, y);
		frame_interior(input->output->frame, &fx, &fy, NULL, NULL);
		x -= fx;
		y -= fy;

		if (frame_status(input->output->frame) & FRAME_STATUS_REPAINT)
			weston_output_schedule_repaint(&input->output->base);
	} else {
		location = THEME_LOCATION_CLIENT_AREA;
	}

	weston_output_transform_coordinate(&input->output->base, x, y, &x, &y);

	if (location == THEME_LOCATION_CLIENT_AREA) {
		input->has_focus = true;
		notify_pointer_focus(&input->base, &input->output->base, x, y);
		wl_pointer_set_cursor(input->parent.pointer,
				      input->enter_serial, NULL, 0, 0);
	} else {
		input->has_focus = false;
		notify_pointer_focus(&input->base, NULL, 0, 0);
		input_set_cursor(input);
	}
}

static void
input_handle_pointer_leave(void *data, struct wl_pointer *pointer,
			   uint32_t serial, struct wl_surface *surface)
{
	struct wayland_input *input = data;

	if (!input->output)
		return;

	if (input->output->frame) {
		frame_pointer_leave(input->output->frame, input);

		if (frame_status(input->output->frame) & FRAME_STATUS_REPAINT)
			weston_output_schedule_repaint(&input->output->base);
	}

	notify_pointer_focus(&input->base, NULL, 0, 0);
	input->output = NULL;
	input->has_focus = false;
}

static void
input_handle_motion(void *data, struct wl_pointer *pointer,
		    uint32_t time, wl_fixed_t fixed_x, wl_fixed_t fixed_y)
{
	struct wayland_input *input = data;
	int32_t fx, fy;
	enum theme_location location;
	bool want_frame = false;
	double x, y;
	struct timespec ts;

	if (!input->output)
		return;

	x = wl_fixed_to_double(fixed_x);
	y = wl_fixed_to_double(fixed_y);

	if (input->output->frame) {
		location = frame_pointer_motion(input->output->frame, input,
						x, y);
		frame_interior(input->output->frame, &fx, &fy, NULL, NULL);
		x -= fx;
		y -= fy;

		if (frame_status(input->output->frame) & FRAME_STATUS_REPAINT)
			weston_output_schedule_repaint(&input->output->base);
	} else {
		location = THEME_LOCATION_CLIENT_AREA;
	}

	weston_output_transform_coordinate(&input->output->base, x, y, &x, &y);

	if (input->has_focus && location != THEME_LOCATION_CLIENT_AREA) {
		input_set_cursor(input);
		notify_pointer_focus(&input->base, NULL, 0, 0);
		input->has_focus = false;
		want_frame = true;
	} else if (!input->has_focus &&
		   location == THEME_LOCATION_CLIENT_AREA) {
		wl_pointer_set_cursor(input->parent.pointer,
				      input->enter_serial, NULL, 0, 0);
		notify_pointer_focus(&input->base, &input->output->base, x, y);
		input->has_focus = true;
		want_frame = true;
	}

	if (location == THEME_LOCATION_CLIENT_AREA) {
		timespec_from_msec(&ts, time);
		notify_motion_absolute(&input->base, &ts, x, y);
		want_frame = true;
	}

	if (want_frame && input->seat_version < WL_POINTER_FRAME_SINCE_VERSION)
		notify_pointer_frame(&input->base);
}

static void
input_handle_button(void *data, struct wl_pointer *pointer,
		    uint32_t serial, uint32_t time, uint32_t button,
		    enum wl_pointer_button_state state)
{
	struct wayland_input *input = data;
	enum theme_location location;
	struct timespec ts;

	if (!input->output)
		return;

	if (input->output->frame) {
		location = frame_pointer_button(input->output->frame, input,
						button, state);

		if (frame_status(input->output->frame) & FRAME_STATUS_MOVE) {
			if (input->output->parent.xdg_toplevel)
				xdg_toplevel_move(input->output->parent.xdg_toplevel,
						  input->parent.seat, serial);
			else if (input->output->parent.shell_surface)
				wl_shell_surface_move(input->output->parent.shell_surface,
						      input->parent.seat, serial);
			frame_status_clear(input->output->frame,
					   FRAME_STATUS_MOVE);
			return;
		}

		if (frame_status(input->output->frame) & FRAME_STATUS_CLOSE) {
			wayland_output_destroy(&input->output->base);
			input->output = NULL;
			input->keyboard_focus = NULL;

			if (wl_list_empty(&input->backend->compositor->output_list))
				weston_compositor_exit(input->backend->compositor);

			return;
		}

		if (frame_status(input->output->frame) & FRAME_STATUS_REPAINT)
			weston_output_schedule_repaint(&input->output->base);
	} else {
		location = THEME_LOCATION_CLIENT_AREA;
	}

	if (location == THEME_LOCATION_CLIENT_AREA) {
		timespec_from_msec(&ts, time);
		notify_button(&input->base, &ts, button, state);
		if (input->seat_version < WL_POINTER_FRAME_SINCE_VERSION)
			notify_pointer_frame(&input->base);
	}
}

static void
input_handle_axis(void *data, struct wl_pointer *pointer,
		  uint32_t time, uint32_t axis, wl_fixed_t value)
{
	struct wayland_input *input = data;
	struct weston_pointer_axis_event weston_event;
	struct timespec ts;

	weston_event.axis = axis;
	weston_event.value = wl_fixed_to_double(value);
	weston_event.has_discrete = false;

	if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL &&
	    input->vert.has_discrete) {
		weston_event.has_discrete = true;
		weston_event.discrete = input->vert.discrete;
		input->vert.has_discrete = false;
	} else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL &&
		   input->horiz.has_discrete) {
		weston_event.has_discrete = true;
		weston_event.discrete = input->horiz.discrete;
		input->horiz.has_discrete = false;
	}

	timespec_from_msec(&ts, time);

	notify_axis(&input->base, &ts, &weston_event);

	if (input->seat_version < WL_POINTER_FRAME_SINCE_VERSION)
		notify_pointer_frame(&input->base);
}

static void
input_handle_frame(void *data, struct wl_pointer *pointer)
{
	struct wayland_input *input = data;

	notify_pointer_frame(&input->base);
}

static void
input_handle_axis_source(void *data, struct wl_pointer *pointer,
			 uint32_t source)
{
	struct wayland_input *input = data;

	notify_axis_source(&input->base, source);
}

static void
input_handle_axis_stop(void *data, struct wl_pointer *pointer,
		       uint32_t time, uint32_t axis)
{
	struct wayland_input *input = data;
	struct weston_pointer_axis_event weston_event;
	struct timespec ts;

	weston_event.axis = axis;
	weston_event.value = 0;

	timespec_from_msec(&ts, time);

	notify_axis(&input->base, &ts, &weston_event);
}

static void
input_handle_axis_discrete(void *data, struct wl_pointer *pointer,
			   uint32_t axis, int32_t discrete)
{
	struct wayland_input *input = data;

	if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
		input->vert.has_discrete = true;
		input->vert.discrete = discrete;
	} else if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) {
		input->horiz.has_discrete = true;
		input->horiz.discrete = discrete;
	}
}

static const struct wl_pointer_listener pointer_listener = {
	input_handle_pointer_enter,
	input_handle_pointer_leave,
	input_handle_motion,
	input_handle_button,
	input_handle_axis,
	input_handle_frame,
	input_handle_axis_source,
	input_handle_axis_stop,
	input_handle_axis_discrete,
};

static void
input_handle_keymap(void *data, struct wl_keyboard *keyboard, uint32_t format,
		    int fd, uint32_t size)
{
	struct wayland_input *input = data;
	struct xkb_keymap *keymap;
	char *map_str;

	if (!data) {
		close(fd);
		return;
	}

	if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
		if (map_str == MAP_FAILED) {
			weston_log("mmap failed: %s\n", strerror(errno));
			goto error;
		}

		keymap = xkb_keymap_new_from_string(input->backend->compositor->xkb_context,
						    map_str,
						    XKB_KEYMAP_FORMAT_TEXT_V1,
						    0);
		munmap(map_str, size);

		if (!keymap) {
			weston_log("failed to compile keymap\n");
			goto error;
		}

		input->keyboard_state_update = STATE_UPDATE_NONE;
	} else if (format == WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP) {
		weston_log("No keymap provided; falling back to default\n");
		keymap = NULL;
		input->keyboard_state_update = STATE_UPDATE_AUTOMATIC;
	} else {
		weston_log("Invalid keymap\n");
		goto error;
	}

	close(fd);

	if (weston_seat_get_keyboard(&input->base))
		weston_seat_update_keymap(&input->base, keymap);
	else
		weston_seat_init_keyboard(&input->base, keymap);

	xkb_keymap_unref(keymap);

	return;

error:
	wl_keyboard_release(input->parent.keyboard);
	close(fd);
}

static void
input_handle_keyboard_enter(void *data,
			    struct wl_keyboard *keyboard,
			    uint32_t serial,
			    struct wl_surface *surface,
			    struct wl_array *keys)
{
	struct wayland_input *input = data;
	struct wayland_output *focus;

	focus = input->keyboard_focus;
	if (focus) {
		/* This shouldn't happen */
		focus->keyboard_count--;
		if (!focus->keyboard_count && focus->frame)
			frame_unset_flag(focus->frame, FRAME_FLAG_ACTIVE);
		if (frame_status(focus->frame) & FRAME_STATUS_REPAINT)
			weston_output_schedule_repaint(&focus->base);
	}

	if (!surface) {
		input->keyboard_focus = NULL;
		return;
	}

	input->keyboard_focus = wl_surface_get_user_data(surface);
	input->keyboard_focus->keyboard_count++;

	focus = input->keyboard_focus;
	if (focus->frame) {
		frame_set_flag(focus->frame, FRAME_FLAG_ACTIVE);
		if (frame_status(focus->frame) & FRAME_STATUS_REPAINT)
			weston_output_schedule_repaint(&focus->base);
	}


	/* XXX: If we get a modifier event immediately before the focus,
	 *      we should try to keep the same serial. */
	notify_keyboard_focus_in(&input->base, keys,
				 STATE_UPDATE_AUTOMATIC);
}

static void
input_handle_keyboard_leave(void *data,
			    struct wl_keyboard *keyboard,
			    uint32_t serial,
			    struct wl_surface *surface)
{
	struct wayland_input *input = data;
	struct wayland_output *focus;

	notify_keyboard_focus_out(&input->base);

	focus = input->keyboard_focus;
	if (!focus)
		return;

	focus->keyboard_count--;
	if (!focus->keyboard_count && focus->frame) {
		frame_unset_flag(focus->frame, FRAME_FLAG_ACTIVE);
		if (frame_status(focus->frame) & FRAME_STATUS_REPAINT)
			weston_output_schedule_repaint(&focus->base);
	}

	input->keyboard_focus = NULL;
}

static void
input_handle_key(void *data, struct wl_keyboard *keyboard,
		 uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	struct wayland_input *input = data;
	struct timespec ts;

	if (!input->keyboard_focus)
		return;

	timespec_from_msec(&ts, time);

	input->key_serial = serial;
	notify_key(&input->base, &ts, key,
		   state ? WL_KEYBOARD_KEY_STATE_PRESSED :
			   WL_KEYBOARD_KEY_STATE_RELEASED,
		   input->keyboard_state_update);
}

static void
input_handle_modifiers(void *data, struct wl_keyboard *wl_keyboard,
		       uint32_t serial_in, uint32_t mods_depressed,
		       uint32_t mods_latched, uint32_t mods_locked,
		       uint32_t group)
{
	struct weston_keyboard *keyboard;
	struct wayland_input *input = data;
	struct wayland_backend *b = input->backend;
	uint32_t serial_out;

	/* If we get a key event followed by a modifier event with the
	 * same serial number, then we try to preserve those semantics by
	 * reusing the same serial number on the way out too. */
	if (serial_in == input->key_serial)
		serial_out = wl_display_get_serial(b->compositor->wl_display);
	else
		serial_out = wl_display_next_serial(b->compositor->wl_display);

	keyboard = weston_seat_get_keyboard(&input->base);
	xkb_state_update_mask(keyboard->xkb_state.state,
			      mods_depressed, mods_latched,
			      mods_locked, 0, 0, group);
	notify_modifiers(&input->base, serial_out);
}

static void
input_handle_repeat_info(void *data, struct wl_keyboard *keyboard,
			 int32_t rate, int32_t delay)
{
	struct wayland_input *input = data;
	struct wayland_backend *b = input->backend;

	b->compositor->kb_repeat_rate = rate;
	b->compositor->kb_repeat_delay = delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
	input_handle_keymap,
	input_handle_keyboard_enter,
	input_handle_keyboard_leave,
	input_handle_key,
	input_handle_modifiers,
	input_handle_repeat_info,
};

static void
input_handle_touch_down(void *data, struct wl_touch *wl_touch,
			uint32_t serial, uint32_t time,
			struct wl_surface *surface, int32_t id,
			wl_fixed_t fixed_x, wl_fixed_t fixed_y)
{
	struct wayland_input *input = data;
	struct wayland_output *output;
	enum theme_location location;
	bool first_touch;
	int32_t fx, fy;
	double x, y;
	struct timespec ts;

	x = wl_fixed_to_double(fixed_x);
	y = wl_fixed_to_double(fixed_y);

	timespec_from_msec(&ts, time);

	first_touch = (input->touch_points == 0);
	input->touch_points++;

	input->touch_focus = wl_surface_get_user_data(surface);
	output = input->touch_focus;
	if (!first_touch && !input->touch_active)
		return;

	if (output->frame) {
		location = frame_touch_down(output->frame, input, id, x, y);

		frame_interior(output->frame, &fx, &fy, NULL, NULL);
		x -= fx;
		y -= fy;

		if (frame_status(output->frame) & FRAME_STATUS_REPAINT)
			weston_output_schedule_repaint(&output->base);

		if (first_touch && (frame_status(output->frame) & FRAME_STATUS_MOVE)) {
			input->touch_points--;
			if (output->parent.xdg_toplevel)
				xdg_toplevel_move(output->parent.xdg_toplevel,
						  input->parent.seat, serial);
			else if (output->parent.shell_surface)
				wl_shell_surface_move(output->parent.shell_surface,
						      input->parent.seat, serial);
			frame_status_clear(output->frame,
					   FRAME_STATUS_MOVE);
			return;
		}

		if (first_touch && location != THEME_LOCATION_CLIENT_AREA)
			return;
	}

	weston_output_transform_coordinate(&output->base, x, y, &x, &y);

	notify_touch(input->touch_device, &ts, id, x, y, WL_TOUCH_DOWN);
	input->touch_active = true;
}

static void
input_handle_touch_up(void *data, struct wl_touch *wl_touch,
		      uint32_t serial, uint32_t time, int32_t id)
{
	struct wayland_input *input = data;
	struct wayland_output *output = input->touch_focus;
	bool active = input->touch_active;
	struct timespec ts;

	timespec_from_msec(&ts, time);

	input->touch_points--;
	if (input->touch_points == 0) {
		input->touch_focus = NULL;
		input->touch_active = false;
	}

	if (!output)
		return;

	if (output->frame) {
		frame_touch_up(output->frame, input, id);

		if (frame_status(output->frame) & FRAME_STATUS_CLOSE) {
			wayland_output_destroy(&output->base);
			input->touch_focus = NULL;
			input->keyboard_focus = NULL;
			if (wl_list_empty(&input->backend->compositor->output_list))
				weston_compositor_exit(input->backend->compositor);

			return;
		}
		if (frame_status(output->frame) & FRAME_STATUS_REPAINT)
			weston_output_schedule_repaint(&output->base);
	}

	if (active)
		notify_touch(input->touch_device, &ts, id, 0, 0, WL_TOUCH_UP);
}

static void
input_handle_touch_motion(void *data, struct wl_touch *wl_touch,
                        uint32_t time, int32_t id,
                        wl_fixed_t fixed_x, wl_fixed_t fixed_y)
{
	struct wayland_input *input = data;
	struct wayland_output *output = input->touch_focus;
	int32_t fx, fy;
	double x, y;
	struct timespec ts;

	x = wl_fixed_to_double(fixed_x);
	y = wl_fixed_to_double(fixed_y);
	timespec_from_msec(&ts, time);

	if (!output || !input->touch_active)
		return;

	if (output->frame) {
		frame_interior(output->frame, &fx, &fy, NULL, NULL);
		x -= fx;
		y -= fy;
	}

	weston_output_transform_coordinate(&output->base, x, y, &x, &y);

	notify_touch(input->touch_device, &ts, id, x, y, WL_TOUCH_MOTION);
}

static void
input_handle_touch_frame(void *data, struct wl_touch *wl_touch)
{
	struct wayland_input *input = data;

	if (!input->touch_focus || !input->touch_active)
		return;

	notify_touch_frame(input->touch_device);
}

static void
input_handle_touch_cancel(void *data, struct wl_touch *wl_touch)
{
	struct wayland_input *input = data;

	if (!input->touch_focus || !input->touch_active)
		return;

	notify_touch_cancel(input->touch_device);
}

static const struct wl_touch_listener touch_listener = {
	input_handle_touch_down,
	input_handle_touch_up,
	input_handle_touch_motion,
	input_handle_touch_frame,
	input_handle_touch_cancel,
};


static struct weston_touch_device *
create_touch_device(struct wayland_input *input)
{
	struct weston_touch_device *touch_device;
	char str[128];

	/* manufacture a unique'ish name */
	snprintf(str, sizeof str, "wayland-touch[%u]",
		 wl_proxy_get_id((struct wl_proxy *)input->parent.seat));

	touch_device = weston_touch_create_touch_device(input->base.touch_state,
							str, NULL, NULL);

	return touch_device;
}

static void
input_handle_capabilities(void *data, struct wl_seat *seat,
		          enum wl_seat_capability caps)
{
	struct wayland_input *input = data;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !input->parent.pointer) {
		input->parent.pointer = wl_seat_get_pointer(seat);
		wl_pointer_set_user_data(input->parent.pointer, input);
		wl_pointer_add_listener(input->parent.pointer,
					&pointer_listener, input);
		weston_seat_init_pointer(&input->base);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && input->parent.pointer) {
		if (input->seat_version >= WL_POINTER_RELEASE_SINCE_VERSION)
			wl_pointer_release(input->parent.pointer);
		else
			wl_pointer_destroy(input->parent.pointer);
		input->parent.pointer = NULL;
		weston_seat_release_pointer(&input->base);
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !input->parent.keyboard) {
		input->parent.keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_set_user_data(input->parent.keyboard, input);
		wl_keyboard_add_listener(input->parent.keyboard,
					 &keyboard_listener, input);
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && input->parent.keyboard) {
		if (input->seat_version >= WL_KEYBOARD_RELEASE_SINCE_VERSION)
			wl_keyboard_release(input->parent.keyboard);
		else
			wl_keyboard_destroy(input->parent.keyboard);
		input->parent.keyboard = NULL;
		weston_seat_release_keyboard(&input->base);
	}

	if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !input->parent.touch) {
		input->parent.touch = wl_seat_get_touch(seat);
		wl_touch_set_user_data(input->parent.touch, input);
		wl_touch_add_listener(input->parent.touch,
				      &touch_listener, input);
		weston_seat_init_touch(&input->base);
		input->touch_device = create_touch_device(input);
	} else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && input->parent.touch) {
		weston_touch_device_destroy(input->touch_device);
		input->touch_device = NULL;
		if (input->seat_version >= WL_TOUCH_RELEASE_SINCE_VERSION)
			wl_touch_release(input->parent.touch);
		else
			wl_touch_destroy(input->parent.touch);
		input->parent.touch = NULL;
		weston_seat_release_touch(&input->base);
	}
}

static void
input_handle_name(void *data, struct wl_seat *seat,
		  const char *name)
{
}

static const struct wl_seat_listener seat_listener = {
	input_handle_capabilities,
	input_handle_name,
};

static void
display_add_seat(struct wayland_backend *b, uint32_t id, uint32_t available_version)
{
	struct wayland_input *input;
	uint32_t version = MIN(available_version, 4);

	input = zalloc(sizeof *input);
	if (input == NULL)
		return;

	weston_seat_init(&input->base, b->compositor, "default");
	input->backend = b;
	input->parent.seat = wl_registry_bind(b->parent.registry, id,
					      &wl_seat_interface, version);
	input->seat_version = version;
	wl_list_insert(b->input_list.prev, &input->link);

	wl_seat_add_listener(input->parent.seat, &seat_listener, input);
	wl_seat_set_user_data(input->parent.seat, input);

	input->parent.cursor.surface =
		wl_compositor_create_surface(b->parent.compositor);

	input->vert.axis = WL_POINTER_AXIS_VERTICAL_SCROLL;
	input->horiz.axis = WL_POINTER_AXIS_HORIZONTAL_SCROLL;
}

static void
wayland_parent_output_geometry(void *data, struct wl_output *output_proxy,
			       int32_t x, int32_t y,
			       int32_t physical_width, int32_t physical_height,
			       int32_t subpixel, const char *make,
			       const char *model, int32_t transform)
{
	struct wayland_parent_output *output = data;

	output->x = x;
	output->y = y;
	output->physical.width = physical_width;
	output->physical.height = physical_height;
	output->physical.subpixel = subpixel;

	free(output->physical.make);
	output->physical.make = strdup(make);
	free(output->physical.model);
	output->physical.model = strdup(model);

	output->transform = transform;
}

static struct weston_mode *
find_mode(struct wl_list *list, int32_t width, int32_t height, uint32_t refresh)
{
	struct weston_mode *mode;

	wl_list_for_each(mode, list, link) {
		if (mode->width == width && mode->height == height &&
		    mode->refresh == refresh)
			return mode;
	}

	mode = zalloc(sizeof *mode);
	if (!mode)
		return NULL;

	mode->width = width;
	mode->height = height;
	mode->refresh = refresh;
	wl_list_insert(list, &mode->link);

	return mode;
}

static struct weston_output *
wayland_parent_output_get_enabled_output(struct wayland_parent_output *poutput)
{
	struct wayland_head *head = poutput->head;

	if (!head)
		return NULL;

	if (!weston_head_is_enabled(&head->base))
		return NULL;

	return weston_head_get_output(&head->base);
}

static void
wayland_parent_output_mode(void *data, struct wl_output *wl_output_proxy,
			   uint32_t flags, int32_t width, int32_t height,
			   int32_t refresh)
{
	struct wayland_parent_output *output = data;
	struct weston_output *enabled_output;
	struct weston_mode *mode;

	enabled_output = wayland_parent_output_get_enabled_output(output);
	if (enabled_output) {
		mode = find_mode(&enabled_output->mode_list,
				 width, height, refresh);
		if (!mode)
			return;
		mode->flags = flags;
		/* Do a mode-switch on current mode change? */
	} else {
		mode = find_mode(&output->mode_list, width, height, refresh);
		if (!mode)
			return;
		mode->flags = flags;
		if (flags & WL_OUTPUT_MODE_CURRENT)
			output->current_mode = mode;
		if (flags & WL_OUTPUT_MODE_PREFERRED)
			output->preferred_mode = mode;
	}
}

static const struct wl_output_listener output_listener = {
	wayland_parent_output_geometry,
	wayland_parent_output_mode
};

static void
output_sync_callback(void *data, struct wl_callback *callback, uint32_t unused)
{
	struct wayland_parent_output *output = data;

	assert(output->sync_cb == callback);
	wl_callback_destroy(callback);
	output->sync_cb = NULL;

	assert(output->backend->sprawl_across_outputs);

	wayland_head_create_for_parent_output(output->backend->compositor, output);
}

static const struct wl_callback_listener output_sync_listener = {
	output_sync_callback
};

static void
wayland_backend_register_output(struct wayland_backend *b, uint32_t id)
{
	struct wayland_parent_output *output;

	output = zalloc(sizeof *output);
	if (!output)
		return;

	output->backend = b;
	output->id = id;
	output->global = wl_registry_bind(b->parent.registry, id,
					  &wl_output_interface, 1);
	if (!output->global) {
		free(output);
		return;
	}

	wl_output_add_listener(output->global, &output_listener, output);

	output->scale = 0;
	output->transform = WL_OUTPUT_TRANSFORM_NORMAL;
	output->physical.subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
	wl_list_init(&output->mode_list);
	wl_list_insert(&b->parent.output_list, &output->link);

	if (b->sprawl_across_outputs) {
		output->sync_cb = wl_display_sync(b->parent.wl_display);
		wl_callback_add_listener(output->sync_cb,
					 &output_sync_listener, output);
	}
}

static void
wayland_parent_output_destroy(struct wayland_parent_output *output)
{
	struct weston_mode *mode, *next;

	if (output->sync_cb)
		wl_callback_destroy(output->sync_cb);

	if (output->head)
		wayland_head_destroy(output->head);

	wl_output_destroy(output->global);
	free(output->physical.make);
	free(output->physical.model);

	wl_list_for_each_safe(mode, next, &output->mode_list, link) {
		wl_list_remove(&mode->link);
		free(mode);
	}

	wl_list_remove(&output->link);
	free(output);
}

static void
xdg_wm_base_ping(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
	xdg_wm_base_pong(shell, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = {
	xdg_wm_base_ping,
};

static void
registry_handle_global(void *data, struct wl_registry *registry, uint32_t name,
		       const char *interface, uint32_t version)
{
	struct wayland_backend *b = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		b->parent.compositor =
			wl_registry_bind(registry, name,
					 &wl_compositor_interface,
					 MIN(version, 4));
	} else if (strcmp(interface, "xdg_wm_base") == 0) {
		b->parent.xdg_wm_base =
			wl_registry_bind(registry, name,
					 &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(b->parent.xdg_wm_base,
					 &wm_base_listener, b);
	} else if (strcmp(interface, "wl_shell") == 0) {
		b->parent.shell =
			wl_registry_bind(registry, name,
					 &wl_shell_interface, 1);
	} else if (strcmp(interface, "zwp_fullscreen_shell_v1") == 0) {
		b->parent.fshell =
			wl_registry_bind(registry, name,
					 &zwp_fullscreen_shell_v1_interface, 1);
	} else if (strcmp(interface, "wl_seat") == 0) {
		display_add_seat(b, name, version);
	} else if (strcmp(interface, "wl_output") == 0) {
		wayland_backend_register_output(b, name);
	} else if (strcmp(interface, "wl_shm") == 0) {
		b->parent.shm =
			wl_registry_bind(registry, name, &wl_shm_interface, 1);
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
	struct wayland_backend *b = data;
	struct wayland_parent_output *output, *next;

	wl_list_for_each_safe(output, next, &b->parent.output_list, link)
		if (output->id == name)
			wayland_parent_output_destroy(output);
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

static int
wayland_backend_handle_event(int fd, uint32_t mask, void *data)
{
	struct wayland_backend *b = data;
	int count = 0;

	if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR)) {
		weston_compositor_exit(b->compositor);
		return 0;
	}

	if (mask & WL_EVENT_READABLE)
		count = wl_display_dispatch(b->parent.wl_display);
	if (mask & WL_EVENT_WRITABLE)
		wl_display_flush(b->parent.wl_display);

	if (mask == 0) {
		count = wl_display_dispatch_pending(b->parent.wl_display);
		wl_display_flush(b->parent.wl_display);
	}

	return count;
}

static void
wayland_destroy(struct weston_compositor *ec)
{
	struct wayland_backend *b = to_wayland_backend(ec);
	struct weston_head *base, *next;

	wl_event_source_remove(b->parent.wl_source);

	weston_compositor_shutdown(ec);

	wl_list_for_each_safe(base, next, &ec->head_list, compositor_link)
		wayland_head_destroy(to_wayland_head(base));

	if (b->parent.shm)
		wl_shm_destroy(b->parent.shm);

	if (b->parent.xdg_wm_base)
		xdg_wm_base_destroy(b->parent.xdg_wm_base);

	if (b->parent.shell)
		wl_shell_destroy(b->parent.shell);

	if (b->parent.fshell)
		zwp_fullscreen_shell_v1_release(b->parent.fshell);

	if (b->parent.compositor)
		wl_compositor_destroy(b->parent.compositor);

	if (b->theme)
		theme_destroy(b->theme);

	if (b->frame_device)
		cairo_device_destroy(b->frame_device);

	wl_cursor_theme_destroy(b->cursor_theme);

	wl_registry_destroy(b->parent.registry);
	wl_display_flush(b->parent.wl_display);
	wl_display_disconnect(b->parent.wl_display);

	free(b);
}

static const char *left_ptrs[] = {
	"left_ptr",
	"default",
	"top_left_arrow",
	"left-arrow"
};

static void
create_cursor(struct wayland_backend *b,
	      struct weston_wayland_backend_config *config)
{
	unsigned int i;

	b->cursor_theme = wl_cursor_theme_load(config->cursor_theme,
					       config->cursor_size,
					       b->parent.shm);
	if (!b->cursor_theme) {
		fprintf(stderr, "could not load cursor theme\n");
		return;
	}

	b->cursor = NULL;
	for (i = 0; !b->cursor && i < ARRAY_LENGTH(left_ptrs); ++i)
		b->cursor = wl_cursor_theme_get_cursor(b->cursor_theme,
						       left_ptrs[i]);
	if (!b->cursor) {
		fprintf(stderr, "could not load left cursor\n");
		return;
	}
}

static void
fullscreen_binding(struct weston_keyboard *keyboard,
		   const struct timespec *time, uint32_t key, void *data)
{
	struct wayland_backend *b = data;
	struct wayland_input *input = NULL;

	wl_list_for_each(input, &b->input_list, link)
		if (&input->base == keyboard->seat)
			break;

	if (!input || !input->output)
		return;

	if (input->output->frame)
		wayland_output_set_fullscreen(input->output, 0, 0, NULL);
	else
		wayland_output_set_windowed(input->output);

	weston_output_schedule_repaint(&input->output->base);
}

static struct wayland_backend *
wayland_backend_create(struct weston_compositor *compositor,
		       struct weston_wayland_backend_config *new_config)
{
	struct wayland_backend *b;
	struct wl_event_loop *loop;
	int fd;

	b = zalloc(sizeof *b);
	if (b == NULL)
		return NULL;

	b->compositor = compositor;
	compositor->backend = &b->base;

	if (weston_compositor_set_presentation_clock_software(compositor) < 0)
		goto err_compositor;

	b->parent.wl_display = wl_display_connect(new_config->display_name);
	if (b->parent.wl_display == NULL) {
		weston_log("Error: Failed to connect to parent Wayland compositor: %s\n",
			   strerror(errno));
		weston_log_continue(STAMP_SPACE "display option: %s, WAYLAND_DISPLAY=%s\n",
				    new_config->display_name ?: "(none)",
				    getenv("WAYLAND_DISPLAY") ?: "(not set)");
		goto err_compositor;
	}

	wl_list_init(&b->parent.output_list);
	wl_list_init(&b->input_list);
	b->parent.registry = wl_display_get_registry(b->parent.wl_display);
	wl_registry_add_listener(b->parent.registry, &registry_listener, b);
	wl_display_roundtrip(b->parent.wl_display);

	create_cursor(b, new_config);

#ifdef ENABLE_EGL
	b->use_pixman = new_config->use_pixman;
#else
	b->use_pixman = true;
#endif
	b->fullscreen = new_config->fullscreen;

	if (!b->use_pixman) {
		gl_renderer = weston_load_module("gl-renderer.so",
						 "gl_renderer_interface");
		if (!gl_renderer)
			b->use_pixman = true;
	}

	if (!b->use_pixman) {
		if (gl_renderer->display_create(compositor,
						EGL_PLATFORM_WAYLAND_KHR,
						b->parent.wl_display,
						NULL,
						gl_renderer->alpha_attribs,
						NULL,
						0) < 0) {
			weston_log("Failed to initialize the GL renderer; "
				   "falling back to pixman.\n");
			b->use_pixman = true;
		}
	}

	if (b->use_pixman) {
		if (pixman_renderer_init(compositor) < 0) {
			weston_log("Failed to initialize pixman renderer\n");
			goto err_display;
		}
	}

	b->base.destroy = wayland_destroy;
	b->base.create_output = wayland_output_create;

	loop = wl_display_get_event_loop(compositor->wl_display);

	fd = wl_display_get_fd(b->parent.wl_display);
	b->parent.wl_source =
		wl_event_loop_add_fd(loop, fd, WL_EVENT_READABLE,
				     wayland_backend_handle_event, b);
	if (b->parent.wl_source == NULL)
		goto err_display;

	wl_event_source_check(b->parent.wl_source);

	if (compositor->renderer->import_dmabuf) {
		if (linux_dmabuf_setup(compositor) < 0)
			weston_log("Error: initializing dmabuf "
			           "support failed.\n");
	}

	return b;
err_display:
	wl_display_disconnect(b->parent.wl_display);
err_compositor:
	weston_compositor_shutdown(compositor);
	free(b);
	return NULL;
}

static void
wayland_backend_destroy(struct wayland_backend *b)
{
	wl_display_disconnect(b->parent.wl_display);

	if (b->theme)
		theme_destroy(b->theme);
	if (b->frame_device)
		cairo_device_destroy(b->frame_device);
	wl_cursor_theme_destroy(b->cursor_theme);

	weston_compositor_shutdown(b->compositor);
	free(b);
}

static const struct weston_windowed_output_api windowed_api = {
	wayland_output_set_size,
	wayland_head_create_windowed,
};

static void
config_init_to_defaults(struct weston_wayland_backend_config *config)
{
}

WL_EXPORT int
weston_backend_init(struct weston_compositor *compositor,
		    struct weston_backend_config *config_base)
{
	struct wayland_backend *b;
	struct wayland_parent_output *poutput;
	struct weston_wayland_backend_config new_config;
	int ret;

	if (config_base == NULL ||
	    config_base->struct_version != WESTON_WAYLAND_BACKEND_CONFIG_VERSION ||
	    config_base->struct_size > sizeof(struct weston_wayland_backend_config)) {
		weston_log("wayland backend config structure is invalid\n");
		return -1;
	}

	config_init_to_defaults(&new_config);
	memcpy(&new_config, config_base, config_base->struct_size);

	b = wayland_backend_create(compositor, &new_config);

	if (!b)
		return -1;

	if (new_config.sprawl || b->parent.fshell) {
		b->sprawl_across_outputs = true;
		wl_display_roundtrip(b->parent.wl_display);

		wl_list_for_each(poutput, &b->parent.output_list, link)
			wayland_head_create_for_parent_output(compositor, poutput);

		return 0;
	}

	if (new_config.fullscreen) {
		if (!wayland_head_create(compositor, "wayland-fullscreen")) {
			weston_log("Unable to create a fullscreen head.\n");
			goto err_outputs;
		}

		return 0;
	}

	ret = weston_plugin_api_register(compositor, WESTON_WINDOWED_OUTPUT_API_NAME,
					 &windowed_api, sizeof(windowed_api));

	if (ret < 0) {
		weston_log("Failed to register output API.\n");
		wayland_backend_destroy(b);
		return -1;
	}

	weston_compositor_add_key_binding(compositor, KEY_F,
				          MODIFIER_CTRL | MODIFIER_ALT,
				          fullscreen_binding, b);
	return 0;

err_outputs:
	wayland_backend_destroy(b);
	return -1;
}
