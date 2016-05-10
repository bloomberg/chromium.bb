/*
 * Copyright © 2010-2011 Intel Corporation
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2012-2015 Collabora, Ltd.
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

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifdef HAVE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#include "compositor.h"
#include "../shared/os-compatibility.h"
#include "../shared/helpers.h"
#include "git-version.h"
#include "version.h"

#include "compositor-drm.h"
#include "compositor-headless.h"
#include "compositor-rdp.h"
#include "compositor-fbdev.h"
#include "compositor-x11.h"
#include "compositor-wayland.h"

#define WINDOW_TITLE "Weston Compositor"

static struct wl_list child_process_list;
static struct weston_compositor *segv_compositor;

static int
sigchld_handler(int signal_number, void *data)
{
	struct weston_process *p;
	int status;
	pid_t pid;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		wl_list_for_each(p, &child_process_list, link) {
			if (p->pid == pid)
				break;
		}

		if (&p->link == &child_process_list) {
			weston_log("unknown child process exited\n");
			continue;
		}

		wl_list_remove(&p->link);
		p->cleanup(p, status);
	}

	if (pid < 0 && errno != ECHILD)
		weston_log("waitpid error %m\n");

	return 1;
}

#ifdef HAVE_LIBUNWIND

static void
print_backtrace(void)
{
	unw_cursor_t cursor;
	unw_context_t context;
	unw_word_t off;
	unw_proc_info_t pip;
	int ret, i = 0;
	char procname[256];
	const char *filename;
	Dl_info dlinfo;

	pip.unwind_info = NULL;
	ret = unw_getcontext(&context);
	if (ret) {
		weston_log("unw_getcontext: %d\n", ret);
		return;
	}

	ret = unw_init_local(&cursor, &context);
	if (ret) {
		weston_log("unw_init_local: %d\n", ret);
		return;
	}

	ret = unw_step(&cursor);
	while (ret > 0) {
		ret = unw_get_proc_info(&cursor, &pip);
		if (ret) {
			weston_log("unw_get_proc_info: %d\n", ret);
			break;
		}

		ret = unw_get_proc_name(&cursor, procname, 256, &off);
		if (ret && ret != -UNW_ENOMEM) {
			if (ret != -UNW_EUNSPEC)
				weston_log("unw_get_proc_name: %d\n", ret);
			procname[0] = '?';
			procname[1] = 0;
		}

		if (dladdr((void *)(pip.start_ip + off), &dlinfo) && dlinfo.dli_fname &&
		    *dlinfo.dli_fname)
			filename = dlinfo.dli_fname;
		else
			filename = "?";

		weston_log("%u: %s (%s%s+0x%x) [%p]\n", i++, filename, procname,
			   ret == -UNW_ENOMEM ? "..." : "", (int)off, (void *)(pip.start_ip + off));

		ret = unw_step(&cursor);
		if (ret < 0)
			weston_log("unw_step: %d\n", ret);
	}
}

#else

static void
print_backtrace(void)
{
	void *buffer[32];
	int i, count;
	Dl_info info;

	count = backtrace(buffer, ARRAY_LENGTH(buffer));
	for (i = 0; i < count; i++) {
		dladdr(buffer[i], &info);
		weston_log("  [%016lx]  %s  (%s)\n",
			(long) buffer[i],
			info.dli_sname ? info.dli_sname : "--",
			info.dli_fname);
	}
}

#endif

WL_EXPORT void
weston_watch_process(struct weston_process *process)
{
	wl_list_insert(&child_process_list, &process->link);
}

static void
log_uname(void)
{
	struct utsname usys;

	uname(&usys);

	weston_log("OS: %s, %s, %s, %s\n", usys.sysname, usys.release,
						usys.version, usys.machine);
}

static const char xdg_error_message[] =
	"fatal: environment variable XDG_RUNTIME_DIR is not set.\n";

static const char xdg_wrong_message[] =
	"fatal: environment variable XDG_RUNTIME_DIR\n"
	"is set to \"%s\", which is not a directory.\n";

static const char xdg_wrong_mode_message[] =
	"warning: XDG_RUNTIME_DIR \"%s\" is not configured\n"
	"correctly.  Unix access mode must be 0700 (current mode is %o),\n"
	"and must be owned by the user (current owner is UID %d).\n";

static const char xdg_detail_message[] =
	"Refer to your distribution on how to get it, or\n"
	"http://www.freedesktop.org/wiki/Specifications/basedir-spec\n"
	"on how to implement it.\n";

static void
verify_xdg_runtime_dir(void)
{
	char *dir = getenv("XDG_RUNTIME_DIR");
	struct stat s;

	if (!dir) {
		weston_log(xdg_error_message);
		weston_log_continue(xdg_detail_message);
		exit(EXIT_FAILURE);
	}

	if (stat(dir, &s) || !S_ISDIR(s.st_mode)) {
		weston_log(xdg_wrong_message, dir);
		weston_log_continue(xdg_detail_message);
		exit(EXIT_FAILURE);
	}

	if ((s.st_mode & 0777) != 0700 || s.st_uid != getuid()) {
		weston_log(xdg_wrong_mode_message,
			   dir, s.st_mode & 0777, s.st_uid);
		weston_log_continue(xdg_detail_message);
	}
}

static int
usage(int error_code)
{
	fprintf(stderr,
		"Usage: weston [OPTIONS]\n\n"
		"This is weston version " VERSION ", the Wayland reference compositor.\n"
		"Weston supports multiple backends, and depending on which backend is in use\n"
		"different options will be accepted.\n\n"


		"Core options:\n\n"
		"  --version\t\tPrint weston version\n"
		"  -B, --backend=MODULE\tBackend module, one of\n"
#if defined(BUILD_DRM_COMPOSITOR)
			"\t\t\t\tdrm-backend.so\n"
#endif
#if defined(BUILD_FBDEV_COMPOSITOR)
			"\t\t\t\tfbdev-backend.so\n"
#endif
#if defined(BUILD_HEADLESS_COMPOSITOR)
			"\t\t\t\theadless-backend.so\n"
#endif
#if defined(BUILD_RDP_COMPOSITOR)
			"\t\t\t\trdp-backend.so\n"
#endif
#if defined(BUILD_RPI_COMPOSITOR) && defined(HAVE_BCM_HOST)
			"\t\t\t\trpi-backend.so\n"
#endif
#if defined(BUILD_WAYLAND_COMPOSITOR)
			"\t\t\t\twayland-backend.so\n"
#endif
#if defined(BUILD_X11_COMPOSITOR)
			"\t\t\t\tx11-backend.so\n"
#endif
		"  --shell=MODULE\tShell module, defaults to desktop-shell.so\n"
		"  -S, --socket=NAME\tName of socket to listen on\n"
		"  -i, --idle-time=SECS\tIdle time in seconds\n"
		"  --modules\t\tLoad the comma-separated list of modules\n"
		"  --log=FILE\t\tLog to the given file\n"
		"  -c, --config=FILE\tConfig file to load, defaults to weston.ini\n"
		"  --no-config\t\tDo not read weston.ini\n"
		"  -h, --help\t\tThis help message\n\n");

#if defined(BUILD_DRM_COMPOSITOR)
	fprintf(stderr,
		"Options for drm-backend.so:\n\n"
		"  --connector=ID\tBring up only this connector\n"
		"  --seat=SEAT\t\tThe seat that weston should run on\n"
		"  --tty=TTY\t\tThe tty to use\n"
		"  --use-pixman\t\tUse the pixman (CPU) renderer\n"
		"  --current-mode\tPrefer current KMS mode over EDID preferred mode\n\n");
#endif

#if defined(BUILD_FBDEV_COMPOSITOR)
	fprintf(stderr,
		"Options for fbdev-backend.so:\n\n"
		"  --tty=TTY\t\tThe tty to use\n"
		"  --device=DEVICE\tThe framebuffer device to use\n"
		"  --use-gl\t\tUse the GL renderer\n\n");
#endif

#if defined(BUILD_HEADLESS_COMPOSITOR)
	fprintf(stderr,
		"Options for headless-backend.so:\n\n"
		"  --width=WIDTH\t\tWidth of memory surface\n"
		"  --height=HEIGHT\tHeight of memory surface\n"
		"  --transform=TR\tThe output transformation, TR is one of:\n"
		"\tnormal 90 180 270 flipped flipped-90 flipped-180 flipped-270\n"
		"  --use-pixman\t\tUse the pixman (CPU) renderer (default: no rendering)\n\n");
#endif

#if defined(BUILD_RDP_COMPOSITOR)
	fprintf(stderr,
		"Options for rdp-backend.so:\n\n"
		"  --width=WIDTH\t\tWidth of desktop\n"
		"  --height=HEIGHT\tHeight of desktop\n"
		"  --env-socket\t\tUse socket defined in RDP_FD env variable as peer connection\n"
		"  --address=ADDR\tThe address to bind\n"
		"  --port=PORT\t\tThe port to listen on\n"
		"  --no-clients-resize\tThe RDP peers will be forced to the size of the desktop\n"
		"  --rdp4-key=FILE\tThe file containing the key for RDP4 encryption\n"
		"  --rdp-tls-cert=FILE\tThe file containing the certificate for TLS encryption\n"
		"  --rdp-tls-key=FILE\tThe file containing the private key for TLS encryption\n"
		"\n");
#endif

#if defined(BUILD_RPI_COMPOSITOR) && defined(HAVE_BCM_HOST)
	fprintf(stderr,
		"Options for rpi-backend.so:\n\n"
		"  --tty=TTY\t\tThe tty to use\n"
		"  --single-buffer\tUse single-buffered Dispmanx elements.\n"
		"  --transform=TR\tThe output transformation, TR is one of:\n"
		"\tnormal 90 180 270 flipped flipped-90 flipped-180 flipped-270\n"
		"  --opaque-regions\tEnable support for opaque regions, can be "
		"very slow without support in the GPU firmware.\n"
		"\n");
#endif

#if defined(BUILD_WAYLAND_COMPOSITOR)
	fprintf(stderr,
		"Options for wayland-backend.so:\n\n"
		"  --width=WIDTH\t\tWidth of Wayland surface\n"
		"  --height=HEIGHT\tHeight of Wayland surface\n"
		"  --scale=SCALE\t\tScale factor of output\n"
		"  --fullscreen\t\tRun in fullscreen mode\n"
		"  --use-pixman\t\tUse the pixman (CPU) renderer\n"
		"  --output-count=COUNT\tCreate multiple outputs\n"
		"  --sprawl\t\tCreate one fullscreen output for every parent output\n"
		"  --display=DISPLAY\tWayland display to connect to\n\n");
#endif

#if defined(BUILD_X11_COMPOSITOR)
	fprintf(stderr,
		"Options for x11-backend.so:\n\n"
		"  --width=WIDTH\t\tWidth of X window\n"
		"  --height=HEIGHT\tHeight of X window\n"
		"  --scale=SCALE\t\tScale factor of output\n"
		"  --fullscreen\t\tRun in fullscreen mode\n"
		"  --use-pixman\t\tUse the pixman (CPU) renderer\n"
		"  --output-count=COUNT\tCreate multiple outputs\n"
		"  --no-input\t\tDont create input devices\n\n");
#endif

	exit(error_code);
}

static int on_term_signal(int signal_number, void *data)
{
	struct wl_display *display = data;

	weston_log("caught signal %d\n", signal_number);
	wl_display_terminate(display);

	return 1;
}

static void
on_caught_signal(int s, siginfo_t *siginfo, void *context)
{
	/* This signal handler will do a best-effort backtrace, and
	 * then call the backend restore function, which will switch
	 * back to the vt we launched from or ungrab X etc and then
	 * raise SIGTRAP.  If we run weston under gdb from X or a
	 * different vt, and tell gdb "handle *s* nostop", this
	 * will allow weston to switch back to gdb on crash and then
	 * gdb will catch the crash with SIGTRAP.*/

	weston_log("caught signal: %d\n", s);

	print_backtrace();

	segv_compositor->backend->restore(segv_compositor);

	raise(SIGTRAP);
}

