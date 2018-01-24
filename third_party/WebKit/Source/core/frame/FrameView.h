// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameView_h
#define FrameView_h

#include "core/dom/DocumentLifecycle.h"
#include "core/frame/EmbeddedContentView.h"
#include "core/frame/IntrinsicSizingInfo.h"

namespace blink {

class CORE_EXPORT FrameView : public EmbeddedContentView {
 public:
  virtual ~FrameView() = default;
  virtual void UpdateViewportIntersectionsForSubtree(
      DocumentLifecycle::LifecycleState) = 0;

  virtual bool GetIntrinsicSizingInfo(IntrinsicSizingInfo&) const = 0;
  virtual bool HasIntrinsicSizingInfo() const = 0;

  bool IsFrameView() const override { return true; }
};

DEFINE_TYPE_CASTS(FrameView,
                  EmbeddedContentView,
                  embedded_content_view,
                  embedded_content_view->IsFrameView(),
                  embedded_content_view.IsFrameView());

}  // namespace blink

#endif  // FrameView_h
