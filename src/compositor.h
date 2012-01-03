/*
 * Copyright © 2008-2011 Kristian Høgsberg
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

#ifndef _WAYLAND_SYSTEM_COMPOSITOR_H_
#define _WAYLAND_SYSTEM_COMPOSITOR_H_

#include <libudev.h>
#include <pixman.h>
#include <wayland-server.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

struct weston_matrix {
	GLfloat d[16];
};

struct weston_vector {
	GLfloat f[4];
};

void
weston_matrix_init(struct weston_matrix *matrix);
void
weston_matrix_scale(struct weston_matrix *matrix, GLfloat x, GLfloat y, GLfloat z);
void
weston_matrix_translate(struct weston_matrix *matrix,
			GLfloat x, GLfloat y, GLfloat z);
void
weston_matrix_transform(struct weston_matrix *matrix, struct weston_vector *v);

struct weston_transform {
	struct weston_matrix matrix;
	struct weston_matrix inverse;
};

struct weston_surface;
struct weston_input_device;

struct weston_mode {
	uint32_t flags;
	int32_t width, height;
	uint32_t refresh;
	struct wl_list link;
};

struct weston_output {
	struct wl_list link;
	struct weston_compositor *compositor;
	struct weston_matrix matrix;
	struct wl_list frame_callback_list;
	int32_t x, y, mm_width, mm_height;
	pixman_region32_t region;
	pixman_region32_t previous_damage;
	uint32_t flags;
	int repaint_needed;
	int repaint_scheduled;

	char *make, *model;
	uint32_t subpixel;
	
	struct weston_mode *current;
	struct wl_list mode_list;
	struct wl_buffer *scanout_buffer;
	struct wl_listener scanout_buffer_destroy_listener;
	struct wl_buffer *pending_scanout_buffer;
	struct wl_listener pending_scanout_buffer_destroy_listener;

	int (*prepare_render)(struct weston_output *output);
	int (*present)(struct weston_output *output);
	int (*prepare_scanout_surface)(struct weston_output *output,
				       struct weston_surface *es);
	int (*set_hardware_cursor)(struct weston_output *output,
				   struct weston_input_device *input);
	void (*destroy)(struct weston_output *output);
};

struct weston_input_device {
	struct wl_input_device input_device;
	struct weston_compositor *compositor;
	struct weston_surface *sprite;
	int32_t hotspot_x, hotspot_y;
	struct wl_list link;
	uint32_t modifier_state;

	struct wl_list drag_resource_list;
	struct weston_data_source *drag_data_source;
	struct wl_surface *drag_focus;
	struct wl_resource *drag_focus_resource;
	struct wl_listener drag_focus_listener;

	struct weston_data_source *selection_data_source;
	struct wl_listener selection_data_source_listener;
	struct wl_grab grab;

	uint32_t num_tp;
	struct wl_surface *touch_focus;
	struct wl_listener touch_focus_listener;
	struct wl_resource *touch_focus_resource;
	struct wl_listener touch_focus_resource_listener;
};

enum weston_visual {
	WESTON_NONE_VISUAL,
	WESTON_ARGB_VISUAL,
	WESTON_PREMUL_ARGB_VISUAL,
	WESTON_RGB_VISUAL
};

struct weston_shader {
	GLuint program;
	GLuint vertex_shader, fragment_shader;
	GLuint proj_uniform;
	GLuint tex_uniform;
	GLuint alpha_uniform;
	GLuint color_uniform;
};

struct weston_animation {
	void (*frame)(struct weston_animation *animation,
		      struct weston_output *output, uint32_t msecs);
	struct wl_list link;
};

struct weston_spring {
	double k;
	double friction;
	double current;
	double target;
	double previous;
	uint32_t timestamp;
};

struct weston_shell {
	void (*lock)(struct weston_shell *shell);
	void (*unlock)(struct weston_shell *shell);
	void (*map)(struct weston_shell *shell, struct weston_surface *surface,
		    int32_t width, int32_t height);
	void (*configure)(struct weston_shell *shell,
			  struct weston_surface *surface,
			  int32_t x, int32_t y, int32_t width, int32_t height);
	void (*destroy)(struct weston_shell *shell);
};

enum {
	WESTON_COMPOSITOR_ACTIVE,
	WESTON_COMPOSITOR_IDLE,		/* shell->unlock called on activity */
	WESTON_COMPOSITOR_SLEEPING	/* no rendering, no frame events */
};

struct screenshooter;