static void
catch_signals(void)
{
	struct sigaction action;

	action.sa_flags = SA_SIGINFO | SA_RESETHAND;
	action.sa_sigaction = on_caught_signal;
	sigemptyset(&action.sa_mask);
	sigaction(SIGSEGV, &action, NULL);
	sigaction(SIGABRT, &action, NULL);
}

static const char *
clock_name(clockid_t clk_id)
{
	static const char *names[] = {
		[CLOCK_REALTIME] =		"CLOCK_REALTIME",
		[CLOCK_MONOTONIC] =		"CLOCK_MONOTONIC",
		[CLOCK_MONOTONIC_RAW] =		"CLOCK_MONOTONIC_RAW",
		[CLOCK_REALTIME_COARSE] =	"CLOCK_REALTIME_COARSE",
		[CLOCK_MONOTONIC_COARSE] =	"CLOCK_MONOTONIC_COARSE",
#ifdef CLOCK_BOOTTIME
		[CLOCK_BOOTTIME] =		"CLOCK_BOOTTIME",
#endif
	};

	if (clk_id < 0 || (unsigned)clk_id >= ARRAY_LENGTH(names))
		return "unknown";

	return names[clk_id];
}

static const struct {
	uint32_t bit; /* enum weston_capability */
	const char *desc;
} capability_strings[] = {
	{ WESTON_CAP_ROTATION_ANY, "arbitrary surface rotation:" },
	{ WESTON_CAP_CAPTURE_YFLIP, "screen capture uses y-flip:" },
};

