/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2012-2013 Raspberry Pi Foundation
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

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <libudev.h>

#ifdef HAVE_BCM_HOST
#  include <bcm_host.h>
#else
#  include "rpi-bcm-stubs.h"
#endif

#include "shared/helpers.h"
#include "compositor.h"
#include "rpi-renderer.h"
#include "launcher-util.h"
#include "libinput-seat.h"
#include "presentation-time-server-protocol.h"

#if 0
#define DBG(...) \
	weston_log(__VA_ARGS__)
#else
#define DBG(...) do {} while (0)
#endif

struct rpi_backend;
struct rpi_output;

struct rpi_flippipe {
	int readfd;
	int writefd;
	clockid_t clk_id;
	struct wl_event_source *source;
};

struct rpi_output {
	struct rpi_backend *backend;
	struct weston_output base;
	int single_buffer;

	struct weston_mode mode;
	struct rpi_flippipe flippipe;

	DISPMANX_DISPLAY_HANDLE_T display;
};

struct rpi_seat {
	struct weston_seat base;
	struct wl_list devices_list;

	struct udev_monitor *udev_monitor;
	struct wl_event_source *udev_monitor_source;
	char *seat_id;
};

struct rpi_backend {
	struct weston_backend base;
	struct weston_compositor *compositor;
	uint32_t prev_state;

	struct udev *udev;
	struct udev_input input;
	struct wl_listener session_listener;

	int single_buffer;
};

static inline struct rpi_output *
to_rpi_output(struct weston_output *base)
{
	return container_of(base, struct rpi_output, base);
}

static inline struct rpi_seat *
to_rpi_seat(struct weston_seat *base)
{
	return container_of(base, struct rpi_seat, base);
}

static inline struct rpi_backend *
to_rpi_backend(struct weston_compositor *c)
{
	return container_of(c->backend, struct rpi_backend, base);
}

static void
rpi_flippipe_update_complete(DISPMANX_UPDATE_HANDLE_T update, void *data)
{
	/* This function runs in a different thread. */
	struct rpi_flippipe *flippipe = data;
	struct timespec ts;
	ssize_t ret;

	/* manufacture flip completion timestamp */
	clock_gettime(flippipe->clk_id, &ts);

	ret = write(flippipe->writefd, &ts, sizeof ts);
	if (ret != sizeof ts)
		weston_log("ERROR: %s failed to write, ret %zd, errno %d\n",
			   __func__, ret, errno);
}

static int
rpi_dispmanx_update_submit(DISPMANX_UPDATE_HANDLE_T update,
			   struct rpi_output *output)
{
	/*
	 * The callback registered here will eventually be called
	 * in a different thread context. Therefore we cannot call
	 * the usual functions from rpi_flippipe_update_complete().
	 * Instead, we have a pipe for passing the message from the
	 * thread, waking up the Weston main event loop, calling
	 * rpi_flippipe_handler(), and then ending up in
	 * rpi_output_update_complete() in the main thread context,
	 * where we can do the frame finishing work.
	 */
	return vc_dispmanx_update_submit(update, rpi_flippipe_update_complete,
					 &output->flippipe);
}

static void
rpi_output_update_complete(struct rpi_output *output,
			   const struct timespec *stamp);

static int
rpi_flippipe_handler(int fd, uint32_t mask, void *data)
{
	struct rpi_output *output = data;
	ssize_t ret;
	struct timespec ts;

	if (mask != WL_EVENT_READABLE)
		weston_log("ERROR: unexpected mask 0x%x in %s\n",
			   mask, __func__);

	ret = read(fd, &ts, sizeof ts);
	if (ret != sizeof ts) {
		weston_log("ERROR: %s failed to read, ret %zd, errno %d\n",
			   __func__, ret, errno);
	}

	rpi_output_update_complete(output, &ts);

	return 1;
}

