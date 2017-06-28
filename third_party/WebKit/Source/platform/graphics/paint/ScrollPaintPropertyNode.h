// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollPaintPropertyNode_h
#define ScrollPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/paint/PaintPropertyNode.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "platform/wtf/text/WTFString.h"

#include <iosfwd>

namespace blink {

using MainThreadScrollingReasons = uint32_t;
class WebLayerScrollClient;

// A scroll node contains auxiliary scrolling information which includes how far
// an area can be scrolled, main thread scrolling reasons, etc. Scroll nodes
// are owned by TransformPaintPropertyNodes that are used for the scroll offset
// translation.
//
// Main thread scrolling reasons force scroll updates to go to the main thread
// and can have dependencies on other nodes. For example, all parents of a
// scroll node with background attachment fixed set should also have it set.
//
// The scroll tree differs from the other trees because it does not affect
// geometry directly. We may want to rename this class to reflect that it is
// more like rare scroll data for TransformPaintPropertyNode.
class PLATFORM_EXPORT ScrollPaintPropertyNode
    : public PaintPropertyNode<ScrollPaintPropertyNode> {
 public:
  // This node is really a sentinel, and does not represent a real scroll.
  static ScrollPaintPropertyNode* Root();

  static PassRefPtr<ScrollPaintPropertyNode> Create(
      PassRefPtr<const ScrollPaintPropertyNode> parent,
      const IntSize& clip,
      const IntSize& bounds,
      bool user_scrollable_horizontal,
      bool user_scrollable_vertical,
      MainThreadScrollingReasons main_thread_scrolling_reasons,
      WebLayerScrollClient* scroll_client) {
    return AdoptRef(new ScrollPaintPropertyNode(
        std::move(parent), clip, bounds, user_scrollable_horizontal,
        user_scrollable_vertical, main_thread_scrolling_reasons,
        scroll_client));
  }

  bool Update(PassRefPtr<const ScrollPaintPropertyNode> parent,
              const IntSize& clip,
              const IntSize& bounds,
              bool user_scrollable_horizontal,
              bool user_scrollable_vertical,
              MainThreadScrollingReasons main_thread_scrolling_reasons,
              WebLayerScrollClient* scroll_client) {
    bool parent_changed = PaintPropertyNode::Update(std::move(parent));

    if (clip == clip_ && bounds == bounds_ &&
        user_scrollable_horizontal == user_scrollable_horizontal_ &&
        user_scrollable_vertical == user_scrollable_vertical_ &&
        main_thread_scrolling_reasons == main_thread_scrolling_reasons_ &&
        scroll_client == scroll_client_)
      return parent_changed;

    SetChanged();
    clip_ = clip;
    bounds_ = bounds;
    user_scrollable_horizontal_ = user_scrollable_horizontal;
    user_scrollable_vertical_ = user_scrollable_vertical;
    main_thread_scrolling_reasons_ = main_thread_scrolling_reasons;
    scroll_client_ = scroll_client;
    return true;
  }

  // The clipped area that contains the scrolled content.
  const IntSize& Clip() const { return clip_; }

  // The bounds of the content that is scrolled within |clip|.
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

  WebLayerScrollClient* ScrollClient() const { return scroll_client_; }

#if DCHECK_IS_ON()
  // The clone function is used by FindPropertiesNeedingUpdate.h for recording
  // a scroll node before it has been updated, to later detect changes.
  PassRefPtr<ScrollPaintPropertyNode> Clone() const {
    RefPtr<ScrollPaintPropertyNode> cloned =
        AdoptRef(new ScrollPaintPropertyNode(
            Parent(), clip_, bounds_, user_scrollable_horizontal_,
            user_scrollable_vertical_, main_thread_scrolling_reasons_,
            scroll_client_));
    return cloned;
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if a scroll node has changed.
  bool operator==(const ScrollPaintPropertyNode& o) const {
    return Parent() == o.Parent() && clip_ == o.clip_ && bounds_ == o.bounds_ &&
           user_scrollable_horizontal_ == o.user_scrollable_horizontal_ &&
           user_scrollable_vertical_ == o.user_scrollable_vertical_ &&
           main_thread_scrolling_reasons_ == o.main_thread_scrolling_reasons_ &&
           scroll_client_ == o.scroll_client_;
  }

  String ToTreeString() const;
#endif

  String ToString() const;

 private:
  ScrollPaintPropertyNode(
      PassRefPtr<const ScrollPaintPropertyNode> parent,
      IntSize clip,
      IntSize bounds,
      bool user_scrollable_horizontal,
      bool user_scrollable_vertical,
      MainThreadScrollingReasons main_thread_scrolling_reasons,
      WebLayerScrollClient* scroll_client)
      : PaintPropertyNode(std::move(parent)),
        clip_(clip),
        bounds_(bounds),
        user_scrollable_horizontal_(user_scrollable_horizontal),
        user_scrollable_vertical_(user_scrollable_vertical),
        main_thread_scrolling_reasons_(main_thread_scrolling_reasons),
        scroll_client_(scroll_client) {}

  IntSize clip_;
  IntSize bounds_;
  bool user_scrollable_horizontal_ : 1;
  bool user_scrollable_vertical_ : 1;
  MainThreadScrollingReasons main_thread_scrolling_reasons_;
  WebLayerScrollClient* scroll_client_;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const ScrollPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // ScrollPaintPropertyNode_h