static void
weston_compositor_log_capabilities(struct weston_compositor *compositor)
{
	unsigned i;
	int yes;

	weston_log("Compositor capabilities:\n");
	for (i = 0; i < ARRAY_LENGTH(capability_strings); i++) {
		yes = compositor->capabilities & capability_strings[i].bit;
		weston_log_continue(STAMP_SPACE "%s %s\n",
				    capability_strings[i].desc,
				    yes ? "yes" : "no");
	}

	weston_log_continue(STAMP_SPACE "presentation clock: %s, id %d\n",
			    clock_name(compositor->presentation_clock),
			    compositor->presentation_clock);
}

static void
handle_primary_client_destroyed(struct wl_listener *listener, void *data)
{
	struct wl_client *client = data;

	weston_log("Primary client died.  Closing...\n");

	wl_display_terminate(wl_client_get_display(client));
}

static int
weston_create_listening_socket(struct wl_display *display, const char *socket_name)
{
	if (socket_name) {
		if (wl_display_add_socket(display, socket_name)) {
			weston_log("fatal: failed to add socket: %m\n");
			return -1;
		}
	} else {
		socket_name = wl_display_add_socket_auto(display);
		if (!socket_name) {
			weston_log("fatal: failed to add socket: %m\n");
			return -1;
		}
	}

	setenv("WAYLAND_DISPLAY", socket_name, 1);

	return 0;
}

static int
load_modules(struct weston_compositor *ec, const char *modules,
	     int *argc, char *argv[])
{
	const char *p, *end;
	char buffer[256];
	int (*module_init)(struct weston_compositor *ec,
			   int *argc, char *argv[]);

	if (modules == NULL)
		return 0;

	p = modules;
	while (*p) {
		end = strchrnul(p, ',');
		snprintf(buffer, sizeof buffer, "%.*s", (int) (end - p), p);
		module_init = weston_load_module(buffer, "module_init");
		if (!module_init)
			return -1;
		if (module_init(ec, argc, argv) < 0)
			return -1;
		p = end;
		while (*p == ',')
			p++;

	}

	return 0;
}

static int
weston_compositor_init_config(struct weston_compositor *ec,
			      struct weston_config *config)
{
	struct xkb_rule_names xkb_names;
	struct weston_config_section *s;
	int repaint_msec;
	int vt_switching;

	s = weston_config_get_section(config, "keyboard", NULL, NULL);
	weston_config_section_get_string(s, "keymap_rules",
					 (char **) &xkb_names.rules, NULL);
	weston_config_section_get_string(s, "keymap_model",
					 (char **) &xkb_names.model, NULL);
	weston_config_section_get_string(s, "keymap_layout",
					 (char **) &xkb_names.layout, NULL);
	weston_config_section_get_string(s, "keymap_variant",
					 (char **) &xkb_names.variant, NULL);
	weston_config_section_get_string(s, "keymap_options",
					 (char **) &xkb_names.options, NULL);

	if (weston_compositor_xkb_init(ec, &xkb_names) < 0)
		return -1;

	weston_config_section_get_int(s, "repeat-rate",
				      &ec->kb_repeat_rate, 40);
	weston_config_section_get_int(s, "repeat-delay",
				      &ec->kb_repeat_delay, 400);

	weston_config_section_get_bool(s, "vt-switching",
				       &vt_switching, true);
	ec->vt_switching = vt_switching;

	s = weston_config_get_section(config, "core", NULL, NULL);
	weston_config_section_get_int(s, "repaint-window", &repaint_msec,
				      ec->repaint_msec);
	if (repaint_msec < -10 || repaint_msec > 1000) {
		weston_log("Invalid repaint_window value in config: %d\n",
			   repaint_msec);
	} else {
		ec->repaint_msec = repaint_msec;
	}
	weston_log("Output repaint window is %d ms maximum.\n",
		   ec->repaint_msec);

	return 0;
}

static char *
weston_choose_default_backend(void)
{
	char *backend = NULL;

	if (getenv("WAYLAND_DISPLAY") || getenv("WAYLAND_SOCKET"))
		backend = strdup("wayland-backend.so");
	else if (getenv("DISPLAY"))
		backend = strdup("x11-backend.so");
	else
		backend = strdup(WESTON_NATIVE_BACKEND);

	return backend;
}

