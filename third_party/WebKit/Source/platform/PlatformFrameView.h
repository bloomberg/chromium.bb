// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformFrameView_h
#define PlatformFrameView_h

#include "platform/PlatformExport.h"

namespace blink {

// PlatformFrameView is a base class for core/frame/FrameView. PlatformFrameView
// is needed to let the platform/ layer access functionalities of FrameView.
class PLATFORM_EXPORT PlatformFrameView {
 public:
  PlatformFrameView() {}
  virtual ~PlatformFrameView() {}

  virtual bool IsFrameView() const { return false; }
};

}  // namespace blink

#endif  // PlatformFrameView_h
