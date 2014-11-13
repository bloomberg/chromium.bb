// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/mouse_cursor_monitor_aura.h"

#include "base/logging.h"

namespace remoting {

MouseCursorMonitorAura::MouseCursorMonitorAura(
    const webrtc::DesktopCaptureOptions& options)
    : callback_(nullptr),
      mode_(SHAPE_AND_POSITION) {
}

MouseCursorMonitorAura::~MouseCursorMonitorAura() {
  NOTIMPLEMENTED();
}

void MouseCursorMonitorAura::Init(Callback* callback, Mode mode) {
  DCHECK(!callback_);
  DCHECK(callback);

  callback_ = callback;
  mode_ = mode;

  NOTIMPLEMENTED();
}

void MouseCursorMonitorAura::Capture() {
  NOTIMPLEMENTED();
}

}  // namespace remoting