static const struct { const char *name; uint32_t token; } transforms[] = {
	{ "normal",     WL_OUTPUT_TRANSFORM_NORMAL },
	{ "90",         WL_OUTPUT_TRANSFORM_90 },
	{ "180",        WL_OUTPUT_TRANSFORM_180 },
	{ "270",        WL_OUTPUT_TRANSFORM_270 },
	{ "flipped",    WL_OUTPUT_TRANSFORM_FLIPPED },
	{ "flipped-90", WL_OUTPUT_TRANSFORM_FLIPPED_90 },
	{ "flipped-180", WL_OUTPUT_TRANSFORM_FLIPPED_180 },
	{ "flipped-270", WL_OUTPUT_TRANSFORM_FLIPPED_270 },
};

WL_EXPORT int
weston_parse_transform(const char *transform, uint32_t *out)
{
	unsigned int i;

	for (i = 0; i < ARRAY_LENGTH(transforms); i++)
		if (strcmp(transforms[i].name, transform) == 0) {
			*out = transforms[i].token;
			return 0;
		}

	*out = WL_OUTPUT_TRANSFORM_NORMAL;
	return -1;
}

WL_EXPORT const char *
weston_transform_to_string(uint32_t output_transform)
{
	unsigned int i;

	for (i = 0; i < ARRAY_LENGTH(transforms); i++)
		if (transforms[i].token == output_transform)
			return transforms[i].name;

	return "<illegal value>";
}

static int
load_configuration(struct weston_config **config, int32_t noconfig,
		   const char *config_file)
{
	const char *file = "weston.ini";
	const char *full_path;

	*config = NULL;

	if (config_file)
		file = config_file;

	if (noconfig == 0)
		*config = weston_config_parse(file);

	if (*config) {
		full_path = weston_config_get_full_path(*config);

		weston_log("Using config file '%s'\n", full_path);
		setenv(WESTON_CONFIG_FILE_ENV_VAR, full_path, 1);

		return 0;
	}

	if (config_file && noconfig == 0) {
		weston_log("fatal: error opening or reading config file"
			   " '%s'.\n", config_file);

		return -1;
	}

	weston_log("Starting with no config file.\n");
	setenv(WESTON_CONFIG_FILE_ENV_VAR, "", 1);

	return 0;
}

static void
handle_exit(struct weston_compositor *c)
{
	wl_display_terminate(c->wl_display);
}

/* Temporary function to be removed when all backends are converted. */
static int
load_backend_old(struct weston_compositor *compositor, const char *backend,
		 int *argc, char **argv, struct weston_config *wc)
{
	int (*backend_init)(struct weston_compositor *c,
			    int *argc, char *argv[],
			    struct weston_config *config,
			    struct weston_backend_config *config_base);

	backend_init = weston_load_module(backend, "backend_init");
	if (!backend_init)
		return -1;

	return backend_init(compositor, argc, argv, wc, NULL);
}

/** Main module call-point for backends.
 *
 * All backends should use this routine to access their init routine.
 * Backends may subclass weston_backend_config to add their own
 * configuration data, setting the major/minor version in config_base
 * accordingly.
 *
 * The config_base object should be treated as temporary, and any data
 * copied out of it by backend_init before returning.  The load_backend_new
 * callers may then free the config_base object.
 *
 * NOTE: This is a temporary function intended to eventually be replaced
 * by weston_compositor_load_backend().
 */
static int
load_backend_new(struct weston_compositor *compositor, const char *backend,
		 struct weston_backend_config *config_base)
{
	int (*backend_init)(struct weston_compositor *c,
			    int *argc, char *argv[],
			    struct weston_config *config,
			    struct weston_backend_config *config_base);

	backend_init = weston_load_module(backend, "backend_init");
	if (!backend_init)
		return -1;

	return backend_init(compositor, NULL, NULL, NULL, config_base);
}

static enum weston_drm_backend_output_mode
drm_configure_output(struct weston_compositor *c,
		     bool use_current_mode,
		     const char *name,
		     struct weston_drm_backend_output_config *config)
{
	struct weston_config *wc = weston_compositor_get_user_data(c);
	struct weston_config_section *section;
	char *s;
	int scale;
	enum weston_drm_backend_output_mode mode =
		WESTON_DRM_BACKEND_OUTPUT_PREFERRED;

	section = weston_config_get_section(wc, "output", "name", name);
	weston_config_section_get_string(section, "mode", &s, "preferred");
	if (strcmp(s, "off") == 0) {
		free(s);
		return WESTON_DRM_BACKEND_OUTPUT_OFF;
	}

	if (use_current_mode || strcmp(s, "current") == 0) {
		mode = WESTON_DRM_BACKEND_OUTPUT_CURRENT;
	} else if (strcmp(s, "preferred") != 0) {
		config->modeline = s;
		s = NULL;
	}
	free(s);

	weston_config_section_get_int(section, "scale", &scale, 1);
	config->base.scale = scale >= 1 ? scale : 1;
	weston_config_section_get_string(section, "transform", &s, "normal");
	if (weston_parse_transform(s, &config->base.transform) < 0)
		weston_log("Invalid transform \"%s\" for output %s\n",
			   s, name);
	free(s);

	weston_config_section_get_string(section,
					 "gbm-format", &config->gbm_format, NULL);
	weston_config_section_get_string(section, "seat", &config->seat, "");
	return mode;
}

