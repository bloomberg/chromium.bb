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

#ifndef _WAYLAND_SYSTEM_COMPOSITOR_H_
#define _WAYLAND_SYSTEM_COMPOSITOR_H_

#include <pixman.h>
#include <xkbcommon/xkbcommon.h>
#include <wayland-server.h>

#include "version.h"
#include "matrix.h"
#include "config-parser.h"

#define ARRAY_LENGTH(a) (sizeof (a) / sizeof (a)[0])

#define container_of(ptr, type, member) ({				\
	const __typeof__( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

struct weston_transform {
	struct weston_matrix matrix;
	struct wl_list link;
};

struct weston_surface;
struct shell_surface;
struct weston_seat;
struct weston_output;
struct input_method;

enum weston_keyboard_modifier {
	MODIFIER_CTRL = (1 << 0),
	MODIFIER_ALT = (1 << 1),
	MODIFIER_SUPER = (1 << 2),
	MODIFIER_SHIFT = (1 << 3),
};

enum weston_led {
	LED_NUM_LOCK = (1 << 0),
	LED_CAPS_LOCK = (1 << 1),
	LED_SCROLL_LOCK = (1 << 2),
};

struct weston_mode {
	uint32_t flags;
	int32_t width, height;
	uint32_t refresh;
	struct wl_list link;
};

struct weston_shell_client {
	void (*send_configure)(struct weston_surface *surface,
			       uint32_t edges, int32_t width, int32_t height);
};

struct weston_shell_interface {
	void *shell;			/* either desktop or tablet */

	struct shell_surface *(*create_shell_surface)(void *shell,
						      struct weston_surface *surface,
						      const struct weston_shell_client *client);

	void (*set_toplevel)(struct shell_surface *shsurf);

	void (*set_transient)(struct shell_surface *shsurf,
			      struct weston_surface *parent,
			      int x, int y, uint32_t flags);
	void (*set_fullscreen)(struct shell_surface *shsurf,
			       uint32_t method,
			       uint32_t framerate,
			       struct weston_output *output);
	int (*move)(struct shell_surface *shsurf, struct weston_seat *ws);
	int (*resize)(struct shell_surface *shsurf,
		      struct weston_seat *ws, uint32_t edges);

};

struct weston_border {
	int32_t left, right, top, bottom;
};

struct weston_animation {
	void (*frame)(struct weston_animation *animation,
		      struct weston_output *output, uint32_t msecs);
	int frame_counter;
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

enum {
	ZOOM_FOCUS_POINTER,
	ZOOM_FOCUS_TEXT
};

struct weston_fixed_point {
	wl_fixed_t x, y;
};

struct weston_output_zoom {
	int active;
	uint32_t type;
	float increment;
	float level;
	float max_level;
	float trans_x, trans_y;
	struct weston_animation animation_z;
	struct weston_spring spring_z;
	struct weston_animation animation_xy;
	struct weston_spring spring_xy;
	struct weston_fixed_point from;
	struct weston_fixed_point to;
	struct weston_fixed_point current;
	struct weston_fixed_point text_cursor;
};

/* bit compatible with drm definitions. */
enum dpms_enum {
	WESTON_DPMS_ON,
	WESTON_DPMS_STANDBY,
	WESTON_DPMS_SUSPEND,
	WESTON_DPMS_OFF
};

struct weston_output {
	uint32_t id;

	void *renderer_state;

	struct wl_list link;
	struct wl_list resource_list;
	struct wl_global *global;
	struct weston_compositor *compositor;
	struct weston_matrix matrix;
	struct wl_list animation_list;
	int32_t x, y, width, height;
	int32_t mm_width, mm_height;
	struct weston_border border;
	pixman_region32_t region;
	pixman_region32_t previous_damage;
	int repaint_needed;
	int repaint_scheduled;
	struct weston_output_zoom zoom;
	int dirty;
	struct wl_signal frame_signal;
	uint32_t frame_time;
	int disable_planes;

	char *make, *model;
	uint32_t subpixel;
	uint32_t transform;
	
	struct weston_mode *current;
	struct weston_mode *origin;
	struct wl_list mode_list;

	void (*repaint)(struct weston_output *output,
			pixman_region32_t *damage);
	void (*destroy)(struct weston_output *output);
	void (*assign_planes)(struct weston_output *output);
	int (*switch_mode)(struct weston_output *output, struct weston_mode *mode);

	/* backlight values are on 0-255 range, where higher is brighter */
	uint32_t backlight_current;
	void (*set_backlight)(struct weston_output *output, uint32_t value);
	void (*set_dpms)(struct weston_output *output, enum dpms_enum level);
};

struct weston_xkb_info {
	struct xkb_keymap *keymap;
	int keymap_fd;
	size_t keymap_size;
	char *keymap_area;
	xkb_mod_index_t shift_mod;
	xkb_mod_index_t caps_mod;
	xkb_mod_index_t ctrl_mod;
	xkb_mod_index_t alt_mod;
	xkb_mod_index_t mod2_mod;
	xkb_mod_index_t mod3_mod;
	xkb_mod_index_t super_mod;
	xkb_mod_index_t mod5_mod;
	xkb_led_index_t num_led;
	xkb_led_index_t caps_led;
	xkb_led_index_t scroll_led;
};

struct weston_keyboard {
	struct wl_keyboard keyboard;

	struct wl_keyboard_grab input_method_grab;
	struct wl_resource *input_method_resource;
};

struct weston_seat {
	struct wl_seat seat;
	struct wl_pointer pointer;
	int has_pointer;
	struct weston_keyboard keyboard;
	int has_keyboard;
	struct wl_touch touch;
	int has_touch;

	struct weston_compositor *compositor;
	struct weston_surface *sprite;
	struct wl_listener sprite_destroy_listener;
	struct weston_surface *drag_surface;
	struct wl_listener drag_surface_destroy_listener;
	int32_t hotspot_x, hotspot_y;
	struct wl_list link;
	enum weston_keyboard_modifier modifier_state;
	struct wl_surface *saved_kbd_focus;
	struct wl_listener saved_kbd_focus_listener;

	uint32_t num_tp;

	struct wl_listener new_drag_icon_listener;

	void (*led_update)(struct weston_seat *ws, enum weston_led leds);

	struct weston_xkb_info xkb_info;
	struct {
		struct xkb_state *state;
		enum weston_led leds;
	} xkb_state;

	struct input_method *input_method;
};

enum {
	WESTON_COMPOSITOR_ACTIVE,
	WESTON_COMPOSITOR_IDLE,		/* shell->unlock called on activity */
	WESTON_COMPOSITOR_SLEEPING	/* no rendering, no frame events */
};

struct weston_layer {
	struct wl_list surface_list;
	struct wl_list link;
};

struct weston_plane {
	pixman_region32_t damage;
	pixman_region32_t opaque;
	int32_t x, y;
};

struct weston_renderer {
	int (*read_pixels)(struct weston_output *output,
			       pixman_format_code_t format, void *pixels,
			       uint32_t x, uint32_t y,
			       uint32_t width, uint32_t height);
	void (*repaint_output)(struct weston_output *output,
			       pixman_region32_t *output_damage);
	void (*flush_damage)(struct weston_surface *surface);
	void (*attach)(struct weston_surface *es, struct wl_buffer *buffer);
	int (*create_surface)(struct weston_surface *surface);
	void (*surface_set_color)(struct weston_surface *surface,
			       float red, float green,
			       float blue, float alpha);
	void (*destroy_surface)(struct weston_surface *surface);
	void (*destroy)(struct weston_compositor *ec);
};

struct weston_compositor {
	struct wl_signal destroy_signal;

	struct wl_display *wl_display;
	struct weston_shell_interface shell_interface;

	struct wl_signal activate_signal;
	struct wl_signal kill_signal;
	struct wl_signal lock_signal;
	struct wl_signal unlock_signal;

	struct wl_signal show_input_panel_signal;
	struct wl_signal hide_input_panel_signal;

	struct wl_signal seat_created_signal;

	struct wl_event_loop *input_loop;
	struct wl_event_source *input_loop_source;

	struct weston_layer fade_layer;
	struct weston_layer cursor_layer;

	struct wl_list output_list;
	struct wl_list seat_list;
	struct wl_list layer_list;
	struct wl_list surface_list;
	struct wl_list key_binding_list;
	struct wl_list button_binding_list;
	struct wl_list axis_binding_list;
	struct wl_list debug_binding_list;
	struct {
		struct weston_spring spring;
		struct weston_animation animation;
		struct weston_surface *surface;
	} fade;

	uint32_t state;
	struct wl_event_source *idle_source;
	uint32_t idle_inhibit;
	int option_idle_time;		/* default timeout, s */
	int idle_time;			/* effective timeout, s */

	/* Repaint state. */
	struct wl_array vertices;
	struct wl_array indices; /* only used in compositor-wayland */
	struct wl_array vtxcnt;
	struct weston_plane primary_plane;
	int fan_debug;

	uint32_t focus;

	struct weston_renderer *renderer;

	pixman_format_code_t read_format;

	void (*destroy)(struct weston_compositor *ec);
	void (*restore)(struct weston_compositor *ec);
	int (*authenticate)(struct weston_compositor *c, uint32_t id);

	void (*ping_handler)(struct weston_surface *surface, uint32_t serial);

	int launcher_sock;

	uint32_t output_id_pool;

	struct xkb_rule_names xkb_names;
	struct xkb_context *xkb_context;
	struct weston_xkb_info xkb_info;
};

struct weston_buffer_reference {
	struct wl_buffer *buffer;
	struct wl_listener destroy_listener;
};

struct weston_region {
	struct wl_resource resource;
	pixman_region32_t region;
};

/* Using weston_surface transformations
 *
 * To add a transformation to a surface, create a struct weston_transform, and
 * add it to the list surface->geometry.transformation_list. Whenever you
 * change the list, anything under surface->geometry, or anything in the
 * weston_transforms linked into the list, you must set
 * surface->geometry.dirty = 1.
 *
 * The order in the list defines the order of transformations. Let the list
 * contain the transformation matrices M1, ..., Mn as head to tail. The
 * transformation is applied to surface-local coordinate vector p as
 *    P = Mn * ... * M2 * M1 * p
 * to produce the global coordinate vector P. The total transform
 *    Mn * ... * M2 * M1
 * is cached in surface->transform.matrix, and the inverse of it in
 * surface->transform.inverse.
 *
 * The list always contains surface->transform.position transformation, which
 * is the translation by surface->geometry.x and y.
 *
 * If you want to apply a transformation in local coordinates, add your
 * weston_transform to the head of the list. If you want to apply a
 * transformation in global coordinates, add it to the tail of the list.
 */

struct weston_surface {
	struct wl_surface surface;
	struct weston_compositor *compositor;
	pixman_region32_t clip;
	pixman_region32_t damage;
	pixman_region32_t opaque;
	pixman_region32_t input;
	struct wl_list link;
	struct wl_list layer_link;
	float alpha;
	struct weston_plane *plane;

	void *renderer_state;

	/* Surface geometry state, mutable.
	 * If you change anything, set dirty = 1.
	 * That includes the transformations referenced from the list.
	 */
	struct {
		float x, y; /* surface translation on display */
		int32_t width, height;

		/* struct weston_transform */
		struct wl_list transformation_list;

		int dirty;
	} geometry;

	/* State derived from geometry state, read-only.
	 * This is updated by weston_surface_update_transform().
	 */
	struct {
		pixman_region32_t boundingbox;
		pixman_region32_t opaque;

		/* matrix and inverse are used only if enabled = 1.
		 * If enabled = 0, use x, y, width, height directly.
		 */
		int enabled;
		struct weston_matrix matrix;
		struct weston_matrix inverse;

		struct weston_transform position; /* matrix from x, y */
	} transform;

	/*
	 * Which output to vsync this surface to.
	 * Used to determine, whether to send or queue frame events.
	 * Must be NULL, if 'link' is not in weston_compositor::surface_list.
	 */
	struct weston_output *output;

	/*
	 * A more complete representation of all outputs this surface is
	 * displayed on.
	 */
	uint32_t output_mask;

	struct wl_list frame_callback_list;

	struct weston_buffer_reference buffer_ref;
	uint32_t buffer_transform;
	int keep_buffer; /* bool for backends to prevent early release */

	/* All the pending state, that wl_surface.commit will apply. */
	struct {
		/* wl_surface.attach */
		int remove_contents;
		struct wl_buffer *buffer;
		struct wl_listener buffer_destroy_listener;
		int32_t sx;
		int32_t sy;

		/* wl_surface.damage */
		pixman_region32_t damage;

		/* wl_surface.set_opaque_region */
		pixman_region32_t opaque;

		/* wl_surface.set_input_region */
		pixman_region32_t input;

		/* wl_surface.frame */
		struct wl_list frame_callback_list;

		/* wl_surface.set_buffer_transform */
		uint32_t buffer_transform;
	} pending;

	/*
	 * If non-NULL, this function will be called on surface::attach after
	 * a new buffer has been set up for this surface. The integer params
	 * are the sx and sy paramerters supplied to surface::attach .
	 */
	void (*configure)(struct weston_surface *es, int32_t sx, int32_t sy);
	void *private;
};

enum weston_key_state_update {
	STATE_UPDATE_AUTOMATIC,
	STATE_UPDATE_NONE,
};

void
weston_version(int *major, int *minor, int *micro);

void
weston_surface_update_transform(struct weston_surface *surface);

void
weston_surface_to_global_fixed(struct weston_surface *surface,
			       wl_fixed_t sx, wl_fixed_t sy,
			       wl_fixed_t *x, wl_fixed_t *y);
void
weston_surface_to_global_float(struct weston_surface *surface,
			       float sx, float sy, float *x, float *y);

void
weston_surface_from_global_float(struct weston_surface *surface,
				 float x, float y, float *sx, float *sy);
void
weston_surface_from_global(struct weston_surface *surface,
			   int32_t x, int32_t y, int32_t *sx, int32_t *sy);
void
weston_surface_from_global_fixed(struct weston_surface *surface,
			         wl_fixed_t x, wl_fixed_t y,
			         wl_fixed_t *sx, wl_fixed_t *sy);
int32_t
weston_surface_buffer_width(struct weston_surface *surface);
int32_t
weston_surface_buffer_height(struct weston_surface *surface);

WL_EXPORT void
weston_surface_to_buffer_float(struct weston_surface *surface,
			       float x, float y, float *bx, float *by);
pixman_box32_t
weston_surface_to_buffer_rect(struct weston_surface *surface,
			      pixman_box32_t rect);

void
weston_spring_init(struct weston_spring *spring,
		   double k, double current, double target);
void
weston_spring_update(struct weston_spring *spring, uint32_t msec);
int
weston_spring_done(struct weston_spring *spring);

void
weston_surface_activate(struct weston_surface *surface,
			struct weston_seat *seat);
void
notify_motion(struct weston_seat *seat, uint32_t time,
	      wl_fixed_t x, wl_fixed_t y);
void
notify_button(struct weston_seat *seat, uint32_t time, int32_t button,
	      enum wl_pointer_button_state state);
void
notify_axis(struct weston_seat *seat, uint32_t time, uint32_t axis,
	    wl_fixed_t value);
void
notify_key(struct weston_seat *seat, uint32_t time, uint32_t key,
	   enum wl_keyboard_key_state state,
	   enum weston_key_state_update update_state);
void
notify_modifiers(struct weston_seat *seat, uint32_t serial);

void
notify_pointer_focus(struct weston_seat *seat, struct weston_output *output,
		     wl_fixed_t x, wl_fixed_t y);

void
notify_keyboard_focus_in(struct weston_seat *seat, struct wl_array *keys,
			 enum weston_key_state_update update_state);
void
notify_keyboard_focus_out(struct weston_seat *seat);

void
notify_touch(struct weston_seat *seat, uint32_t time, int touch_id,
	     wl_fixed_t x, wl_fixed_t y, int touch_type);

void
weston_layer_init(struct weston_layer *layer, struct wl_list *below);

void
weston_plane_init(struct weston_plane *plane, int32_t x, int32_t y);
void
weston_plane_release(struct weston_plane *plane);

void
weston_output_finish_frame(struct weston_output *output, uint32_t msecs);
void
weston_output_schedule_repaint(struct weston_output *output);
void
weston_output_damage(struct weston_output *output);
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
void
weston_compositor_update_drag_surfaces(struct weston_compositor *compositor);


struct weston_binding;
typedef void (*weston_key_binding_handler_t)(struct wl_seat *seat,
					     uint32_t time, uint32_t key,
					     void *data);
struct weston_binding *
weston_compositor_add_key_binding(struct weston_compositor *compositor,
				  uint32_t key,
				  enum weston_keyboard_modifier modifier,
				  weston_key_binding_handler_t binding,
				  void *data);

typedef void (*weston_button_binding_handler_t)(struct wl_seat *seat,
						uint32_t time, uint32_t button,
						void *data);
struct weston_binding *
weston_compositor_add_button_binding(struct weston_compositor *compositor,
				     uint32_t button,
				     enum weston_keyboard_modifier modifier,
				     weston_button_binding_handler_t binding,
				     void *data);

typedef void (*weston_axis_binding_handler_t)(struct wl_seat *seat,
					      uint32_t time, uint32_t axis,
					      wl_fixed_t value, void *data);
struct weston_binding *
weston_compositor_add_axis_binding(struct weston_compositor *compositor,
			           uint32_t axis,
			           enum weston_keyboard_modifier modifier,
			           weston_axis_binding_handler_t binding,
				   void *data);
struct weston_binding *
weston_compositor_add_debug_binding(struct weston_compositor *compositor,
				    uint32_t key,
				    weston_key_binding_handler_t binding,
				    void *data);
void
weston_binding_destroy(struct weston_binding *binding);

void
weston_binding_list_destroy_all(struct wl_list *list);

void
weston_compositor_run_key_binding(struct weston_compositor *compositor,
				  struct weston_seat *seat, uint32_t time,
				  uint32_t key,
				  enum wl_keyboard_key_state state);
void
weston_compositor_run_button_binding(struct weston_compositor *compositor,
				     struct weston_seat *seat, uint32_t time,
				     uint32_t button,
				     enum wl_pointer_button_state value);
void
weston_compositor_run_axis_binding(struct weston_compositor *compositor,
				   struct weston_seat *seat, uint32_t time,
				   uint32_t axis, int32_t value);
int
weston_compositor_run_debug_binding(struct weston_compositor *compositor,
				    struct weston_seat *seat, uint32_t time,
				    uint32_t key,
				    enum wl_keyboard_key_state state);

int
weston_environment_get_fd(const char *env);

struct wl_list *
weston_compositor_top(struct weston_compositor *compositor);

struct weston_surface *
weston_surface_create(struct weston_compositor *compositor);

void
weston_surface_configure(struct weston_surface *surface,
			 float x, float y, int width, int height);

void
weston_surface_restack(struct weston_surface *surface, struct wl_list *below);

void
weston_surface_set_position(struct weston_surface *surface,
			    float x, float y);

int
weston_surface_is_mapped(struct weston_surface *surface);

void
weston_surface_schedule_repaint(struct weston_surface *surface);

void
weston_surface_damage(struct weston_surface *surface);

void
weston_surface_damage_below(struct weston_surface *surface);

void
weston_surface_move_to_plane(struct weston_surface *surface,
			     struct weston_plane *plane);
void
weston_surface_unmap(struct weston_surface *surface);

void
weston_buffer_post_release(struct wl_buffer *buffer);

void
weston_buffer_reference(struct weston_buffer_reference *ref,
			struct wl_buffer *buffer);

uint32_t
weston_compositor_get_time(void);

int
weston_compositor_init(struct weston_compositor *ec, struct wl_display *display,
		       int argc, char *argv[], const char *config_file);
void
weston_compositor_shutdown(struct weston_compositor *ec);
void
weston_text_cursor_position_notify(struct weston_surface *surface,
						wl_fixed_t x, wl_fixed_t y);
void
weston_output_init_zoom(struct weston_output *output);
void
weston_output_update_zoom(struct weston_output *output, uint32_t type);
void
weston_output_update_matrix(struct weston_output *output);
void
weston_output_move(struct weston_output *output, int x, int y);
void
weston_output_init(struct weston_output *output, struct weston_compositor *c,
		   int x, int y, int width, int height, uint32_t transform);
void
weston_output_destroy(struct weston_output *output);

void
weston_seat_init(struct weston_seat *seat, struct weston_compositor *ec);
void
weston_seat_init_pointer(struct weston_seat *seat);
int
weston_seat_init_keyboard(struct weston_seat *seat, struct xkb_keymap *keymap);
void
weston_seat_init_touch(struct weston_seat *seat);

void
weston_seat_release(struct weston_seat *seat);

/* String literal of spaces, the same width as the timestamp. */
#define STAMP_SPACE "               "

void
weston_log_file_open(const char *filename);
void
weston_log_file_close(void);
int
weston_log(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));
int
weston_log_continue(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));

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