static int
rpi_flippipe_init(struct rpi_flippipe *flippipe, struct rpi_output *output)
{
	struct weston_compositor *compositor = output->backend->compositor;
	struct wl_event_loop *loop;
	int fd[2];

	if (pipe2(fd, O_CLOEXEC) == -1)
		return -1;

	flippipe->readfd = fd[0];
	flippipe->writefd = fd[1];
	flippipe->clk_id = compositor->presentation_clock;

	loop = wl_display_get_event_loop(compositor->wl_display);
	flippipe->source = wl_event_loop_add_fd(loop, flippipe->readfd,
						WL_EVENT_READABLE,
						rpi_flippipe_handler, output);

	if (!flippipe->source) {
		close(flippipe->readfd);
		close(flippipe->writefd);
		return -1;
	}

	return 0;
}

static void
rpi_flippipe_release(struct rpi_flippipe *flippipe)
{
	wl_event_source_remove(flippipe->source);
	close(flippipe->readfd);
	close(flippipe->writefd);
}

static void
rpi_output_start_repaint_loop(struct weston_output *output)
{
	struct timespec ts;

	/* XXX: do a phony dispmanx update and trigger on its completion? */
	weston_compositor_read_presentation_clock(output->compositor, &ts);
	weston_output_finish_frame(output, &ts, WP_PRESENTATION_FEEDBACK_INVALID);
}

static int
rpi_output_repaint(struct weston_output *base, pixman_region32_t *damage)
{
	struct rpi_output *output = to_rpi_output(base);
	struct weston_compositor *compositor = output->backend->compositor;
	struct weston_plane *primary_plane = &compositor->primary_plane;
	DISPMANX_UPDATE_HANDLE_T update;

	DBG("frame update start\n");

	/* Update priority higher than in rpi-renderer's
	 * output destroy function, see rpi_output_destroy().
	 */
	update = vc_dispmanx_update_start(1);

	rpi_renderer_set_update_handle(&output->base, update);
	compositor->renderer->repaint_output(&output->base, damage);

	pixman_region32_subtract(&primary_plane->damage,
				 &primary_plane->damage, damage);

	/* schedule callback to rpi_output_update_complete() */
	rpi_dispmanx_update_submit(update, output);
	DBG("frame update submitted\n");
	return 0;
}

static void
rpi_output_update_complete(struct rpi_output *output,
			   const struct timespec *stamp)
{
	uint32_t flags = WP_PRESENTATION_FEEDBACK_KIND_VSYNC |
			 WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION;

	DBG("frame update complete(%ld.%09ld)\n",
	    (long)stamp->tv_sec, (long)stamp->tv_nsec);
	rpi_renderer_finish_frame(&output->base);
	weston_output_finish_frame(&output->base, stamp, flags);
}

static void
rpi_output_destroy(struct weston_output *base)
{
	struct rpi_output *output = to_rpi_output(base);

	DBG("%s\n", __func__);

	rpi_renderer_output_destroy(base);

	/* rpi_renderer_output_destroy() will schedule a removal of
	 * all Dispmanx Elements, and wait for the update to complete.
	 * Assuming updates are sequential, the wait should guarantee,
	 * that any pending rpi_flippipe_update_complete() callbacks
	 * have happened already. Therefore we can destroy the flippipe
	 * now.
	 */
	rpi_flippipe_release(&output->flippipe);

	weston_output_destroy(&output->base);

	vc_dispmanx_display_close(output->display);

	free(output);
}

