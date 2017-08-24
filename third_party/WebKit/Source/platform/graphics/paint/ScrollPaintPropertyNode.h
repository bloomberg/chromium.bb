// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollPaintPropertyNode_h
#define ScrollPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/graphics/paint/PaintPropertyNode.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "platform/wtf/text/WTFString.h"

#include <iosfwd>

namespace blink {

using MainThreadScrollingReasons = uint32_t;
class WebLayerScrollClient;

// A scroll node contains auxiliary scrolling information which includes how far
// an area can be scrolled, main thread scrolling reasons, etc. Scroll nodes
// are referenced by TransformPaintPropertyNodes that are used for the scroll
// offset translation, though scroll offset translation can exist without a
// scroll node (e.g., overflow: hidden).
//
// Main thread scrolling reasons force scroll updates to go to the main thread
// and can have dependencies on other nodes. For example, all parents of a
// scroll node with background attachment fixed set should also have it set.
//
// The scroll tree differs from the other trees because it does not affect
// geometry directly.
class PLATFORM_EXPORT ScrollPaintPropertyNode
    : public PaintPropertyNode<ScrollPaintPropertyNode> {
 public:
  // This node is really a sentinel, and does not represent a real scroll.
  static ScrollPaintPropertyNode* Root();

  static PassRefPtr<ScrollPaintPropertyNode> Create(
      PassRefPtr<const ScrollPaintPropertyNode> parent,
      const IntPoint& bounds_offset,
      const IntSize& container_bounds,
      const IntSize& bounds,
      bool user_scrollable_horizontal,
      bool user_scrollable_vertical,
      MainThreadScrollingReasons main_thread_scrolling_reasons,
      CompositorElementId compositor_element_id,
      WebLayerScrollClient* scroll_client) {
    return AdoptRef(new ScrollPaintPropertyNode(
        std::move(parent), bounds_offset, container_bounds, bounds,
        user_scrollable_horizontal, user_scrollable_vertical,
        main_thread_scrolling_reasons, compositor_element_id, scroll_client));
  }

  bool Update(PassRefPtr<const ScrollPaintPropertyNode> parent,
              const IntPoint& bounds_offset,
              const IntSize& container_bounds,
              const IntSize& bounds,
              bool user_scrollable_horizontal,
              bool user_scrollable_vertical,
              MainThreadScrollingReasons main_thread_scrolling_reasons,
              CompositorElementId compositor_element_id,
              WebLayerScrollClient* scroll_client) {
    bool parent_changed = PaintPropertyNode::Update(std::move(parent));

    if (bounds_offset == bounds_offset_ &&
        container_bounds == container_bounds_ && bounds == bounds_ &&
        user_scrollable_horizontal == user_scrollable_horizontal_ &&
        user_scrollable_vertical == user_scrollable_vertical_ &&
        main_thread_scrolling_reasons == main_thread_scrolling_reasons_ &&
        compositor_element_id_ == compositor_element_id &&
        scroll_client == scroll_client_)
      return parent_changed;

    SetChanged();
    bounds_offset_ = bounds_offset;
    container_bounds_ = container_bounds;
    bounds_ = bounds;
    user_scrollable_horizontal_ = user_scrollable_horizontal;
    user_scrollable_vertical_ = user_scrollable_vertical;
    main_thread_scrolling_reasons_ = main_thread_scrolling_reasons;
    compositor_element_id_ = compositor_element_id;
    DCHECK(ElementIdNamespaceIsForScrolling());
    scroll_client_ = scroll_client;
    return true;
  }

  // Offset for |ContainerBounds| and |Bounds|.
  const IntPoint& Offset() const { return bounds_offset_; }

  // Size of the container area that the contents scrolls in, not including
  // non-overlay scrollbars. Overlay scrollbars do not affect these bounds.
  const IntSize& ContainerBounds() const { return container_bounds_; }

  // Size of the content that is scrolled within the container bounds.
  const IntSize& Bounds() const { return bounds_; }

  bool UserScrollableHorizontal() const { return user_scrollable_horizontal_; }
  bool UserScrollableVertical() const { return user_scrollable_vertical_; }

  // Return reason bitfield with values from cc::MainThreadScrollingReason.
  MainThreadScrollingReasons GetMainThreadScrollingReasons() const {
    return main_thread_scrolling_reasons_;
  }

  // Main thread scrolling reason for the threaded scrolling disabled setting.
  bool ThreadedScrollingDisabled() const {
    return main_thread_scrolling_reasons_ &
           MainThreadScrollingReason::kThreadedScrollingDisabled;
  }

  // Main thread scrolling reason for background attachment fixed descendants.
  bool HasBackgroundAttachmentFixedDescendants() const {
    return main_thread_scrolling_reasons_ &
           MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  }

  const CompositorElementId& GetCompositorElementId() const {
    return compositor_element_id_;
  }

  WebLayerScrollClient* ScrollClient() const { return scroll_client_; }

#if DCHECK_IS_ON()
  // The clone function is used by FindPropertiesNeedingUpdate.h for recording
  // a scroll node before it has been updated, to later detect changes.
  PassRefPtr<ScrollPaintPropertyNode> Clone() const {
    RefPtr<ScrollPaintPropertyNode> cloned =
        AdoptRef(new ScrollPaintPropertyNode(
            Parent(), bounds_offset_, container_bounds_, bounds_,
            user_scrollable_horizontal_, user_scrollable_vertical_,
            main_thread_scrolling_reasons_, compositor_element_id_,
            scroll_client_));
    return cloned;
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if a scroll node has changed.
  bool operator==(const ScrollPaintPropertyNode& o) const {
    return Parent() == o.Parent() && bounds_offset_ == o.bounds_offset_ &&
           container_bounds_ == o.container_bounds_ && bounds_ == o.bounds_ &&
           user_scrollable_horizontal_ == o.user_scrollable_horizontal_ &&
           user_scrollable_vertical_ == o.user_scrollable_vertical_ &&
           main_thread_scrolling_reasons_ == o.main_thread_scrolling_reasons_ &&
           compositor_element_id_ == o.compositor_element_id_ &&
           scroll_client_ == o.scroll_client_;
  }

  String ToTreeString() const;
#endif

  String ToString() const;

 private:
  ScrollPaintPropertyNode(
      PassRefPtr<const ScrollPaintPropertyNode> parent,
      IntPoint bounds_offset,
      IntSize container_bounds,
      IntSize bounds,
      bool user_scrollable_horizontal,
      bool user_scrollable_vertical,
      MainThreadScrollingReasons main_thread_scrolling_reasons,
      CompositorElementId compositor_element_id,
      WebLayerScrollClient* scroll_client)
      : PaintPropertyNode(std::move(parent)),
        bounds_offset_(bounds_offset),
        container_bounds_(container_bounds),
        bounds_(bounds),
        user_scrollable_horizontal_(user_scrollable_horizontal),
        user_scrollable_vertical_(user_scrollable_vertical),
        main_thread_scrolling_reasons_(main_thread_scrolling_reasons),
        compositor_element_id_(compositor_element_id),
        scroll_client_(scroll_client) {
    DCHECK(ElementIdNamespaceIsForScrolling());
  }

  bool ElementIdNamespaceIsForScrolling() const {
    return !compositor_element_id_ ||
           NamespaceFromCompositorElementId(compositor_element_id_) ==
               CompositorElementIdNamespace::kScroll;
  }

  IntPoint bounds_offset_;
  IntSize container_bounds_;
  IntSize bounds_;
  bool user_scrollable_horizontal_ : 1;
  bool user_scrollable_vertical_ : 1;
  MainThreadScrollingReasons main_thread_scrolling_reasons_;
  // The scrolling element id is stored directly on the scroll node and not on
  // the associated TransformPaintPropertyNode used for scroll offset.
  CompositorElementId compositor_element_id_;
  // TODO(pdr): This can be destroyed and be not safe to use. Refactor this to
  // use ElementIds for safety (crbug.com/758360).
  WebLayerScrollClient* scroll_client_;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const ScrollPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // ScrollPaintPropertyNode_h