static int
load_drm_backend(struct weston_compositor *c, const char *backend,
		 int *argc, char **argv, struct weston_config *wc)
{
	struct weston_drm_backend_config config = {{ 0, }};
	struct weston_config_section *section;
	int ret = 0;

	const struct weston_option options[] = {
		{ WESTON_OPTION_INTEGER, "connector", 0, &config.connector },
		{ WESTON_OPTION_STRING, "seat", 0, &config.seat_id },
		{ WESTON_OPTION_INTEGER, "tty", 0, &config.tty },
		{ WESTON_OPTION_BOOLEAN, "current-mode", 0, &config.use_current_mode },
		{ WESTON_OPTION_BOOLEAN, "use-pixman", 0, &config.use_pixman },
	};

	parse_options(options, ARRAY_LENGTH(options), argc, argv);

	section = weston_config_get_section(wc, "core", NULL, NULL);
	weston_config_section_get_string(section,
					 "gbm-format", &config.gbm_format,
					 NULL);

	config.base.struct_version = WESTON_DRM_BACKEND_CONFIG_VERSION;
	config.base.struct_size = sizeof(struct weston_drm_backend_config);
	config.configure_output = drm_configure_output;

	ret = load_backend_new(c, backend, &config.base);

	free(config.gbm_format);
	free(config.seat_id);

	return ret;
}

static int
load_headless_backend(struct weston_compositor *c, char const * backend,
		      int *argc, char **argv, struct weston_config *wc)
{
	struct weston_headless_backend_config config = {{ 0, }};
	int ret = 0;
	char *transform = NULL;

	config.width = 1024;
	config.height = 640;

	const struct weston_option options[] = {
		{ WESTON_OPTION_INTEGER, "width", 0, &config.width },
		{ WESTON_OPTION_INTEGER, "height", 0, &config.height },
		{ WESTON_OPTION_BOOLEAN, "use-pixman", 0, &config.use_pixman },
		{ WESTON_OPTION_STRING, "transform", 0, &transform },
	};

	parse_options(options, ARRAY_LENGTH(options), argc, argv);

	config.transform = WL_OUTPUT_TRANSFORM_NORMAL;
	if (transform) {
		if (weston_parse_transform(transform, &config.transform) < 0)
			weston_log("Invalid transform \"%s\"\n", transform);
		free(transform);
	}

	config.base.struct_version = WESTON_HEADLESS_BACKEND_CONFIG_VERSION;
	config.base.struct_size = sizeof(struct weston_headless_backend_config);

	/* load the actual wayland backend and configure it */
	ret = load_backend_new(c, backend, &config.base);

	return ret;
}

static void
weston_rdp_backend_config_init(struct weston_rdp_backend_config *config)
{
	config->base.struct_version = WESTON_RDP_BACKEND_CONFIG_VERSION;
	config->base.struct_size = sizeof(struct weston_rdp_backend_config);

	config->width = 640;
	config->height = 480;
	config->bind_address = NULL;
	config->port = 3389;
	config->rdp_key = NULL;
	config->server_cert = NULL;
	config->server_key = NULL;
	config->env_socket = 0;
	config->no_clients_resize = 0;
}

static int
load_rdp_backend(struct weston_compositor *c, char const * backend,
		int *argc, char *argv[], struct weston_config *wc)
{
	struct weston_rdp_backend_config config  = {{ 0, }};
	int ret = 0;

	weston_rdp_backend_config_init(&config);

	const struct weston_option rdp_options[] = {
		{ WESTON_OPTION_BOOLEAN, "env-socket", 0, &config.env_socket },
		{ WESTON_OPTION_INTEGER, "width", 0, &config.width },
		{ WESTON_OPTION_INTEGER, "height", 0, &config.height },
		{ WESTON_OPTION_STRING,  "address", 0, &config.bind_address },
		{ WESTON_OPTION_INTEGER, "port", 0, &config.port },
		{ WESTON_OPTION_BOOLEAN, "no-clients-resize", 0, &config.no_clients_resize },
		{ WESTON_OPTION_STRING,  "rdp4-key", 0, &config.rdp_key },
		{ WESTON_OPTION_STRING,  "rdp-tls-cert", 0, &config.server_cert },
		{ WESTON_OPTION_STRING,  "rdp-tls-key", 0, &config.server_key }
	};

	parse_options(rdp_options, ARRAY_LENGTH(rdp_options), argc, argv);

	ret = load_backend_new(c, backend, &config.base);

	free(config.bind_address);
	free(config.rdp_key);
	free(config.server_cert);
	free(config.server_key);
	return ret;
}

static int
load_fbdev_backend(struct weston_compositor *c, char const * backend,
		      int *argc, char **argv, struct weston_config *wc)
{
	struct weston_fbdev_backend_config config = {{ 0, }};
	struct weston_config_section *section;
	char *s = NULL;
	int ret = 0;

	const struct weston_option fbdev_options[] = {
		{ WESTON_OPTION_INTEGER, "tty", 0, &config.tty },
		{ WESTON_OPTION_STRING, "device", 0, &config.device },
		{ WESTON_OPTION_BOOLEAN, "use-gl", 0, &config.use_gl },
	};

	parse_options(fbdev_options, ARRAY_LENGTH(fbdev_options), argc, argv);

	if (!config.device)
		config.device = strdup("/dev/fb0");

	section = weston_config_get_section(wc, "output", "name", "fbdev");
	weston_config_section_get_string(section, "transform", &s, "normal");
	if (weston_parse_transform(s, &config.output_transform) < 0)
		weston_log("Invalid transform \"%s\" for output fbdev\n", s);
	free(s);

	config.base.struct_version = WESTON_FBDEV_BACKEND_CONFIG_VERSION;
	config.base.struct_size = sizeof(struct weston_fbdev_backend_config);

	/* load the actual wayland backend and configure it */
	ret = load_backend_new(c, backend, &config.base);

	free(config.device);

	return ret;
}

static int
weston_x11_backend_config_append_output_config(struct weston_x11_backend_config *config,
					       struct weston_x11_backend_output_config *output_config) {
	struct weston_x11_backend_output_config *new_outputs;

	new_outputs = realloc(config->outputs, (config->num_outputs+1) *
			      sizeof(struct weston_x11_backend_output_config));
	if (new_outputs == NULL)
		return -1;

	config->outputs = new_outputs;
	config->outputs[config->num_outputs].width = output_config->width;
	config->outputs[config->num_outputs].height = output_config->height;
	config->outputs[config->num_outputs].transform = output_config->transform;
	config->outputs[config->num_outputs].scale = output_config->scale;
	config->outputs[config->num_outputs].name = strdup(output_config->name);
	config->num_outputs++;

	return 0;
}

