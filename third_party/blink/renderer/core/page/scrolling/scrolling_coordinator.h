/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLLING_COORDINATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLLING_COORDINATOR_H_

#include <memory>
#include "base/macros.h"
#include "third_party/blink/public/platform/web_layer_scroll_client.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/layer_hit_test_rects.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scroll/main_thread_scrolling_reason.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
using MainThreadScrollingReasons = uint32_t;

class CompositorAnimationHost;
class CompositorAnimationTimeline;
class LayoutBox;
class LocalFrame;
class LocalFrameView;
class GraphicsLayer;
class Page;
class PaintLayer;
class Region;
class ScrollableArea;
class WebLayerTreeView;
class WebScrollbarLayer;

using ScrollbarId = uint64_t;

// ScrollingCoordinator is a page-level object that mediates interactions
// between Blink and the compositor's scroll-related APIs on WebLayer and
// WebScrollbarLayer.
//
// It's responsible for propagating scroll offsets, main-thread scrolling
// reasons, touch action regions, and non-fast-scrollable regions into the
// compositor, as well as creating and managing scrollbar layers.

class CORE_EXPORT ScrollingCoordinator final
    : public GarbageCollectedFinalized<ScrollingCoordinator>,
      public WebLayerScrollClient {
 public:
  static ScrollingCoordinator* Create(Page*);

  ~ScrollingCoordinator();
  void Trace(blink::Visitor*);

  // The LocalFrameView argument is optional, nullptr causes the the scrolling
  // animation host and timeline to be owned by the ScrollingCoordinator. When
  // not null, the host and timeline are attached to the specified
  // LocalFrameView. A LocalFrameView only needs to own them when it is the view
  // for an OOPIF.
  void LayerTreeViewInitialized(WebLayerTreeView&, LocalFrameView*);
  void WillCloseLayerTreeView(WebLayerTreeView&, LocalFrameView*);

  void WillBeDestroyed();

  // Return whether this scrolling coordinator handles scrolling for the given
  // frame view.
  bool CoordinatesScrollingForFrameView(LocalFrameView*) const;

  // Called when any frame has done its layout or compositing has changed.
  void NotifyGeometryChanged(LocalFrameView*);
  // Called when any layoutBox has transform changed
  void NotifyTransformChanged(LocalFrame*, const LayoutBox&);

  void UpdateAfterCompositingChangeIfNeeded(LocalFrameView*);

  // Should be called whenever the slow repaint objects counter changes between
  // zero and one.
  void FrameViewHasBackgroundAttachmentFixedObjectsDidChange(LocalFrameView*);

  // Should be called whenever the set of fixed objects changes.
  void FrameViewFixedObjectsDidChange(LocalFrameView*);

  // Should be called whenever the root layer for the given frame view changes.
  void FrameViewRootLayerDidChange(LocalFrameView*);

  std::unique_ptr<WebScrollbarLayer> CreateSolidColorScrollbarLayer(
      ScrollbarOrientation,
      int thumb_thickness,
      int track_start,
      bool is_left_side_vertical_scrollbar);

  void WillDestroyScrollableArea(ScrollableArea*);
  // Updates the compositor layers and returns true if the scrolling coordinator
  // handled this change.
  // TODO(pdr): Factor the container bounds change out of this function. The
  // compositor tracks scroll container bounds on the scroll layer whereas
  // blink uses a separate layer. To ensure the compositor scroll layer has the
  // updated scroll container bounds, this needs to be called when the scrolling
  // contents layer is resized.
  bool ScrollableAreaScrollLayerDidChange(ScrollableArea*);
  void ScrollableAreaScrollbarLayerDidChange(ScrollableArea*,
                                             ScrollbarOrientation);
  void UpdateLayerPositionConstraint(PaintLayer*);
  // LocalFrame* must be a local root if non-null.
  void TouchEventTargetRectsDidChange(LocalFrame*);
  void WillDestroyLayer(PaintLayer*);

  void UpdateScrollParentForGraphicsLayer(GraphicsLayer* child,
                                          const PaintLayer* parent);
  void UpdateClipParentForGraphicsLayer(GraphicsLayer* child,
                                        const PaintLayer* parent);
  Region ComputeShouldHandleScrollGestureOnMainThreadRegion(
      const LocalFrame*) const;

  void UpdateTouchEventTargetRectsIfNeeded(LocalFrame*);

  void UpdateUserInputScrollable(ScrollableArea*);

  CompositorAnimationHost* GetCompositorAnimationHost() {
    return animation_host_.get();
  }
  CompositorAnimationTimeline* GetCompositorAnimationTimeline() {
    return programmatic_scroll_animator_timeline_.get();
  }

  // Callback for compositor-side layer scrolls.
  void DidScroll(const gfx::ScrollOffset&, const CompositorElementId&) final;

  // For testing purposes only. This ScrollingCoordinator is reused between
  // layout test, and must be reset for the results to be valid.
  void Reset(LocalFrame*);

 protected:
  explicit ScrollingCoordinator(Page*);

  bool IsForRootLayer(ScrollableArea*) const;
  bool IsForMainFrame(ScrollableArea*) const;

  Member<Page> page_;

  // Dirty flags used to idenfity what really needs to be computed after
  // compositing is updated.
  bool touch_event_target_rects_are_dirty_;
  bool should_scroll_on_main_thread_dirty_;

 private:
  void SetShouldUpdateScrollLayerPositionOnMainThread(
      LocalFrame*,
      MainThreadScrollingReasons);

  void SetShouldHandleScrollGestureOnMainThreadRegion(const Region&,
                                                      LocalFrameView*);
  void SetTouchEventTargetRects(LocalFrame*, LayerHitTestRects&);
  void ComputeTouchEventTargetRects(LocalFrame*, LayerHitTestRects&);

  WebScrollbarLayer* AddWebScrollbarLayer(ScrollableArea*,
                                          ScrollbarOrientation,
                                          std::unique_ptr<WebScrollbarLayer>);
  WebScrollbarLayer* GetWebScrollbarLayer(ScrollableArea*,
                                          ScrollbarOrientation);
  void RemoveWebScrollbarLayer(ScrollableArea*, ScrollbarOrientation);

  bool FrameScrollerIsDirty(LocalFrameView*) const;

  std::unique_ptr<CompositorAnimationHost> animation_host_;
  std::unique_ptr<CompositorAnimationTimeline>
      programmatic_scroll_animator_timeline_;

  using ScrollbarMap =
      HeapHashMap<Member<ScrollableArea>, std::unique_ptr<WebScrollbarLayer>>;
  ScrollbarMap horizontal_scrollbars_;
  ScrollbarMap vertical_scrollbars_;

  DISALLOW_COPY_AND_ASSIGN(ScrollingCoordinator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SCROLLING_COORDINATOR_H_