static int
rpi_output_create(struct rpi_backend *backend, uint32_t transform)
{
	struct rpi_output *output;
	DISPMANX_MODEINFO_T modeinfo;
	int ret;
	float mm_width, mm_height;

	output = zalloc(sizeof *output);
	if (output == NULL)
		return -1;

	output->backend = backend;
	output->single_buffer = backend->single_buffer;

	if (rpi_flippipe_init(&output->flippipe, output) < 0) {
		weston_log("Creating message pipe failed.\n");
		goto out_free;
	}

	output->display = vc_dispmanx_display_open(DISPMANX_ID_HDMI);
	if (!output->display) {
		weston_log("Failed to open dispmanx HDMI display.\n");
		goto out_pipe;
	}

	ret = vc_dispmanx_display_get_info(output->display, &modeinfo);
	if (ret < 0) {
		weston_log("Failed to get display mode information.\n");
		goto out_dmx_close;
	}

	output->base.start_repaint_loop = rpi_output_start_repaint_loop;
	output->base.repaint = rpi_output_repaint;
	output->base.destroy = rpi_output_destroy;
	output->base.assign_planes = NULL;
	output->base.set_backlight = NULL;
	output->base.set_dpms = NULL;
	output->base.switch_mode = NULL;

	/* XXX: use tvservice to get information from and control the
	 * HDMI and SDTV outputs. See:
	 * /opt/vc/include/interface/vmcs_host/vc_tvservice.h
	 */

	/* only one static mode in list */
	output->mode.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	output->mode.width = modeinfo.width;
	output->mode.height = modeinfo.height;
	output->mode.refresh = 60000;
	wl_list_init(&output->base.mode_list);
	wl_list_insert(&output->base.mode_list, &output->mode.link);

	output->base.current_mode = &output->mode;
	output->base.subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
	output->base.make = "unknown";
	output->base.model = "unknown";
	output->base.name = strdup("rpi");

	/* guess 96 dpi */
	mm_width  = modeinfo.width * (25.4f / 96.0f);
	mm_height = modeinfo.height * (25.4f / 96.0f);

	weston_output_init(&output->base, backend->compositor,
			   0, 0, round(mm_width), round(mm_height),
			   transform, 1);

	if (rpi_renderer_output_create(&output->base, output->display) < 0)
		goto out_output;

	weston_compositor_add_output(backend->compositor, &output->base);

	weston_log("Raspberry Pi HDMI output %dx%d px\n",
		   output->mode.width, output->mode.height);
	weston_log_continue(STAMP_SPACE "guessing %d Hz and 96 dpi\n",
			    output->mode.refresh / 1000);
	weston_log_continue(STAMP_SPACE "orientation: %s\n",
			    weston_transform_to_string(output->base.transform));

	if (!strncmp(weston_transform_to_string(output->base.transform),
						"flipped", 7))
		weston_log("warning: flipped output transforms may not work\n");

	return 0;

out_output:
	weston_output_destroy(&output->base);

out_dmx_close:
	vc_dispmanx_display_close(output->display);

out_pipe:
	rpi_flippipe_release(&output->flippipe);

out_free:
	free(output);
	return -1;
}

static void
rpi_backend_destroy(struct weston_compositor *base)
{
	struct rpi_backend *backend = to_rpi_backend(base);

	udev_input_destroy(&backend->input);

	/* destroys outputs, too */
	weston_compositor_shutdown(base);

	weston_launcher_destroy(base->launcher);

	bcm_host_deinit();
	free(backend);
}

static void
session_notify(struct wl_listener *listener, void *data)
{
	struct weston_compositor *compositor = data;
	struct rpi_backend *backend = to_rpi_backend(compositor);
	struct weston_output *output;

	if (compositor->session_active) {
		weston_log("activating session\n");
		compositor->state = backend->prev_state;
		weston_compositor_damage_all(compositor);
		udev_input_enable(&backend->input);
	} else {
		weston_log("deactivating session\n");
		udev_input_disable(&backend->input);

		backend->prev_state = compositor->state;
		weston_compositor_offscreen(compositor);

		/* If we have a repaint scheduled (either from a
		 * pending pageflip or the idle handler), make sure we
		 * cancel that so we don't try to pageflip when we're
		 * vt switched away.  The OFFSCREEN state will prevent
		 * further attemps at repainting.  When we switch
		 * back, we schedule a repaint, which will process
		 * pending frame callbacks. */

		wl_list_for_each(output,
				 &compositor->output_list, link) {
			output->repaint_needed = 0;
		}
	};
}

