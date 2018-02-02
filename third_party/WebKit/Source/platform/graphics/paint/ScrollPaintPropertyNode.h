// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollPaintPropertyNode_h
#define ScrollPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/CompositorElementId.h"
#include "platform/graphics/paint/PaintPropertyNode.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "platform/wtf/text/WTFString.h"

#include <iosfwd>

namespace blink {

using MainThreadScrollingReasons = uint32_t;

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

  static scoped_refptr<ScrollPaintPropertyNode> Create(
      scoped_refptr<const ScrollPaintPropertyNode> parent,
      const IntRect& container_rect,
      const IntRect& contents_rect,
      bool user_scrollable_horizontal,
      bool user_scrollable_vertical,
      MainThreadScrollingReasons main_thread_scrolling_reasons,
      CompositorElementId compositor_element_id) {
    return base::AdoptRef(new ScrollPaintPropertyNode(
        std::move(parent), container_rect, contents_rect,
        user_scrollable_horizontal, user_scrollable_vertical,
        main_thread_scrolling_reasons, compositor_element_id));
  }

  bool Update(scoped_refptr<const ScrollPaintPropertyNode> parent,
              const IntRect& container_rect,
              const IntRect& contents_rect,
              bool user_scrollable_horizontal,
              bool user_scrollable_vertical,
              MainThreadScrollingReasons main_thread_scrolling_reasons,
              CompositorElementId compositor_element_id) {
    bool parent_changed = PaintPropertyNode::Update(std::move(parent));

    if (container_rect == container_rect_ && contents_rect == contents_rect_ &&
        user_scrollable_horizontal == user_scrollable_horizontal_ &&
        user_scrollable_vertical == user_scrollable_vertical_ &&
        main_thread_scrolling_reasons == main_thread_scrolling_reasons_ &&
        compositor_element_id_ == compositor_element_id)
      return parent_changed;

    SetChanged();
    container_rect_ = container_rect;
    contents_rect_ = contents_rect;
    user_scrollable_horizontal_ = user_scrollable_horizontal;
    user_scrollable_vertical_ = user_scrollable_vertical;
    main_thread_scrolling_reasons_ = main_thread_scrolling_reasons;
    compositor_element_id_ = compositor_element_id;
    DCHECK(ElementIdNamespaceIsForScrolling());
    return true;
  }

  // Rect of the container area that the contents scrolls in, in the space of
  // the parent of the associated transform node (ScrollTranslation).
  // It doesn't include non-overlay scrollbars. Overlay scrollbars do not affect
  // the rect.
  const IntRect& ContainerRect() const { return container_rect_; }

  // Rect of the contents that is scrolled within the container rect, in the
  // space of the associated transform node (ScrollTranslation).
  const IntRect& ContentsRect() const { return contents_rect_; }

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

#if DCHECK_IS_ON()
  // The clone function is used by FindPropertiesNeedingUpdate.h for recording
  // a scroll node before it has been updated, to later detect changes.
  scoped_refptr<ScrollPaintPropertyNode> Clone() const {
    scoped_refptr<ScrollPaintPropertyNode> cloned =
        base::AdoptRef(new ScrollPaintPropertyNode(
            Parent(), container_rect_, contents_rect_,
            user_scrollable_horizontal_, user_scrollable_vertical_,
            main_thread_scrolling_reasons_, compositor_element_id_));
    return cloned;
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if a scroll node has changed.
  bool operator==(const ScrollPaintPropertyNode& o) const {
    return Parent() == o.Parent() && container_rect_ == o.container_rect_ &&
           contents_rect_ == o.contents_rect_ &&
           user_scrollable_horizontal_ == o.user_scrollable_horizontal_ &&
           user_scrollable_vertical_ == o.user_scrollable_vertical_ &&
           main_thread_scrolling_reasons_ == o.main_thread_scrolling_reasons_ &&
           compositor_element_id_ == o.compositor_element_id_;
  }
#endif

  std::unique_ptr<JSONObject> ToJSON() const;

 private:
  ScrollPaintPropertyNode(
      scoped_refptr<const ScrollPaintPropertyNode> parent,
      const IntRect& container_rect,
      const IntRect& contents_rect,
      bool user_scrollable_horizontal,
      bool user_scrollable_vertical,
      MainThreadScrollingReasons main_thread_scrolling_reasons,
      CompositorElementId compositor_element_id)
      : PaintPropertyNode(std::move(parent)),
        container_rect_(container_rect),
        contents_rect_(contents_rect),
        user_scrollable_horizontal_(user_scrollable_horizontal),
        user_scrollable_vertical_(user_scrollable_vertical),
        main_thread_scrolling_reasons_(main_thread_scrolling_reasons),
        compositor_element_id_(compositor_element_id) {
    DCHECK(ElementIdNamespaceIsForScrolling());
  }

  bool ElementIdNamespaceIsForScrolling() const {
    return !compositor_element_id_ ||
           NamespaceFromCompositorElementId(compositor_element_id_) ==
               CompositorElementIdNamespace::kScroll;
  }

  IntRect container_rect_;
  IntRect contents_rect_;
  bool user_scrollable_horizontal_ : 1;
  bool user_scrollable_vertical_ : 1;
  MainThreadScrollingReasons main_thread_scrolling_reasons_;
  // The scrolling element id is stored directly on the scroll node and not on
  // the associated TransformPaintPropertyNode used for scroll offset.
  CompositorElementId compositor_element_id_;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const ScrollPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // ScrollPaintPropertyNode_h
