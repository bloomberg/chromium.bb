// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wayland/wayland_message_pump.h"

#include <wayland-client.h>

#include "ui/wayland/wayland_display.h"

namespace ui {

WaylandMessagePump::WaylandMessagePump(WaylandDisplay* display)
    : display_(display) {
  static GSourceFuncs kSourceHandlers = {
    WaylandMessagePump::SourcePrepare,
    WaylandMessagePump::SourceCheck,
    WaylandMessagePump::SourceDispatch,
    NULL
  };

  source_ = static_cast<WorkSource*>(
      g_source_new(&kSourceHandlers, sizeof(WorkSource)));
  source_->pump = this;
  pfd_.fd = wl_display_get_fd(display_->display(),
                              WaylandMessagePump::SourceUpdate,
                              source_);
  pfd_.events = G_IO_IN | G_IO_ERR;
  g_source_add_poll(source_, &pfd_);
  g_source_attach(source_, NULL);
}

WaylandMessagePump::~WaylandMessagePump() {
  g_source_destroy(source_);
  g_source_unref(source_);
}

int WaylandMessagePump::HandlePrepare() {
  while (mask_ & WL_DISPLAY_WRITABLE)
    wl_display_iterate(display_->display(), WL_DISPLAY_WRITABLE);

  return -1;
}

bool WaylandMessagePump::HandleCheck() {
  return pfd_.revents;
}

void WaylandMessagePump::HandleDispatch() {
  wl_display_iterate(display_->display(), WL_DISPLAY_READABLE);
}

// static
gboolean WaylandMessagePump::SourcePrepare(GSource* source, gint* timeout) {
  *timeout = static_cast<WorkSource*>(source)->pump->HandlePrepare();
  return FALSE;
}

// static
gboolean WaylandMessagePump::SourceCheck(GSource* source) {
  return static_cast<WorkSource*>(source)->pump->HandleCheck();
}

// static
gboolean WaylandMessagePump::SourceDispatch(GSource* source,
                                            GSourceFunc callback,
                                            gpointer data) {
  static_cast<WorkSource*>(source)->pump->HandleDispatch();
  return TRUE;
}

// static
int WaylandMessagePump::SourceUpdate(uint32_t mask, void* data) {
  static_cast<WorkSource*>(data)->pump->mask_ = mask;
  return 0;
}

}  // namespace ui