static int
load_x11_backend(struct weston_compositor *c, char const * backend,
		 int *argc, char **argv, struct weston_config *wc)
{
	struct weston_x11_backend_output_config default_output;
	struct weston_x11_backend_config config = {{ 0, }};
	struct weston_config_section *section;
	int ret = 0;
	int option_width = 0;
	int option_height = 0;
	int option_scale = 0;
	int option_count = 1;
	int output_count = 0;
	char const *section_name;
	int i;
	uint32_t j;

	const struct weston_option options[] = {
	       { WESTON_OPTION_INTEGER, "width", 0, &option_width },
	       { WESTON_OPTION_INTEGER, "height", 0, &option_height },
	       { WESTON_OPTION_INTEGER, "scale", 0, &option_scale },
	       { WESTON_OPTION_BOOLEAN, "fullscreen", 'f', &config.fullscreen },
	       { WESTON_OPTION_INTEGER, "output-count", 0, &option_count },
	       { WESTON_OPTION_BOOLEAN, "no-input", 0, &config.no_input },
	       { WESTON_OPTION_BOOLEAN, "use-pixman", 0, &config.use_pixman },
	};

	parse_options(options, ARRAY_LENGTH(options), argc, argv);

	section = NULL;
	while (weston_config_next_section(wc, &section, &section_name)) {
		struct weston_x11_backend_output_config current_output = { 0, };
		char *t;
		char *mode;

		if (strcmp(section_name, "output") != 0) {
			continue;
		}

		weston_config_section_get_string(section, "name", &current_output.name, NULL);
		if (current_output.name == NULL || current_output.name[0] != 'X') {
			free(current_output.name);
			continue;
		}

		weston_config_section_get_string(section, "mode", &mode, "1024x600");
		if (sscanf(mode, "%dx%d", &current_output.width,
			   &current_output.height) != 2) {
			weston_log("Invalid mode \"%s\" for output %s\n",
				   mode, current_output.name);
			current_output.width = 1024;
			current_output.height = 600;
		}
		free(mode);
		if (current_output.width < 1)
			current_output.width = 1024;
		if (current_output.height < 1)
			current_output.height = 600;
		if (option_width)
			current_output.width = option_width;
		if (option_height)
			current_output.height = option_height;

		weston_config_section_get_int(section, "scale", &current_output.scale, 1);
		if (option_scale)
			current_output.scale = option_scale;

		weston_config_section_get_string(section,
						 "transform", &t, "normal");
		if (weston_parse_transform(t, &current_output.transform) < 0)
			weston_log("Invalid transform \"%s\" for output %s\n",
				   t, current_output.name);
		free(t);

		if (weston_x11_backend_config_append_output_config(&config, &current_output) < 0) {
			ret = -1;
			goto out;
		}

		output_count++;
		if (output_count >= option_count)
			break;
	}

	default_output.name = NULL;
	default_output.width = option_width ? option_width : 1024;
	default_output.height = option_height ? option_height : 600;
	default_output.scale = option_scale ? option_scale : 1;
	default_output.transform = WL_OUTPUT_TRANSFORM_NORMAL;

	for (i = output_count; i < option_count; i++) {
		if (asprintf(&default_output.name, "screen%d", i) < 0) {
			ret = -1;
			goto out;
		}

		if (weston_x11_backend_config_append_output_config(&config, &default_output) < 0) {
			ret = -1;
			free(default_output.name);
			goto out;
		}
		free(default_output.name);
	}

	config.base.struct_version = WESTON_X11_BACKEND_CONFIG_VERSION;
	config.base.struct_size = sizeof(struct weston_x11_backend_config);

	/* load the actual backend and configure it */
	ret = load_backend_new(c, backend, &config.base);

out:
	for (j = 0; j < config.num_outputs; ++j)
		free(config.outputs[j].name);
	free(config.outputs);

	return ret;
}

static void
weston_wayland_output_config_init(struct weston_wayland_backend_output_config *output_config,
				  struct weston_config_section *config_section,
				  int option_width, int option_height,
				  int option_scale)
{
	char *mode, *t, *str;
	unsigned int slen;

	weston_config_section_get_string(config_section, "name", &output_config->name,
					 NULL);
	if (output_config->name) {
		slen = strlen(output_config->name);
		slen += strlen(WINDOW_TITLE " - ");
		str = malloc(slen + 1);
		if (str)
			snprintf(str, slen + 1, WINDOW_TITLE " - %s",
				 output_config->name);
		free(output_config->name);
		output_config->name = str;
	}
	if (!output_config->name)
		output_config->name = strdup(WINDOW_TITLE);

	weston_config_section_get_string(config_section,
					 "mode", &mode, "1024x600");
	if (sscanf(mode, "%dx%d", &output_config->width, &output_config->height) != 2) {
		weston_log("Invalid mode \"%s\" for output %s\n",
			   mode, output_config->name);
		output_config->width = 1024;
		output_config->height = 640;
	}
	free(mode);

	if (option_width)
		output_config->width = option_width;
	if (option_height)
		output_config->height = option_height;

	weston_config_section_get_int(config_section, "scale", &output_config->scale, 1);

	if (option_scale)
		output_config->scale = option_scale;

	weston_config_section_get_string(config_section,
					 "transform", &t, "normal");
	if (weston_parse_transform(t, &output_config->transform) < 0)
		weston_log("Invalid transform \"%s\" for output %s\n",
			   t, output_config->name);
	free(t);

}

static void
weston_wayland_backend_config_release(struct weston_wayland_backend_config *config)
{
	int i;

	for (i = 0; i < config->num_outputs; ++i) {
		free(config->outputs[i].name);
	}
	free(config->cursor_theme);
	free(config->display_name);
	free(config->outputs);
}

/*
 * Append a new output struct at the end of new_config.outputs and return a
 * pointer to the newly allocated structure or NULL if fail. The allocated
 * structure is NOT cleared nor set to default values.
 */