static void
rpi_restore(struct weston_compositor *compositor)
{
	weston_launcher_restore(compositor->launcher);
}

struct rpi_parameters {
	int tty;
	struct rpi_renderer_parameters renderer;
	uint32_t output_transform;
};

static struct rpi_backend *
rpi_backend_create(struct weston_compositor *compositor,
		   struct rpi_parameters *param)
{
	struct rpi_backend *backend;

	weston_log("initializing Raspberry Pi backend\n");

	backend = zalloc(sizeof *backend);
	if (backend == NULL)
		return NULL;

	if (weston_compositor_set_presentation_clock_software(
							compositor) < 0)
		goto out_compositor;

	backend->udev = udev_new();
	if (backend->udev == NULL) {
		weston_log("Failed to initialize udev context.\n");
		goto out_compositor;
	}

	backend->session_listener.notify = session_notify;
	wl_signal_add(&compositor->session_signal,
		      &backend->session_listener);
	compositor->launcher =
		weston_launcher_connect(compositor, param->tty, "seat0", false);
	if (!compositor->launcher) {
		weston_log("Failed to initialize tty.\n");
		goto out_udev;
	}

	backend->base.destroy = rpi_backend_destroy;
	backend->base.restore = rpi_restore;

	backend->compositor = compositor;
	backend->prev_state = WESTON_COMPOSITOR_ACTIVE;
	backend->single_buffer = param->renderer.single_buffer;

	weston_log("Dispmanx planes are %s buffered.\n",
		   backend->single_buffer ? "single" : "double");

	weston_setup_vt_switch_bindings(compositor);

	/*
	 * bcm_host_init() creates threads.
	 * Therefore we must have all signal handlers set and signals blocked
	 * before calling it. Otherwise the signals may end in the bcm
	 * threads and cause the default behaviour there. For instance,
	 * SIGUSR1 used for VT switching caused Weston to terminate there.
	 */
	bcm_host_init();

	if (rpi_renderer_create(compositor, &param->renderer) < 0)
		goto out_launcher;

	if (rpi_output_create(backend, param->output_transform) < 0)
		goto out_launcher;

	if (udev_input_init(&backend->input,
			    compositor,
			    backend->udev, "seat0") != 0) {
		weston_log("Failed to initialize udev input.\n");
		goto out_launcher;
	}

	compositor->backend = &backend->base;

	return backend;

out_launcher:
	weston_launcher_destroy(compositor->launcher);

out_udev:
	udev_unref(backend->udev);

out_compositor:
	weston_compositor_shutdown(compositor);

	bcm_host_deinit();
	free(backend);

	return NULL;
}

WL_EXPORT int
backend_init(struct weston_compositor *compositor,
	     int *argc, char *argv[],
	     struct weston_config *config,
	     struct weston_backend_config *config_base)
{
	const char *transform = "normal";
	struct rpi_backend *b;

	struct rpi_parameters param = {
		.tty = 0, /* default to current tty */
		.renderer.single_buffer = 0,
		.output_transform = WL_OUTPUT_TRANSFORM_NORMAL,
		.renderer.opaque_regions = 0,
	};

	const struct weston_option rpi_options[] = {
		{ WESTON_OPTION_INTEGER, "tty", 0, &param.tty },
		{ WESTON_OPTION_BOOLEAN, "single-buffer", 0,
		  &param.renderer.single_buffer },
		{ WESTON_OPTION_STRING, "transform", 0, &transform },
		{ WESTON_OPTION_BOOLEAN, "opaque-regions", 0,
		  &param.renderer.opaque_regions },
	};

	parse_options(rpi_options, ARRAY_LENGTH(rpi_options), argc, argv);

	if (weston_parse_transform(transform, &param.output_transform) < 0)
		weston_log("invalid transform \"%s\"\n", transform);

	b = rpi_backend_create(compositor, &param);
	if (b == NULL)
		return -1;
	return 0;
}
