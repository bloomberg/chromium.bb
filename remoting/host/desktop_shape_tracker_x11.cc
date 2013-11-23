// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_shape_tracker.h"

#include "base/memory/scoped_ptr.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"

namespace remoting {

scoped_ptr<DesktopShapeTracker> DesktopShapeTracker::Create(
    webrtc::DesktopCaptureOptions options) {
  return scoped_ptr<DesktopShapeTracker>();
}

}  // namespace remoting
