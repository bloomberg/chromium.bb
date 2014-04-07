/*
 * Copyright © 2011 Intel Corporation
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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include "xwayland.h"


static int
handle_sigusr1(int signal_number, void *data)
{
	struct weston_xserver *wxs = data;

	/* We'd be safer if we actually had the struct
	 * signalfd_siginfo from the signalfd data and could verify
	 * this came from Xwayland.*/
	weston_wm_create(wxs, wxs->wm_fd);
	wl_event_source_remove(wxs->sigusr1_source);

	return 1;
}

static int
weston_xserver_handle_event(int listen_fd, uint32_t mask, void *data)
{
	struct weston_xserver *wxs = data;
	char display[8], s[8], abstract_fd[8], unix_fd[8], wm_fd[8];
	int sv[2], wm[2], fd;
	char *xserver = NULL;
	struct weston_config_section *section;

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) < 0) {
		weston_log("wl connection socketpair failed\n");
		return 1;
	}

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, wm) < 0) {
		weston_log("X wm connection socketpair failed\n");
		return 1;
	}

	wxs->process.pid = fork();
	switch (wxs->process.pid) {
	case 0:
		/* SOCK_CLOEXEC closes both ends, so we need to unset
		 * the flag on the client fd. */
		fd = dup(sv[1]);
		if (fd < 0)
			goto fail;
		snprintf(s, sizeof s, "%d", fd);
		setenv("WAYLAND_SOCKET", s, 1);

		snprintf(display, sizeof display, ":%d", wxs->display);

		fd = dup(wxs->abstract_fd);
		if (fd < 0)
			goto fail;
		snprintf(abstract_fd, sizeof abstract_fd, "%d", fd);
		fd = dup(wxs->unix_fd);
		if (fd < 0)
			goto fail;
		snprintf(unix_fd, sizeof unix_fd, "%d", fd);
		fd = dup(wm[1]);
		if (fd < 0)
			goto fail;
		snprintf(wm_fd, sizeof wm_fd, "%d", fd);

		section = weston_config_get_section(wxs->compositor->config,
						    "xwayland", NULL, NULL);
		weston_config_section_get_string(section, "path",
						 &xserver, XSERVER_PATH);

		/* Ignore SIGUSR1 in the child, which will make the X
		 * server send SIGUSR1 to the parent (weston) when
		 * it's done with initialization.  During
		 * initialization the X server will round trip and
		 * block on the wayland compositor, so avoid making
		 * blocking requests (like xcb_connect_to_fd) until
		 * it's done with that. */
		signal(SIGUSR1, SIG_IGN);

		if (execl(xserver,
			  xserver,
			  display,
			  "-rootless",
			  "-listen", abstract_fd,
			  "-listen", unix_fd,
			  "-wm", wm_fd,
			  "-terminate",
			  NULL) < 0)
			weston_log("exec failed: %m\n");
	fail:
		_exit(EXIT_FAILURE);

	default:
		weston_log("forked X server, pid %d\n", wxs->process.pid);

		close(sv[1]);
		wxs->client = wl_client_create(wxs->wl_display, sv[0]);

		close(wm[1]);
		wxs->wm_fd = wm[0];

		weston_watch_process(&wxs->process);

		wl_event_source_remove(wxs->abstract_source);
		wl_event_source_remove(wxs->unix_source);
		break;

	case -1:
		weston_log( "failed to fork\n");
		break;
	}

	return 1;
}

static void
weston_xserver_shutdown(struct weston_xserver *wxs)
{
	char path[256];

	snprintf(path, sizeof path, "/tmp/.X%d-lock", wxs->display);
	unlink(path);
	snprintf(path, sizeof path, "/tmp/.X11-unix/X%d", wxs->display);
	unlink(path);
	if (wxs->process.pid == 0) {
		wl_event_source_remove(wxs->abstract_source);
		wl_event_source_remove(wxs->unix_source);
	}
	close(wxs->abstract_fd);
	close(wxs->unix_fd);
	if (wxs->wm)
		weston_wm_destroy(wxs->wm);
	wxs->loop = NULL;
}

static void
weston_xserver_cleanup(struct weston_process *process, int status)
{
	struct weston_xserver *wxs =
		container_of(process, struct weston_xserver, process);

	wxs->process.pid = 0;
	wxs->client = NULL;
	wxs->resource = NULL;

	wxs->abstract_source =
		wl_event_loop_add_fd(wxs->loop, wxs->abstract_fd,
				     WL_EVENT_READABLE,
				     weston_xserver_handle_event, wxs);

	wxs->unix_source =
		wl_event_loop_add_fd(wxs->loop, wxs->unix_fd,
				     WL_EVENT_READABLE,
				     weston_xserver_handle_event, wxs);

	if (wxs->wm) {
		weston_log("xserver exited, code %d\n", status);
		weston_wm_destroy(wxs->wm);
		wxs->wm = NULL;
	} else {
		/* If the X server crashes before it binds to the
		 * xserver interface, shut down and don't try
		 * again. */
		weston_log("xserver crashing too fast: %d\n", status);
		weston_xserver_shutdown(wxs);
	}
}

