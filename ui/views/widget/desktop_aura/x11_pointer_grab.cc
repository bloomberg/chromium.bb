// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ui/views/widget/desktop_aura/x11_pointer_grab.h"

#include <X11/Xlib.h>

namespace views {

namespace {

// The grab window. None if there are no active pointer grabs.
XID g_grab_window = None;

// The "owner events" parameter used to grab the pointer.
bool g_owner_events = false;

}  // namespace

int GrabPointer(XID window, bool owner_events, ::Cursor cursor) {
  int event_mask = PointerMotionMask | ButtonReleaseMask | ButtonPressMask;
  int result = XGrabPointer(gfx::GetXDisplay(), window, owner_events,
                            event_mask, GrabModeAsync, GrabModeAsync, None,
                            cursor, CurrentTime);
  if (result == GrabSuccess) {
    g_grab_window = window;
    g_owner_events = owner_events;
  }
  return result;
}

void ChangeActivePointerGrabCursor(::Cursor cursor) {
  DCHECK(g_grab_window != None);
  GrabPointer(g_grab_window, g_owner_events, cursor);
}

void UngrabPointer() {
  g_grab_window = None;
  XUngrabPointer(gfx::GetXDisplay(), CurrentTime);
}

}  // namespace views