void
tty_reset(struct tty *tty);

int
tty_activate_vt(struct tty *tty, int vt);

void
screenshooter_create(struct weston_compositor *ec);

struct clipboard *
clipboard_create(struct weston_seat *seat);

void
text_cursor_position_notifier_create(struct weston_compositor *ec);

int
text_backend_init(struct weston_compositor *ec);

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

void
weston_watch_process(struct weston_process *process);

struct weston_surface_animation;
typedef	void (*weston_surface_animation_done_func_t)(struct weston_surface_animation *animation, void *data);

struct weston_surface_animation *
weston_zoom_run(struct weston_surface *surface, float start, float stop,
		weston_surface_animation_done_func_t done, void *data);

struct weston_surface_animation *
weston_fade_run(struct weston_surface *surface,
		weston_surface_animation_done_func_t done, void *data);
struct weston_surface_animation *
weston_slide_run(struct weston_surface *surface, float start, float stop,
		 weston_surface_animation_done_func_t done, void *data);

void
weston_surface_set_color(struct weston_surface *surface,
			 float red, float green, float blue, float alpha);

void
weston_surface_destroy(struct weston_surface *surface);

int
weston_output_switch_mode(struct weston_output *output, struct weston_mode *mode);

int
noop_renderer_init(struct weston_compositor *ec);

struct weston_compositor *
backend_init(struct wl_display *display, int argc, char *argv[],
	    const char *config_file);

int
module_init(struct weston_compositor *compositor);

void
weston_transformed_coord(int width, int height,
			 enum wl_output_transform transform,
			 float sx, float sy, float *bx, float *by);
pixman_box32_t
weston_transformed_rect(int width, int height,
			enum wl_output_transform transform,
			pixman_box32_t rect);

#endif
