// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_MOUSE_CURSOR_MONITOR_H_
#define REMOTING_HOST_FAKE_MOUSE_CURSOR_MONITOR_H_

#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

class FakeMouseCursorMonitor : public webrtc::MouseCursorMonitor {
 public:
  FakeMouseCursorMonitor();
  virtual ~FakeMouseCursorMonitor();

  virtual void Init(Callback* callback, Mode mode) OVERRIDE;
  virtual void Capture() OVERRIDE;

 private:
  Callback* callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeMouseCursorMonitor);
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_MOUSE_CURSOR_MONITOR_H_
