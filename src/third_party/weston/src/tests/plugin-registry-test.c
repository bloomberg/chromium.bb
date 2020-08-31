/*
 * Copyright (C) 2016 DENSO CORPORATION
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

#include <libweston/libweston.h>
#include "compositor/weston.h"
#include <libweston/plugin-registry.h>

static void
dummy_func(void)
{
}

static const struct my_api {
	void (*func1)(void);
	void (*func2)(void);
	void (*func3)(void);
} my_test_api = {
	dummy_func,
	dummy_func,
	dummy_func,
};

#define MY_API_NAME "test_my_api_v1"

static void
init_tests(struct weston_compositor *compositor)
{
	assert(weston_plugin_api_get(compositor, MY_API_NAME,
				     sizeof(my_test_api)) == NULL);

	assert(weston_plugin_api_register(compositor, MY_API_NAME, &my_test_api,
					  sizeof(my_test_api)) == 0);

	assert(weston_plugin_api_register(compositor, MY_API_NAME, &my_test_api,
					  sizeof(my_test_api)) == -2);

	assert(weston_plugin_api_get(compositor, MY_API_NAME,
				     sizeof(my_test_api)) == &my_test_api);

	assert(weston_plugin_api_register(compositor, "another", &my_test_api,
					  sizeof(my_test_api)) == 0);
}

static void
runtime_tests(void *data)
{
	struct weston_compositor *compositor = data;
	const struct my_api *api;
	size_t sz = sizeof(struct my_api);

	assert(weston_plugin_api_get(compositor, MY_API_NAME, sz) ==
				     &my_test_api);

	assert(weston_plugin_api_get(compositor, MY_API_NAME, sz - 4) ==
				     &my_test_api);

	assert(weston_plugin_api_get(compositor, MY_API_NAME, sz + 4) == NULL);

	api = weston_plugin_api_get(compositor, MY_API_NAME, sz);
	assert(api && api->func2 == dummy_func);

	weston_compositor_exit(compositor);
}

WL_EXPORT int
wet_module_init(struct weston_compositor *compositor,
		int *argc, char *argv[])
{
	struct wl_event_loop *loop;

	init_tests(compositor);

	loop = wl_display_get_event_loop(compositor->wl_display);
	wl_event_loop_add_idle(loop, runtime_tests, compositor);

	return 0;
}
