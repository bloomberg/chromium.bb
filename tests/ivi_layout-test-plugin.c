/*
 * Copyright © 2012 Intel Corporation
 * Copyright © 2013 DENSO CORPORATION
 * Copyright © 2015 Collabora, Ltd.
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
#include <string.h>

#include "src/compositor.h"
#include "weston-test-server-protocol.h"
#include "ivi-test.h"
#include "ivi-shell/ivi-layout-export.h"

struct test_context;

struct runner_test {
	const char *name;
	void (*run)(struct test_context *);
} __attribute__ ((aligned (32)));

#define RUNNER_TEST(name)					\
	static void runner_func_##name(struct test_context *);	\
								\
	const struct runner_test runner_test_##name		\
		__attribute__ ((section ("test_section"))) =	\
	{							\
		#name, runner_func_##name			\
	};							\
								\
	static void runner_func_##name(struct test_context *ctx)

extern const struct runner_test __start_test_section;
extern const struct runner_test __stop_test_section;

static const struct runner_test *
find_runner_test(const char *name)
{
	const struct runner_test *t;

	for (t = &__start_test_section; t < &__stop_test_section; t++) {
		if (strcmp(t->name, name) == 0)
			return t;
	}

	return NULL;
}

struct test_launcher {
	struct weston_compositor *compositor;
	char exe[2048];
	struct weston_process process;
	const struct ivi_controller_interface *controller_interface;
};

static void
runner_destroy_handler(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

struct test_context {
	const struct ivi_controller_interface *controller_interface;
	struct wl_resource *runner_resource;
};

static void
runner_run_handler(struct wl_client *client, struct wl_resource *resource,
		   const char *test_name)
{
	struct test_launcher *launcher;
	const struct runner_test *t;
	struct test_context ctx;

	launcher = wl_resource_get_user_data(resource);
	ctx.controller_interface = launcher->controller_interface;
	ctx.runner_resource = resource;

	t = find_runner_test(test_name);
	if (!t) {
		weston_log("Error: runner test \"%s\" not found.\n",
			   test_name);
		wl_resource_post_error(resource,
				       WESTON_TEST_RUNNER_ERROR_UNKNOWN_TEST,
				       "weston_test_runner: unknown: '%s'",
				       test_name);
		return;
	}

	weston_log("weston_test_runner.run(\"%s\")\n", test_name);

	t->run(&ctx);

	weston_test_runner_send_finished(resource);
}

static const struct weston_test_runner_interface runner_implementation = {
	runner_destroy_handler,
	runner_run_handler
};

static void
bind_runner(struct wl_client *client, void *data,
	    uint32_t version, uint32_t id)
{
	struct test_launcher *launcher = data;
	struct wl_resource *resource;

	resource = wl_resource_create(client, &weston_test_runner_interface,
				      1, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &runner_implementation,
				       launcher, NULL);
}

static void
test_client_sigchld(struct weston_process *process, int status)
{
	struct test_launcher *launcher =
		container_of(process, struct test_launcher, process);
	struct weston_compositor *c = launcher->compositor;

	/* Chain up from weston-test-runner's exit code so that automake
	 * knows the exit status and can report e.g. skipped tests. */
	if (WIFEXITED(status))
		weston_compositor_exit_with_code(c, WEXITSTATUS(status));
	else
		weston_compositor_exit_with_code(c, EXIT_FAILURE);
}

static void
idle_launch_client(void *data)
{
	struct test_launcher *launcher = data;
	pid_t pid;
	sigset_t allsigs;

	pid = fork();
	if (pid == -1) {
		weston_log("fatal: failed to fork '%s': %m\n", launcher->exe);
		weston_compositor_exit_with_code(launcher->compositor,
						 EXIT_FAILURE);
		return;
	}

	if (pid == 0) {
		sigfillset(&allsigs);
		sigprocmask(SIG_UNBLOCK, &allsigs, NULL);
		execl(launcher->exe, launcher->exe, NULL);
		weston_log("compositor: executing '%s' failed: %m\n",
			   launcher->exe);
		_exit(EXIT_FAILURE);
	}

	launcher->process.pid = pid;
	launcher->process.cleanup = test_client_sigchld;
	weston_watch_process(&launcher->process);
}

int
controller_module_init(struct weston_compositor *compositor,
		       int *argc, char *argv[],
		       const struct ivi_controller_interface *iface,
		       size_t iface_version);

WL_EXPORT int
controller_module_init(struct weston_compositor *compositor,
		       int *argc, char *argv[],
		       const struct ivi_controller_interface *iface,
		       size_t iface_version)
{
	struct wl_event_loop *loop;
	struct test_launcher *launcher;
	const char *path;

	/* strict check, since this is an internal test module */
	if (iface_version != sizeof(*iface)) {
		weston_log("fatal: controller interface mismatch\n");
		return -1;
	}

	path = getenv("WESTON_BUILD_DIR");
	if (!path) {
		weston_log("test setup failure: WESTON_BUILD_DIR not set\n");
		return -1;
	}

	launcher = zalloc(sizeof *launcher);
	if (!launcher)
		return -1;

	launcher->compositor = compositor;
	launcher->controller_interface = iface;
	snprintf(launcher->exe, sizeof launcher->exe,
		 "%s/ivi-layout.ivi", path);

	if (wl_global_create(compositor->wl_display,
			     &weston_test_runner_interface, 1,
			     launcher, bind_runner) == NULL)
		return -1;

	loop = wl_display_get_event_loop(compositor->wl_display);
	wl_event_loop_add_idle(loop, idle_launch_client, launcher);

	return 0;
}