struct weston_compositor {
	struct wl_shm *shm;
	struct weston_xserver *wxs;

	EGLDisplay display;
	EGLContext context;
	EGLConfig config;
	GLuint fbo;
	GLuint proj_uniform, tex_uniform, alpha_uniform;
	uint32_t current_alpha;
	struct weston_shader texture_shader;
	struct weston_shader solid_shader;
	struct wl_display *wl_display;

	struct weston_shell *shell;

	/* There can be more than one, but not right now... */
	struct wl_input_device *input_device;

	struct wl_list output_list;
	struct wl_list input_device_list;
	struct wl_list surface_list;
	struct wl_list binding_list;
	struct wl_list animation_list;
	struct {
		struct weston_spring spring;
		struct weston_animation animation;
	} fade;

	uint32_t state;
	struct wl_event_source *idle_source;
	uint32_t idle_inhibit;
	int option_idle_time;		/* default timeout, s */
	int idle_time;			/* effective timeout, s */

	/* Repaint state. */
	struct timespec previous_swap;
	struct wl_array vertices, indices;

	struct weston_surface *overlay;
	struct weston_switcher *switcher;
	uint32_t focus;

	PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC
		image_target_renderbuffer_storage;
	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_texture_2d;
	PFNEGLCREATEIMAGEKHRPROC create_image;
	PFNEGLDESTROYIMAGEKHRPROC destroy_image;
	PFNEGLBINDWAYLANDDISPLAYWL bind_display;
	PFNEGLUNBINDWAYLANDDISPLAYWL unbind_display;
	int has_bind_display;

	void (*destroy)(struct weston_compositor *ec);
	int (*authenticate)(struct weston_compositor *c, uint32_t id);
	EGLImageKHR (*create_cursor_image)(struct weston_compositor *c,
					   int32_t *width, int32_t *height);

	struct screenshooter *screenshooter;
};

#define MODIFIER_CTRL	(1 << 8)
#define MODIFIER_ALT	(1 << 9)
#define MODIFIER_SUPER	(1 << 10)

enum weston_output_flags {
	WL_OUTPUT_FLIPPED = 0x01
};

struct weston_surface {
	struct wl_surface surface;
	struct weston_compositor *compositor;
	GLuint texture, saved_texture;
	pixman_region32_t damage;
	pixman_region32_t opaque;
	int32_t x, y, width, height;
	int32_t pitch;
	struct wl_list link;
	struct wl_list buffer_link;
	struct weston_transform *transform;
	uint32_t alpha;
	uint32_t visual;

	/*
	 * Which output to vsync this surface to.
	 * Used to determine, whether to send or queue frame events.
	 * Must be NULL, if 'link' is not in weston_compositor::surface_list.
	 */
	struct weston_output *output;

	struct weston_output *fullscreen_output;
	struct wl_list frame_callback_list;

	EGLImageKHR image;

	struct wl_buffer *buffer;
	struct wl_listener buffer_destroy_listener;
};

struct weston_data_offer {
	struct wl_resource resource;
	struct weston_data_source *source;
	struct wl_listener source_destroy_listener;
};

struct weston_data_source {
	struct wl_resource resource;
	struct wl_array mime_types;

	const struct wl_data_offer_interface *offer_interface;
	void (*cancel)(struct weston_data_source *source);
};

void
weston_data_source_unref(struct weston_data_source *source);

void
weston_input_device_set_selection(struct weston_input_device *device,
				  struct weston_data_source *source,
				  uint32_t time);

void
weston_spring_init(struct weston_spring *spring,
		   double k, double current, double target);
void
weston_spring_update(struct weston_spring *spring, uint32_t msec);
int
weston_spring_done(struct weston_spring *spring);

void
weston_surface_activate(struct weston_surface *surface,
			struct weston_input_device *device, uint32_t time);

void
notify_motion(struct wl_input_device *device,
	      uint32_t time, int x, int y);
void
notify_button(struct wl_input_device *device,
	      uint32_t time, int32_t button, int32_t state);
void
notify_key(struct wl_input_device *device,
	   uint32_t time, uint32_t key, uint32_t state);

void
notify_pointer_focus(struct wl_input_device *device,
		     uint32_t time,
		     struct weston_output *output,
		     int32_t x, int32_t y);

void
notify_keyboard_focus(struct wl_input_device *device,
		      uint32_t time, struct weston_output *output,
		      struct wl_array *keys);

void
notify_touch(struct wl_input_device *device, uint32_t time, int touch_id,
	     int x, int y, int touch_type);

