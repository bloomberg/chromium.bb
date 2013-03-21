// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/media_switches.h"

namespace switches {

#if defined(OS_ANDROID)
// Always use external video surface. Normally, external video surfaces are used
// sparingly.
const char kUseExternalVideoSurface[] = "use-external-video-surface";
#endif

}  // namespace switches
