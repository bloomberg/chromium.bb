// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_platform_specific.h"

#include "build/build_config.h"

#if !defined(OS_LINUX)

// static
std::unique_ptr<OpaqueBrowserFrameViewPlatformSpecific>
OpaqueBrowserFrameViewPlatformSpecific::Create(
    OpaqueBrowserFrameView* view,
    OpaqueBrowserFrameViewLayout* layout) {
  return std::make_unique<OpaqueBrowserFrameViewPlatformSpecific>();
}

#endif
