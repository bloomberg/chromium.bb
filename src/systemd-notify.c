/*
 * Copyright (c) 2015 General Electric Company. All rights reserved.
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
#include <systemd/sd-daemon.h>
#include <wayland-server.h>
#include "shared/helpers.h"
#include "shared/zalloc.h"
#include "compositor.h"

struct systemd_notifier {
	int watchdog_time;
	struct wl_event_source *watchdog_source;
	struct wl_listener compositor_destroy_listener;
};

static int
watchdog_handler(void *data)
{
	struct systemd_notifier *notifier = data;

	wl_event_source_timer_update(notifier->watchdog_source,
				     notifier->watchdog_time);

	sd_notify(0, "WATCHDOG=1");

	return 1;
}

static void
weston_compositor_destroy_listener(struct wl_listener *listener, void *data)
{
	struct systemd_notifier *notifier;

	sd_notify(0, "STOPPING=1");

	notifier = container_of(listener,
				struct systemd_notifier,
				compositor_destroy_listener);

	if (notifier->watchdog_source)
		wl_event_source_remove(notifier->watchdog_source);

	wl_list_remove(&notifier->compositor_destroy_listener.link);
	free(notifier);
}

WL_EXPORT int
module_init(struct weston_compositor *compositor,
	    int *argc, char *argv[])
{
	char *tail;
	char *watchdog_time_env;
	struct wl_event_loop *loop;
	long watchdog_time_conv;
	struct systemd_notifier *notifier;

	notifier = zalloc(sizeof *notifier);
	if (notifier == NULL)
		return -1;

	notifier->compositor_destroy_listener.notify =
		weston_compositor_destroy_listener;
	wl_signal_add(&compositor->destroy_signal,
		      &notifier->compositor_destroy_listener);

	sd_notify(0, "READY=1");

	/* 'WATCHDOG_USEC' is environment variable that is set
	 * by systemd to transfer 'WatchdogSec' watchdog timeout
	 * setting from service file.*/
	watchdog_time_env = getenv("WATCHDOG_USEC");

	if (!watchdog_time_env)
		return 0;

	errno = 0;
	watchdog_time_conv = strtol(watchdog_time_env, &tail, 0);
	if ((errno != 0) || (*tail != '\0'))
		return 0;

	/* Convert 'WATCHDOG_USEC' to milliseconds and notify
	 * systemd every half of that time.*/
	watchdog_time_conv /= 1000 * 2;
	if (watchdog_time_conv <= 0)
		return 0;

	notifier->watchdog_time = watchdog_time_conv;

	loop = wl_display_get_event_loop(compositor->wl_display);
	notifier->watchdog_source =
		wl_event_loop_add_timer(loop, watchdog_handler, notifier);
	wl_event_source_timer_update(notifier->watchdog_source,
				     notifier->watchdog_time);

	return 0;
}

