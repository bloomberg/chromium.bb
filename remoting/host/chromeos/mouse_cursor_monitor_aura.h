// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_MOUSE_CURSOR_MONITOR_AURA_H_
#define REMOTING_HOST_CHROMEOS_MOUSE_CURSOR_MONITOR_AURA_H_

#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

// A MouseCursorMonitor place holder implementation for Chrome OS with ozone.
// TODO(kelvinp): Implement this (See crbug.com/431457).
class MouseCursorMonitorAura : public webrtc::MouseCursorMonitor {
 public:
  explicit MouseCursorMonitorAura(const webrtc::DesktopCaptureOptions& options);
  ~MouseCursorMonitorAura() override;

  void Init(Callback* callback, Mode mode) override;
  void Capture() override;

 private:
  Callback* callback_;
  Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(MouseCursorMonitorAura);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_MOUSE_CURSOR_MONITOR_AURA_H_
