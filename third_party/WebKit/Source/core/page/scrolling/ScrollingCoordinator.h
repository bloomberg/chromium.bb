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

#include "core/CoreExport.h"
#include "core/paint/LayerHitTestRects.h"
#include "platform/geometry/IntRect.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "platform/scroll/ScrollTypes.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {
using MainThreadScrollingReasons = uint32_t;

class CompositorAnimationHost;
class CompositorAnimationTimeline;
class LayoutBox;
class LocalFrame;
class FrameView;
class GraphicsLayer;
class Page;
class PaintLayer;
class Region;
class ScrollableArea;
class WebLayerTreeView;
class WebScrollbarLayer;

class CORE_EXPORT ScrollingCoordinator final
    : public GarbageCollectedFinalized<ScrollingCoordinator> {
  WTF_MAKE_NONCOPYABLE(ScrollingCoordinator);

 public:
  static ScrollingCoordinator* create(Page*);

  ~ScrollingCoordinator();
  DECLARE_TRACE();

  // The FrameView argument is optional, nullptr causes the the scrolling
  // animation host and timeline to be owned by the ScrollingCoordinator. When
  // not null, the host and timeline are attached to the specified FrameView.
  // A FrameView only needs to own them when it is the view for an OOPIF.
  void layerTreeViewInitialized(WebLayerTreeView&, FrameView*);
  void willCloseLayerTreeView(WebLayerTreeView&, FrameView*);

  void willBeDestroyed();

  // Return whether this scrolling coordinator handles scrolling for the given
  // frame view.
  bool coordinatesScrollingForFrameView(FrameView*) const;

  // Called when any frame has done its layout or compositing has changed.
  void notifyGeometryChanged();
  // Called when any frame recalculates its overflows after style change.
  void notifyOverflowUpdated();
  // Called when any layoutBox has transform changed
  void notifyTransformChanged(const LayoutBox&);

  void updateAfterCompositingChangeIfNeeded();

  // Should be called whenever a frameview visibility is changed.
  void frameViewVisibilityDidChange();

  // Should be called whenever a scrollable area is added or removed, or
  // gains/loses a composited layer.
  void scrollableAreasDidChange();

  // Should be called whenever the slow repaint objects counter changes between
  // zero and one.
  void frameViewHasBackgroundAttachmentFixedObjectsDidChange(FrameView*);

  // Should be called whenever the set of fixed objects changes.
  void frameViewFixedObjectsDidChange(FrameView*);

  // Should be called whenever the root layer for the given frame view changes.
  void frameViewRootLayerDidChange(FrameView*);

  std::unique_ptr<WebScrollbarLayer> createSolidColorScrollbarLayer(
      ScrollbarOrientation,
      int thumbThickness,
      int trackStart,
      bool isLeftSideVerticalScrollbar);

  void willDestroyScrollableArea(ScrollableArea*);
  // Returns true if the coordinator handled this change.
  bool scrollableAreaScrollLayerDidChange(ScrollableArea*);
  void scrollableAreaScrollbarLayerDidChange(ScrollableArea*,
                                             ScrollbarOrientation);
  void setLayerIsContainerForFixedPositionLayers(GraphicsLayer*, bool);
  void updateLayerPositionConstraint(PaintLayer*);
  void touchEventTargetRectsDidChange();
  void willDestroyLayer(PaintLayer*);

  void updateScrollParentForGraphicsLayer(GraphicsLayer* child,
                                          const PaintLayer* parent);
  void updateClipParentForGraphicsLayer(GraphicsLayer* child,
                                        const PaintLayer* parent);
  Region computeShouldHandleScrollGestureOnMainThreadRegion(
      const LocalFrame*) const;

  void updateTouchEventTargetRectsIfNeeded();

  CompositorAnimationHost* compositorAnimationHost() {
    return m_animationHost.get();
  }
  CompositorAnimationTimeline* compositorAnimationTimeline() {
    return m_programmaticScrollAnimatorTimeline.get();
  }

  // For testing purposes only. This ScrollingCoordinator is reused between
  // layout test, and must be reset for the results to be valid.
  void reset();

 protected:
  explicit ScrollingCoordinator(Page*);

  bool isForRootLayer(ScrollableArea*) const;
  bool isForMainFrame(ScrollableArea*) const;

  Member<Page> m_page;

  // Dirty flags used to idenfity what really needs to be computed after
  // compositing is updated.
  bool m_scrollGestureRegionIsDirty;
  bool m_touchEventTargetRectsAreDirty;
  bool m_shouldScrollOnMainThreadDirty;

 private:
  bool shouldUpdateAfterCompositingChange() const {
    return m_scrollGestureRegionIsDirty || m_touchEventTargetRectsAreDirty ||
           m_shouldScrollOnMainThreadDirty || frameScrollerIsDirty();
  }

  void setShouldUpdateScrollLayerPositionOnMainThread(
      MainThreadScrollingReasons);

  void setShouldHandleScrollGestureOnMainThreadRegion(const Region&);
  void setTouchEventTargetRects(LayerHitTestRects&);
  void computeTouchEventTargetRects(LayerHitTestRects&);

  WebScrollbarLayer* addWebScrollbarLayer(ScrollableArea*,
                                          ScrollbarOrientation,
                                          std::unique_ptr<WebScrollbarLayer>);
  WebScrollbarLayer* getWebScrollbarLayer(ScrollableArea*,
                                          ScrollbarOrientation);
  void removeWebScrollbarLayer(ScrollableArea*, ScrollbarOrientation);

  bool frameScrollerIsDirty() const;

  std::unique_ptr<CompositorAnimationHost> m_animationHost;
  std::unique_ptr<CompositorAnimationTimeline>
      m_programmaticScrollAnimatorTimeline;

  using ScrollbarMap =
      HeapHashMap<Member<ScrollableArea>, std::unique_ptr<WebScrollbarLayer>>;
  ScrollbarMap m_horizontalScrollbars;
  ScrollbarMap m_verticalScrollbars;
  HashSet<const PaintLayer*> m_layersWithTouchRects;
  bool m_wasFrameScrollable;

  MainThreadScrollingReasons m_lastMainThreadScrollingReasons;
};

}  // namespace blink

#endif  // ScrollingCoordinator_h
