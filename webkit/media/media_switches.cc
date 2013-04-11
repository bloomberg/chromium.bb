// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/media_switches.h"

namespace switches {

#if defined(OS_ANDROID)
// Use external video surface for video with more than or equal pixels to
// specified value. For example, value of 0 will enable external video surface
// for all videos, and value of 921600 (=1280*720) will enable external video
// surface for 720p video and larger.
const char kUseExternalVideoSurfaceThresholdInPixels[] =
    "use-external-video-surface-threshold-in-pixels";
#endif

}  // namespace switches
