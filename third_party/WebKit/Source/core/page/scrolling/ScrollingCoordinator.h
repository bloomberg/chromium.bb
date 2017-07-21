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

#ifndef ScrollingCoordinator_h
#define ScrollingCoordinator_h

#include <memory>
#include "core/CoreExport.h"
#include "core/paint/LayerHitTestRects.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"

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
    : public GarbageCollectedFinalized<ScrollingCoordinator> {
  WTF_MAKE_NONCOPYABLE(ScrollingCoordinator);

 public:
  static ScrollingCoordinator* Create(Page*);

  ~ScrollingCoordinator();
  DECLARE_TRACE();

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
  void NotifyGeometryChanged();
  // Called when any frame recalculates its overflows after style change.
  void NotifyOverflowUpdated();
  // Called when any layoutBox has transform changed
  void NotifyTransformChanged(const LayoutBox&);

  void UpdateAfterCompositingChangeIfNeeded();

  // Should be called whenever a frameview visibility is changed.
  void FrameViewVisibilityDidChange();

  // Should be called whenever a scrollable area is added or removed, or
  // gains/loses a composited layer.
  void ScrollableAreasDidChange();

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
  // Returns true if the coordinator handled this change.
  bool ScrollableAreaScrollLayerDidChange(ScrollableArea*);
  void ScrollableAreaScrollbarLayerDidChange(ScrollableArea*,
                                             ScrollbarOrientation);
  void SetLayerIsContainerForFixedPositionLayers(GraphicsLayer*, bool);
  void UpdateLayerPositionConstraint(PaintLayer*);
  void TouchEventTargetRectsDidChange();
  void WillDestroyLayer(PaintLayer*);

  void UpdateScrollParentForGraphicsLayer(GraphicsLayer* child,
                                          const PaintLayer* parent);
  void UpdateClipParentForGraphicsLayer(GraphicsLayer* child,
                                        const PaintLayer* parent);
  Region ComputeShouldHandleScrollGestureOnMainThreadRegion(
      const LocalFrame*) const;

  void UpdateTouchEventTargetRectsIfNeeded();

  void UpdateUserInputScrollable(ScrollableArea*);

  CompositorAnimationHost* GetCompositorAnimationHost() {
    return animation_host_.get();
  }
  CompositorAnimationTimeline* GetCompositorAnimationTimeline() {
    return programmatic_scroll_animator_timeline_.get();
  }

  // For testing purposes only. This ScrollingCoordinator is reused between
  // layout test, and must be reset for the results to be valid.
  void Reset();

 protected:
  explicit ScrollingCoordinator(Page*);

  bool IsForRootLayer(ScrollableArea*) const;
  bool IsForMainFrame(ScrollableArea*) const;

  Member<Page> page_;

  // Dirty flags used to idenfity what really needs to be computed after
  // compositing is updated.
  bool scroll_gesture_region_is_dirty_;
  bool touch_event_target_rects_are_dirty_;
  bool should_scroll_on_main_thread_dirty_;

 private:
  bool ShouldUpdateAfterCompositingChange() const {
    return scroll_gesture_region_is_dirty_ ||
           touch_event_target_rects_are_dirty_ ||
           should_scroll_on_main_thread_dirty_ || FrameScrollerIsDirty();
  }

  void SetShouldUpdateScrollLayerPositionOnMainThread(
      MainThreadScrollingReasons);

  void SetShouldHandleScrollGestureOnMainThreadRegion(const Region&);
  void SetTouchEventTargetRects(LayerHitTestRects&);
  void ComputeTouchEventTargetRects(LayerHitTestRects&);

  WebScrollbarLayer* AddWebScrollbarLayer(ScrollableArea*,
                                          ScrollbarOrientation,
                                          std::unique_ptr<WebScrollbarLayer>);
  WebScrollbarLayer* GetWebScrollbarLayer(ScrollableArea*,
                                          ScrollbarOrientation);
  void RemoveWebScrollbarLayer(ScrollableArea*, ScrollbarOrientation);

  bool FrameScrollerIsDirty() const;

  std::unique_ptr<CompositorAnimationHost> animation_host_;
  std::unique_ptr<CompositorAnimationTimeline>
      programmatic_scroll_animator_timeline_;

  using ScrollbarMap =
      HeapHashMap<Member<ScrollableArea>, std::unique_ptr<WebScrollbarLayer>>;
  ScrollbarMap horizontal_scrollbars_;
  ScrollbarMap vertical_scrollbars_;
  HashSet<const PaintLayer*> layers_with_touch_rects_;
  bool was_frame_scrollable_;

  MainThreadScrollingReasons last_main_thread_scrolling_reasons_;
};

}  // namespace blink

#endif  // ScrollingCoordinator_h