static struct weston_wayland_backend_output_config *
weston_wayland_backend_config_add_new_output(struct weston_wayland_backend_config *config)
{
	struct weston_wayland_backend_output_config *outputs;
	const size_t element_size = sizeof(struct weston_wayland_backend_output_config);

	outputs = realloc(config->outputs,
			  (config->num_outputs + 1) * element_size);
	if (!outputs)
		return NULL;
	config->num_outputs += 1;
	config->outputs = outputs;
	return &(config->outputs[config->num_outputs - 1]);
}

static int
load_wayland_backend_config(struct weston_compositor *compositor, int *argc,
			    char *argv[], struct weston_config *wc,
			    struct weston_wayland_backend_config *config)
{
	struct weston_config_section *section;
	struct weston_wayland_backend_output_config *oc;
	int count, width, height, scale;
	const char *section_name;
	char *name;

	const struct weston_option wayland_options[] = {
		{ WESTON_OPTION_INTEGER, "width", 0, &width },
		{ WESTON_OPTION_INTEGER, "height", 0, &height },
		{ WESTON_OPTION_INTEGER, "scale", 0, &scale },
		{ WESTON_OPTION_STRING, "display", 0, &config->display_name },
		{ WESTON_OPTION_BOOLEAN, "use-pixman", 0, &config->use_pixman },
		{ WESTON_OPTION_INTEGER, "output-count", 0, &count },
		{ WESTON_OPTION_BOOLEAN, "fullscreen", 0, &config->fullscreen },
		{ WESTON_OPTION_BOOLEAN, "sprawl", 0, &config->sprawl },
	};

	width = 0;
	height = 0;
	scale = 0;
	config->display_name = NULL;
	config->use_pixman = 0;
	count = 1;
	config->fullscreen = 0;
	config->sprawl = 0;
	parse_options(wayland_options,
		      ARRAY_LENGTH(wayland_options), argc, argv);

	config->cursor_size = 32;
	config->cursor_theme = NULL;
	config->base.struct_size = sizeof(struct weston_wayland_backend_config);
	config->base.struct_version = WESTON_WAYLAND_BACKEND_CONFIG_VERSION;

	section = weston_config_get_section(wc, "shell", NULL, NULL);
	weston_config_section_get_string(section, "cursor-theme",
					 &config->cursor_theme, NULL);
	weston_config_section_get_int(section, "cursor-size",
				      &config->cursor_size, 32);

	if (config->sprawl) {
		/* do nothing, everything is already set */
		return 0;
	}

	if (config->fullscreen) {
		oc = weston_wayland_backend_config_add_new_output(config);
		if (!oc)
			return -1;

		oc->width = width;
		oc->height = height;
		oc->name = NULL;
		oc->transform = WL_OUTPUT_TRANSFORM_NORMAL;
		oc->scale = 1;

		return 0;
	}

	section = NULL;
	while (weston_config_next_section(wc, &section, &section_name)) {
		if (!section_name || strcmp(section_name, "output") != 0)
			continue;
		weston_config_section_get_string(section, "name", &name, NULL);
		if (name == NULL)
			continue;

		if (name[0] != 'W' || name[1] != 'L') {
			free(name);
			continue;
		}
		free(name);

		oc = weston_wayland_backend_config_add_new_output(config);
		if (!oc)
			return -1;

		weston_wayland_output_config_init(oc, section, width,
						  height, scale);
		--count;
	}

	if (!width)
		width = 1024;
	if (!height)
		height = 640;
	if (!scale)
		scale = 1;

	while (count > 0) {
		oc = weston_wayland_backend_config_add_new_output(config);
		if (!oc)
			return -1;

		oc->width = width;
		oc->height = height;
		oc->name = NULL;
		oc->transform = WL_OUTPUT_TRANSFORM_NORMAL;
		oc->scale = scale;

		--count;
	}

	return 0;
}

static int
load_wayland_backend(struct weston_compositor *c, char const * backend,
		     int *argc, char **argv, struct weston_config *wc)
{
	struct weston_wayland_backend_config config = {{ 0, }};
	int ret = 0;

	ret = load_wayland_backend_config(c, argc, argv, wc, &config);
	if(ret < 0) {
		weston_wayland_backend_config_release(&config);
		return ret;
	}

	/* load the actual wayland backend and configure it */
	ret = load_backend_new(c, backend, &config.base);
	weston_wayland_backend_config_release(&config);
	return ret;
}


static int
load_backend(struct weston_compositor *compositor, const char *backend,
	     int *argc, char **argv, struct weston_config *config)
{
	if (strstr(backend, "headless-backend.so"))
		return load_headless_backend(compositor, backend, argc, argv, config);
	else if (strstr(backend, "rdp-backend.so"))
		return load_rdp_backend(compositor, backend, argc, argv, config);
	else if (strstr(backend, "fbdev-backend.so"))
		return load_fbdev_backend(compositor, backend, argc, argv, config);
	else if (strstr(backend, "drm-backend.so"))
		return load_drm_backend(compositor, backend, argc, argv, config);
	else if (strstr(backend, "x11-backend.so"))
		return load_x11_backend(compositor, backend, argc, argv, config);
	else if (strstr(backend, "wayland-backend.so"))
		return load_wayland_backend(compositor, backend, argc, argv, config);
#if 0
	else if (strstr(backend, "rpi-backend.so"))
		return load_rpi_backend(compositor, backend, argc, argv, config);
#endif

	return load_backend_old(compositor, backend, argc, argv, config);
}

