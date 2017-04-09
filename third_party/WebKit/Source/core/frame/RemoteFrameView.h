// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrameView_h
#define RemoteFrameView_h

#include "platform/FrameViewBase.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"

namespace blink {

class RemoteFrame;

class RemoteFrameView final : public FrameViewBase {
 public:
  static RemoteFrameView* Create(RemoteFrame*);

  ~RemoteFrameView() override;

  bool IsRemoteFrameView() const override { return true; }
  void SetParent(FrameViewBase*) override;

  RemoteFrame& GetFrame() const {
    ASSERT(remote_frame_);
    return *remote_frame_;
  }

  void Dispose() override;
  // Override to notify remote frame that its viewport size has changed.
  void FrameRectsChanged() override;
  void InvalidateRect(const IntRect&) override;
  void SetFrameRect(const IntRect&) override;
  void Hide() override;
  void Show() override;
  void SetParentVisible(bool) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit RemoteFrameView(RemoteFrame*);

  void UpdateRemoteViewportIntersection();

  // The properties and handling of the cycle between RemoteFrame
  // and its RemoteFrameView corresponds to that between LocalFrame
  // and FrameView. Please see the FrameView::m_frame comment for
  // details.
  Member<RemoteFrame> remote_frame_;

  IntRect last_viewport_intersection_;
};

DEFINE_TYPE_CASTS(RemoteFrameView,
                  FrameViewBase,
                  frameViewBase,
                  frameViewBase->IsRemoteFrameView(),
                  frameViewBase.IsRemoteFrameView());

}  // namespace blink

#endif  // RemoteFrameView_h
