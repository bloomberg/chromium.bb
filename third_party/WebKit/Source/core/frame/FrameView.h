// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameView_h
#define FrameView_h

#include "core/dom/DocumentLifecycle.h"
#include "core/frame/EmbeddedContentView.h"

namespace blink {

class CORE_EXPORT FrameView : public EmbeddedContentView {
 public:
  virtual ~FrameView() = default;
  virtual void UpdateViewportIntersectionsForSubtree(
      DocumentLifecycle::LifecycleState) = 0;
};

}  // namespace blink

#endif  // FrameView_h