int main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE;
	struct wl_display *display;
	struct weston_compositor *ec;
	struct wl_event_source *signals[4];
	struct wl_event_loop *loop;
	int i, fd;
	char *backend = NULL;
	char *shell = NULL;
	char *modules = NULL;
	char *option_modules = NULL;
	char *log = NULL;
	char *server_socket = NULL, *end;
	int32_t idle_time = -1;
	int32_t help = 0;
	char *socket_name = NULL;
	int32_t version = 0;
	int32_t noconfig = 0;
	int32_t numlock_on;
	char *config_file = NULL;
	struct weston_config *config = NULL;
	struct weston_config_section *section;
	struct wl_client *primary_client;
	struct wl_listener primary_client_destroyed;
	struct weston_seat *seat;

	const struct weston_option core_options[] = {
		{ WESTON_OPTION_STRING, "backend", 'B', &backend },
		{ WESTON_OPTION_STRING, "shell", 0, &shell },
		{ WESTON_OPTION_STRING, "socket", 'S', &socket_name },
		{ WESTON_OPTION_INTEGER, "idle-time", 'i', &idle_time },
		{ WESTON_OPTION_STRING, "modules", 0, &option_modules },
		{ WESTON_OPTION_STRING, "log", 0, &log },
		{ WESTON_OPTION_BOOLEAN, "help", 'h', &help },
		{ WESTON_OPTION_BOOLEAN, "version", 0, &version },
		{ WESTON_OPTION_BOOLEAN, "no-config", 0, &noconfig },
		{ WESTON_OPTION_STRING, "config", 'c', &config_file },
	};

	parse_options(core_options, ARRAY_LENGTH(core_options), &argc, argv);

	if (help)
		usage(EXIT_SUCCESS);

	if (version) {
		printf(PACKAGE_STRING "\n");
		return EXIT_SUCCESS;
	}

	weston_log_file_open(log);

	weston_log("%s\n"
		   STAMP_SPACE "%s\n"
		   STAMP_SPACE "Bug reports to: %s\n"
		   STAMP_SPACE "Build: %s\n",
		   PACKAGE_STRING, PACKAGE_URL, PACKAGE_BUGREPORT,
		   BUILD_ID);
	log_uname();

	verify_xdg_runtime_dir();

	display = wl_display_create();

	loop = wl_display_get_event_loop(display);
	signals[0] = wl_event_loop_add_signal(loop, SIGTERM, on_term_signal,
					      display);
	signals[1] = wl_event_loop_add_signal(loop, SIGINT, on_term_signal,
					      display);
	signals[2] = wl_event_loop_add_signal(loop, SIGQUIT, on_term_signal,
					      display);

	wl_list_init(&child_process_list);
	signals[3] = wl_event_loop_add_signal(loop, SIGCHLD, sigchld_handler,
					      NULL);

	if (!signals[0] || !signals[1] || !signals[2] || !signals[3])
		goto out_signals;

	if (load_configuration(&config, noconfig, config_file) < 0)
		goto out_signals;

	section = weston_config_get_section(config, "core", NULL, NULL);

	if (!backend) {
		weston_config_section_get_string(section, "backend", &backend,
						 NULL);
		if (!backend)
			backend = weston_choose_default_backend();
	}

	ec = weston_compositor_create(display, config);
	if (ec == NULL) {
		weston_log("fatal: failed to create compositor\n");
		goto out;
	}

	ec->config = config;
	if (weston_compositor_init_config(ec, config) < 0)
		goto out;

	if (load_backend(ec, backend, &argc, argv, config) < 0) {
		weston_log("fatal: failed to create compositor backend\n");
		goto out;
	}

	catch_signals();
	segv_compositor = ec;

	if (idle_time < 0)
		weston_config_section_get_int(section, "idle-time", &idle_time, -1);
	if (idle_time < 0)
		idle_time = 300; /* default idle timeout, in seconds */

	ec->idle_time = idle_time;
	ec->default_pointer_grab = NULL;
	ec->exit = handle_exit;

	weston_compositor_log_capabilities(ec);

	server_socket = getenv("WAYLAND_SERVER_SOCKET");
	if (server_socket) {
		weston_log("Running with single client\n");
		fd = strtol(server_socket, &end, 0);
		if (*end != '\0')
			fd = -1;
	} else {
		fd = -1;
	}

	if (fd != -1) {
		primary_client = wl_client_create(display, fd);
		if (!primary_client) {
			weston_log("fatal: failed to add client: %m\n");
			goto out;
		}
		primary_client_destroyed.notify =
			handle_primary_client_destroyed;
		wl_client_add_destroy_listener(primary_client,
					       &primary_client_destroyed);
	} else if (weston_create_listening_socket(display, socket_name)) {
		goto out;
	}

	if (!shell)
		weston_config_section_get_string(section, "shell", &shell,
						 "desktop-shell.so");

	if (load_modules(ec, shell, &argc, argv) < 0)
		goto out;

	weston_config_section_get_string(section, "modules", &modules, "");
	if (load_modules(ec, modules, &argc, argv) < 0)
		goto out;

	if (load_modules(ec, option_modules, &argc, argv) < 0)
		goto out;

	section = weston_config_get_section(config, "keyboard", NULL, NULL);
	weston_config_section_get_bool(section, "numlock-on", &numlock_on, 0);
	if (numlock_on) {
		wl_list_for_each(seat, &ec->seat_list, link) {
			struct weston_keyboard *keyboard =
				weston_seat_get_keyboard(seat);

			if (keyboard)
				weston_keyboard_set_locks(keyboard,
							  WESTON_NUM_LOCK,
							  WESTON_NUM_LOCK);
		}
	}

	for (i = 1; i < argc; i++)
		weston_log("fatal: unhandled option: %s\n", argv[i]);
	if (argc > 1)
		goto out;

	weston_compositor_wake(ec);

	wl_display_run(display);

	/* Allow for setting return exit code after
	* wl_display_run returns normally. This is
	* useful for devs/testers and automated tests
	* that want to indicate failure status to
	* testing infrastructure above
	*/
	ret = ec->exit_code;

out:
	weston_compositor_destroy(ec);

out_signals:
	for (i = ARRAY_LENGTH(signals) - 1; i >= 0; i--)
		if (signals[i])
			wl_event_source_remove(signals[i]);

	wl_display_destroy(display);

	weston_log_file_close();

	if (config)
		weston_config_destroy(config);
	free(config_file);
	free(backend);
	free(shell);
	free(socket_name);
	free(option_modules);
	free(log);
	free(modules);

	return ret;
}