static int
bind_to_abstract_socket(int display)
{
	struct sockaddr_un addr;
	socklen_t size, name_size;
	int fd;

	fd = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;

	addr.sun_family = AF_LOCAL;
	name_size = snprintf(addr.sun_path, sizeof addr.sun_path,
			     "%c/tmp/.X11-unix/X%d", 0, display);
	size = offsetof(struct sockaddr_un, sun_path) + name_size;
	if (bind(fd, (struct sockaddr *) &addr, size) < 0) {
		weston_log("failed to bind to @%s: %s\n",
			addr.sun_path + 1, strerror(errno));
		close(fd);
		return -1;
	}

	if (listen(fd, 1) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static int
bind_to_unix_socket(int display)
{
	struct sockaddr_un addr;
	socklen_t size, name_size;
	int fd;

	fd = socket(PF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -1;

	addr.sun_family = AF_LOCAL;
	name_size = snprintf(addr.sun_path, sizeof addr.sun_path,
			     "/tmp/.X11-unix/X%d", display) + 1;
	size = offsetof(struct sockaddr_un, sun_path) + name_size;
	unlink(addr.sun_path);
	if (bind(fd, (struct sockaddr *) &addr, size) < 0) {
		weston_log("failed to bind to %s (%s)\n",
			addr.sun_path, strerror(errno));
		close(fd);
		return -1;
	}

	if (listen(fd, 1) < 0) {
		unlink(addr.sun_path);
		close(fd);
		return -1;
	}

	return fd;
}

static int
create_lockfile(int display, char *lockfile, size_t lsize)
{
	char pid[16], *end;
	int fd, size;
	pid_t other;

	snprintf(lockfile, lsize, "/tmp/.X%d-lock", display);
	fd = open(lockfile, O_WRONLY | O_CLOEXEC | O_CREAT | O_EXCL, 0444);
	if (fd < 0 && errno == EEXIST) {
		fd = open(lockfile, O_CLOEXEC | O_RDONLY);
		if (fd < 0 || read(fd, pid, 11) != 11) {
			weston_log("can't read lock file %s: %s\n",
				lockfile, strerror(errno));
			if (fd >= 0)
				close (fd);

			errno = EEXIST;
			return -1;
		}

		other = strtol(pid, &end, 0);
		if (end != pid + 10) {
			weston_log("can't parse lock file %s\n",
				lockfile);
			close(fd);
			errno = EEXIST;
			return -1;
		}

		if (kill(other, 0) < 0 && errno == ESRCH) {
			/* stale lock file; unlink and try again */
			weston_log("unlinking stale lock file %s\n", lockfile);
			close(fd);
			if (unlink(lockfile))
				/* If we fail to unlink, return EEXIST
				   so we try the next display number.*/
				errno = EEXIST;
			else
				errno = EAGAIN;
			return -1;
		}

		close(fd);
		errno = EEXIST;
		return -1;
	} else if (fd < 0) {
		weston_log("failed to create lock file %s: %s\n",
			lockfile, strerror(errno));
		return -1;
	}

	/* Subtle detail: we use the pid of the wayland
	 * compositor, not the xserver in the lock file. */
	size = snprintf(pid, sizeof pid, "%10d\n", getpid());
	if (write(fd, pid, size) != size) {
		unlink(lockfile);
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}

static void
weston_xserver_destroy(struct wl_listener *l, void *data)
{
	struct weston_xserver *wxs =
		container_of(l, struct weston_xserver, destroy_listener);

	if (!wxs)
		return;

	if (wxs->loop)
		weston_xserver_shutdown(wxs);

	free(wxs);
}

WL_EXPORT int
module_init(struct weston_compositor *compositor,
	    int *argc, char *argv[])

{
	struct wl_display *display = compositor->wl_display;
	struct weston_xserver *wxs;
	char lockfile[256], display_name[8];

	wxs = zalloc(sizeof *wxs);
	wxs->process.cleanup = weston_xserver_cleanup;
	wxs->wl_display = display;
	wxs->compositor = compositor;

	wxs->display = 0;

 retry:
	if (create_lockfile(wxs->display, lockfile, sizeof lockfile) < 0) {
		if (errno == EAGAIN) {
			goto retry;
		} else if (errno == EEXIST) {
			wxs->display++;
			goto retry;
		} else {
			free(wxs);
			return -1;
		}
	}

	wxs->abstract_fd = bind_to_abstract_socket(wxs->display);
	if (wxs->abstract_fd < 0 && errno == EADDRINUSE) {
		wxs->display++;
		unlink(lockfile);
		goto retry;
	}

	wxs->unix_fd = bind_to_unix_socket(wxs->display);
	if (wxs->unix_fd < 0) {
		unlink(lockfile);
		close(wxs->abstract_fd);
		free(wxs);
		return -1;
	}

	snprintf(display_name, sizeof display_name, ":%d", wxs->display);
	weston_log("xserver listening on display %s\n", display_name);
	setenv("DISPLAY", display_name, 1);

	wxs->loop = wl_display_get_event_loop(display);
	wxs->abstract_source =
		wl_event_loop_add_fd(wxs->loop, wxs->abstract_fd,
				     WL_EVENT_READABLE,
				     weston_xserver_handle_event, wxs);
	wxs->unix_source =
		wl_event_loop_add_fd(wxs->loop, wxs->unix_fd,
				     WL_EVENT_READABLE,
				     weston_xserver_handle_event, wxs);

	wxs->sigusr1_source = wl_event_loop_add_signal(wxs->loop, SIGUSR1,
						       handle_sigusr1, wxs);
	wxs->destroy_listener.notify = weston_xserver_destroy;
	wl_signal_add(&compositor->destroy_signal, &wxs->destroy_listener);

	return 0;
}
