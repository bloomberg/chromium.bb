// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrameView_h
#define RemoteFrameView_h

#include "core/frame/FrameOrPlugin.h"
#include "core/frame/FrameView.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"

namespace blink {

class CullRect;
class GraphicsContext;
class RemoteFrame;

class RemoteFrameView final : public GarbageCollectedFinalized<RemoteFrameView>,
                              public FrameOrPlugin {
  USING_GARBAGE_COLLECTED_MIXIN(RemoteFrameView);

 public:
  static RemoteFrameView* Create(RemoteFrame*);

  ~RemoteFrameView() override;

  void SetParent(FrameView*) override;
  FrameView* Parent() const override { return parent_; }

  RemoteFrame& GetFrame() const {
    DCHECK(remote_frame_);
    return *remote_frame_;
  }

  void Dispose() override;
  // Override to notify remote frame that its viewport size has changed.
  void FrameRectsChanged() override;
  void InvalidateRect(const IntRect&);
  void SetFrameRect(const IntRect&) override;
  const IntRect& FrameRect() const override { return frame_rect_; }
  void Paint(GraphicsContext&, const CullRect&) const override {}
  void Hide() override;
  void Show() override;
  void SetParentVisible(bool) override;

  void UpdateRemoteViewportIntersection();

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit RemoteFrameView(RemoteFrame*);

  IntRect ConvertFromRootFrame(const IntRect&) const;

  // The properties and handling of the cycle between RemoteFrame
  // and its RemoteFrameView corresponds to that between LocalFrame
  // and FrameView. Please see the FrameView::m_frame comment for
  // details.
  Member<RemoteFrame> remote_frame_;
  Member<FrameView> parent_;
  IntRect last_viewport_intersection_;
  IntRect frame_rect_;
  bool self_visible_;
  bool parent_visible_;
};

}  // namespace blink

#endif  // RemoteFrameView_h