static void
runner_assert_fail(const char *cond, const char *file, int line,
		   const char *func, struct test_context *ctx)
{
	weston_log("Assert failure in %s:%d, %s: '%s'\n",
		   file, line, func, cond);
	wl_resource_post_error(ctx->runner_resource,
			       WESTON_TEST_RUNNER_ERROR_TEST_FAILED,
			       "Assert failure in %s:%d, %s: '%s'\n",
			       file, line, func, cond);
}

#define runner_assert(cond) ({					\
	bool b_ = (cond);					\
	if (!b_)						\
		runner_assert_fail(#cond, __FILE__, __LINE__,	\
				   __func__, ctx);		\
	b_;							\
})

#define runner_assert_or_return(cond) do {			\
	bool b_ = (cond);					\
	if (!b_) {						\
		runner_assert_fail(#cond, __FILE__, __LINE__,	\
				   __func__, ctx);		\
		return;						\
	}							\
} while (0)


/*************************** tests **********************************/

/*
 * This is a controller module: a plugin to ivi-shell.so, i.e. a sub-plugin.
 * This module is specially written to execute tests that target the
 * ivi_layout API.
 *
 * This module is listed in TESTS in Makefile.am. weston-tests-env handles
 * this module specially by loading it in ivi-shell.
 *
 * Once Weston init completes, this module launches one test program:
 * ivi-layout.ivi (ivi_layout-test.c). That program uses the weston-test-runner
 * framework to fork and exec each TEST() in ivi_layout-test.c with a fresh
 * connection to the single compositor instance.
 *
 * Each TEST() in ivi_layout-test.c will bind to weston_test_runner global
 * interface. A TEST() will set up the client state, and issue
 * weston_test_runner.run request to execute the compositor-side of the test.
 *
 * The compositor-side parts of the tests are in this file. They are specified
 * by RUNNER_TEST() macro, where the name argument matches the name string
 * passed to weston_test_runner.run.
 *
 * A RUNNER_TEST() function simply returns when it succeeds. If it fails,
 * a fatal protocol error is sent to the client from runner_assert() or
 * runner_assert_or_return(). This module catches the test program exit
 * code and passes it out of Weston to the test harness.
 *
 * A single TEST() in ivi_layout-test.c may use multiple RUNNER_TEST()s to
 * achieve multiple test points over a client action sequence.
 */

RUNNER_TEST(surface_create_p1)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_surface *ivisurf[2];
	uint32_t ivi_id;

	ivisurf[0] = ctl->get_surface_from_id(IVI_TEST_SURFACE_ID(0));
	runner_assert_or_return(ivisurf[0]);

	ivisurf[1] = ctl->get_surface_from_id(IVI_TEST_SURFACE_ID(1));
	runner_assert_or_return(ivisurf[1]);

	ivi_id = ctl->get_id_of_surface(ivisurf[0]);
	runner_assert_or_return(ivi_id == IVI_TEST_SURFACE_ID(0));

	ivi_id = ctl->get_id_of_surface(ivisurf[1]);
	runner_assert_or_return(ivi_id == IVI_TEST_SURFACE_ID(1));
}

RUNNER_TEST(surface_create_p2)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_surface *ivisurf;

	/* the ivi_surface was destroyed by the client */
	ivisurf = ctl->get_surface_from_id(IVI_TEST_SURFACE_ID(0));
	runner_assert_or_return(ivisurf == NULL);
}

RUNNER_TEST(surface_visibility)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_surface *ivisurf;
	int32_t ret;
	bool visibility;
	const struct ivi_layout_surface_properties *prop;

	ivisurf = ctl->get_surface_from_id(IVI_TEST_SURFACE_ID(0));
	runner_assert_or_return(ivisurf);

	ret = ctl->surface_set_visibility(ivisurf, true);
	runner_assert_or_return(ret == IVI_SUCCEEDED);

	ctl->commit_changes();

	visibility = ctl->surface_get_visibility(ivisurf);
	runner_assert_or_return(visibility == true);

	prop = ctl->get_properties_of_surface(ivisurf);
	runner_assert_or_return(prop->visibility == true);
}

RUNNER_TEST(surface_opacity)
{
	const struct ivi_controller_interface *ctl = ctx->controller_interface;
	struct ivi_layout_surface *ivisurf;
	int32_t ret;
	wl_fixed_t opacity;
	const struct ivi_layout_surface_properties *prop;

	ivisurf = ctl->get_surface_from_id(IVI_TEST_SURFACE_ID(0));
	runner_assert_or_return(ivisurf);

	ret = ctl->surface_set_opacity(ivisurf, wl_fixed_from_double(0.5));
	runner_assert_or_return(ret == IVI_SUCCEEDED);

	ctl->commit_changes();

	opacity = ctl->surface_get_opacity(ivisurf);
	runner_assert_or_return(opacity == wl_fixed_from_double(0.5));

	prop = ctl->get_properties_of_surface(ivisurf);
	runner_assert_or_return(prop->opacity == wl_fixed_from_double(0.5));
}