void
weston_output_finish_frame(struct weston_output *output, int msecs);
void
weston_output_damage(struct weston_output *output);
void
weston_compositor_repick(struct weston_compositor *compositor);
void
weston_compositor_schedule_repaint(struct weston_compositor *compositor);
void
weston_compositor_fade(struct weston_compositor *compositor, float tint);
void
weston_compositor_damage_all(struct weston_compositor *compositor);
void
weston_compositor_unlock(struct weston_compositor *compositor);
void
weston_compositor_wake(struct weston_compositor *compositor);
void
weston_compositor_activity(struct weston_compositor *compositor);

struct weston_binding;
typedef void (*weston_binding_handler_t)(struct wl_input_device *device,
					 uint32_t time, uint32_t key,
					 uint32_t button,
					 uint32_t state, void *data);
struct weston_binding *
weston_compositor_add_binding(struct weston_compositor *compositor,
			      uint32_t key, uint32_t button, uint32_t modifier,
			      weston_binding_handler_t binding, void *data);
void
weston_binding_destroy(struct weston_binding *binding);

void
weston_binding_list_destroy_all(struct wl_list *list);

void
weston_compositor_run_binding(struct weston_compositor *compositor,
			      struct weston_input_device *device,
			      uint32_t time,
			      uint32_t key, uint32_t button, int32_t state);

struct weston_surface *
weston_surface_create(struct weston_compositor *compositor,
		      int32_t x, int32_t y, int32_t width, int32_t height);

void
weston_surface_configure(struct weston_surface *surface,
			 int x, int y, int width, int height);

void
weston_surface_assign_output(struct weston_surface *surface);

void
weston_surface_damage(struct weston_surface *surface);

void
weston_surface_damage_below(struct weston_surface *surface);

void
weston_surface_damage_rectangle(struct weston_surface *surface,
				int32_t x, int32_t y,
				int32_t width, int32_t height);

struct weston_surface *
pick_surface(struct wl_input_device *device, int32_t *sx, int32_t *sy);

uint32_t
weston_compositor_get_time(void);

int
weston_compositor_init(struct weston_compositor *ec, struct wl_display *display);
void
weston_compositor_shutdown(struct weston_compositor *ec);
void
weston_output_move(struct weston_output *output, int x, int y);
void
weston_output_init(struct weston_output *output, struct weston_compositor *c,
		   int x, int y, int width, int height, uint32_t flags);
void
weston_output_destroy(struct weston_output *output);

void
weston_input_device_init(struct weston_input_device *device,
			 struct weston_compositor *ec);

void
weston_input_device_release(struct weston_input_device *device);

void
weston_switcher_init(struct weston_compositor *compositor);

enum {
	TTY_ENTER_VT,
	TTY_LEAVE_VT
};

typedef void (*tty_vt_func_t)(struct weston_compositor *compositor, int event);

struct tty *
tty_create(struct weston_compositor *compositor,
	   tty_vt_func_t vt_func, int tty_nr);

void
tty_destroy(struct tty *tty);

struct screenshooter *
screenshooter_create(struct weston_compositor *ec);

void
screenshooter_destroy(struct screenshooter *s);

uint32_t *
weston_load_image(const char *filename,
		  int32_t *width_arg, int32_t *height_arg,
		  uint32_t *stride_arg);

struct weston_process;
typedef void (*weston_process_cleanup_func_t)(struct weston_process *process,
					    int status);

struct weston_process {
	pid_t pid;
	weston_process_cleanup_func_t cleanup;
	struct wl_list link;
};

struct wl_client *
weston_client_launch(struct weston_compositor *compositor,
		     struct weston_process *proc,
		     const char *path,
		     weston_process_cleanup_func_t cleanup);

int
weston_data_device_manager_init(struct weston_compositor *compositor);
void
weston_data_device_set_keyboard_focus(struct weston_input_device *device);

void
weston_watch_process(struct weston_process *process);

int
weston_xserver_init(struct weston_compositor *compositor);
void
weston_xserver_destroy(struct weston_compositor *compositor);
void
weston_xserver_surface_activate(struct weston_surface *surface);
void
weston_xserver_set_selection(struct weston_input_device *device);

struct weston_zoom;
typedef	void (*weston_zoom_done_func_t)(struct weston_zoom *zoom, void *data);

struct weston_zoom *
weston_zoom_run(struct weston_surface *surface, GLfloat start, GLfloat stop,
		weston_zoom_done_func_t done, void *data);

#endif
