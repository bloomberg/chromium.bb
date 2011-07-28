// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WAYLAND_WAYLAND_MESSAGE_PUMP_H_
#define UI_WAYLAND_WAYLAND_MESSAGE_PUMP_H_

#include <glib.h>
#include <stdint.h>

#include "base/basictypes.h"

namespace ui {

class WaylandDisplay;

// The message pump handles Wayland specific event delivery. This message
// pump uses the default glib context, so running a glib main loop with the
// default context will allow looping through Wayland events.
class WaylandMessagePump {
 public:
  explicit WaylandMessagePump(WaylandDisplay* display);
  virtual ~WaylandMessagePump();

 protected:
  // These are used to process the pump callbacks.
  // HandlePrepare: is called during glib's prepare step and returns a timeout
  //                that will be passed to the poll.
  // HandleCheck:   called after HandlePrepare and returns whether
  //                HandleDispatch should be called.
  // HandleDispatch:is called after HandleCheck returns true and it will
  //                dispatch a Wayland event.
  virtual int HandlePrepare();
  virtual bool HandleCheck();
  virtual void HandleDispatch();

 private:
  struct WorkSource : public GSource {
    WaylandMessagePump* pump;
  };

  // Actual callbacks for glib. These functions will just call the appropriate
  // Handle* functions in a WaylandMessagePump object.
  static gboolean SourcePrepare(GSource* source, gint* timeout);
  static gboolean SourceCheck(GSource* source);
  static gboolean SourceDispatch(GSource* source,
                                 GSourceFunc callback,
                                 gpointer data);
  // Handles updates to the Wayland mask. This is used to signal when the
  // compositor is done processing writable events.
  static int SourceUpdate(uint32_t mask, void* data);

  WaylandDisplay* display_;
  WorkSource* source_;
  GPollFD pfd_;
  uint32_t mask_;

  DISALLOW_COPY_AND_ASSIGN(WaylandMessagePump);
};

}  // namespace ui

#endif  // UI_WAYLAND_WAYLAND_MESSAGE_PUMP_H_
