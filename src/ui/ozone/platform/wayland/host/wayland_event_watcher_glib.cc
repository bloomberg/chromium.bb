// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_event_watcher_glib.h"

#include <glib.h>

namespace ui {

namespace {

struct GLibWaylandSource : public GSource {
  // Note: The GLibWaylandSource is created and destroyed by GLib. So its
  // constructor/destructor may or may not get called.
  WaylandEventWatcherGlib* event_watcher;
  GPollFD* poll_fd;
};

gboolean WatchSourcePrepare(GSource* source, gint* timeout_ms) {
  // Set an infinite timeout.
  *timeout_ms = -1;

  auto* event_watcher_glib =
      static_cast<GLibWaylandSource*>(source)->event_watcher;
  if (event_watcher_glib->HandlePrepare())
    return FALSE;

  // Return true if there are events to dispatch without polling.
  return TRUE;
}

gboolean WatchSourceCheck(GSource* source) {
  auto* glib_wayland_source = static_cast<GLibWaylandSource*>(source);
  gushort flags = glib_wayland_source->poll_fd->revents;
  glib_wayland_source->event_watcher->HandleCheck(flags & G_IO_IN);
  return flags;
}

gboolean WatchSourceDispatch(GSource* source,
                             GSourceFunc unused_func,
                             gpointer data) {
  auto* event_watcher_glib =
      static_cast<GLibWaylandSource*>(source)->event_watcher;
  event_watcher_glib->HandleDispatch();
  return TRUE;
}

GSourceFuncs WatchSourceFuncs = {WatchSourcePrepare, WatchSourceCheck,
                                 WatchSourceDispatch, nullptr};

}  // namespace

WaylandEventWatcherGlib::WaylandEventWatcherGlib(wl_display* display,
                                                 wl_event_queue* event_queue)
    : WaylandEventWatcher(display, event_queue) {}

WaylandEventWatcherGlib::~WaylandEventWatcherGlib() {
  StopProcessingEvents();
}

bool WaylandEventWatcherGlib::StartWatchingFD(int fd) {
  if (started_)
    return true;

  wayland_poll_ = std::make_unique<GPollFD>();
  wayland_poll_->fd = fd;
  wayland_poll_->events = G_IO_IN | G_IO_ERR | G_IO_HUP;
  wayland_poll_->revents = 0;

  GLibWaylandSource* glib_wayland_source = static_cast<GLibWaylandSource*>(
      g_source_new(&WatchSourceFuncs, sizeof(GLibWaylandSource)));
  glib_wayland_source->event_watcher = this;
  glib_wayland_source->poll_fd = wayland_poll_.get();

  wayland_source_ = glib_wayland_source;
  g_source_add_poll(wayland_source_, wayland_poll_.get());
  g_source_set_can_recurse(wayland_source_, TRUE);
  auto* context = g_main_context_get_thread_default();
  if (!context)
    context = g_main_context_default();
  g_source_attach(wayland_source_, context);

  started_ = true;
  return true;
}

void WaylandEventWatcherGlib::StopWatchingFD() {
  if (!started_)
    return;

  g_source_destroy(wayland_source_);
  g_source_unref(wayland_source_);

  started_ = false;
}

bool WaylandEventWatcherGlib::HandlePrepare() {
  return WlDisplayPrepareToRead();
}

void WaylandEventWatcherGlib::HandleCheck(bool is_io_in) {
  if (is_io_in)
    WlDisplayReadEvents();
  else
    WlDisplayCancelRead();
}

void WaylandEventWatcherGlib::HandleDispatch() {
  WlDisplayDispatchPendingQueue();
}

// static
std::unique_ptr<WaylandEventWatcher>
WaylandEventWatcher::CreateWaylandEventWatcher(wl_display* display,
                                               wl_event_queue* event_queue) {
  return std::make_unique<WaylandEventWatcherGlib>(display, event_queue);
}

}  // namespace ui
