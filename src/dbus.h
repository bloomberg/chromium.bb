/*
 * Copyright © 2013 David Herrmann <dh.herrmann@gmail.com>
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

#ifndef _WESTON_DBUS_H_
#define _WESTON_DBUS_H_

#include "config.h"

#include <errno.h>
#include <wayland-server.h>

#include "compositor.h"

#ifdef HAVE_DBUS

#include <dbus/dbus.h>

/*
 * weston_dbus_open() - Open new dbus connection
 *
 * Opens a new dbus connection to the bus given as @bus. It automatically
 * integrates the new connection into the main-loop @loop. The connection
 * itself is returned in @out.
 * This also returns a context source used for dbus dispatching. It is
 * returned on success in @ctx_out and must be passed to weston_dbus_close()
 * unchanged. You must not access it from outside of a dbus helper!
 *
 * Returns 0 on success, negative error code on failure.
 */
int weston_dbus_open(struct wl_event_loop *loop, DBusBusType bus,
		     DBusConnection **out, struct wl_event_source **ctx_out);

/*
 * weston_dbus_close() - Close dbus connection
 *
 * Closes a dbus connection that was previously opened via weston_dbus_open().
 * It unbinds the connection from the main-loop it was previously bound to,
 * closes the dbus connection and frees all resources. If you want to access
 * @c after this call returns, you must hold a dbus-reference to it. But
 * notice that the connection is closed after this returns so it cannot be
 * used to spawn new dbus requests.
 * You must pass the context source returns by weston_dbus_open() as @ctx.
 */
void weston_dbus_close(DBusConnection *c, struct wl_event_source *ctx);

/*
 * weston_dbus_add_match() - Add dbus match
 *
 * Configure a dbus-match on the given dbus-connection. This match is saved
 * on the dbus-server as long as the connection is open. See dbus-manual
 * for information. Compared to the dbus_bus_add_match() this allows a
 * var-arg formatted match-string.
 */
int weston_dbus_add_match(DBusConnection *c, const char *format, ...);

/*
 * weston_dbus_add_match_signal() - Add dbus signal match
 *
 * Same as weston_dbus_add_match() but does the dbus-match formatting for
 * signals internally.
 */
int weston_dbus_add_match_signal(DBusConnection *c, const char *sender,
				 const char *iface, const char *member,
				 const char *path);

/*
 * weston_dbus_remove_match() - Remove dbus match
 *
 * Remove a previously configured dbus-match from the dbus server. There is
 * no need to remove dbus-matches if you close the connection, anyway.
 * Compared to dbus_bus_remove_match() this allows a var-arg formatted
 * match string.
 */
void weston_dbus_remove_match(DBusConnection *c, const char *format, ...);

/*
 * weston_dbus_remove_match_signal() - Remove dbus signal match
 *
 * Same as weston_dbus_remove_match() but does the dbus-match formatting for
 * signals internally.
 */
void weston_dbus_remove_match_signal(DBusConnection *c, const char *sender,
				     const char *iface, const char *member,
				     const char *path);

#endif /* HAVE_DBUS */

#endif // _WESTON_DBUS_H_
