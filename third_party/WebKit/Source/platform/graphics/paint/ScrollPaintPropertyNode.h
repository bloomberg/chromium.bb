// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollPaintPropertyNode_h
#define ScrollPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatSize.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

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
    : public RefCounted<ScrollPaintPropertyNode> {
 public:
  // This node is really a sentinel, and does not represent a real scroll.
  static ScrollPaintPropertyNode* root();

  static PassRefPtr<ScrollPaintPropertyNode> create(
      PassRefPtr<const ScrollPaintPropertyNode> parent,
      const IntSize& clip,
      const IntSize& bounds,
      bool userScrollableHorizontal,
      bool userScrollableVertical,
      MainThreadScrollingReasons mainThreadScrollingReasons,
      WebLayerScrollClient* scrollClient) {
    return adoptRef(new ScrollPaintPropertyNode(
        std::move(parent), clip, bounds, userScrollableHorizontal,
        userScrollableVertical, mainThreadScrollingReasons, scrollClient));
  }

  void update(PassRefPtr<const ScrollPaintPropertyNode> parent,
              const IntSize& clip,
              const IntSize& bounds,
              bool userScrollableHorizontal,
              bool userScrollableVertical,
              MainThreadScrollingReasons mainThreadScrollingReasons,
              WebLayerScrollClient* scrollClient) {
    DCHECK(!isRoot());
    DCHECK(parent != this);
    m_parent = parent;
    m_clip = clip;
    m_bounds = bounds;
    m_userScrollableHorizontal = userScrollableHorizontal;
    m_userScrollableVertical = userScrollableVertical;
    m_mainThreadScrollingReasons = mainThreadScrollingReasons;
    m_scrollClient = scrollClient;
  }

  const ScrollPaintPropertyNode* parent() const { return m_parent.get(); }
  bool isRoot() const { return !m_parent; }

  // The clipped area that contains the scrolled content.
  const IntSize& clip() const { return m_clip; }

  // The bounds of the content that is scrolled within |clip|.
  const IntSize& bounds() const { return m_bounds; }

  bool userScrollableHorizontal() const { return m_userScrollableHorizontal; }
  bool userScrollableVertical() const { return m_userScrollableVertical; }

  // Return reason bitfield with values from cc::MainThreadScrollingReason.
  MainThreadScrollingReasons mainThreadScrollingReasons() const {
    return m_mainThreadScrollingReasons;
  }

  // Main thread scrolling reason for the threaded scrolling disabled setting.
  bool threadedScrollingDisabled() const {
    return m_mainThreadScrollingReasons &
           MainThreadScrollingReason::kThreadedScrollingDisabled;
  }

  // Main thread scrolling reason for background attachment fixed descendants.
  bool hasBackgroundAttachmentFixedDescendants() const {
    return m_mainThreadScrollingReasons &
           MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  }

  WebLayerScrollClient* scrollClient() const { return m_scrollClient; }

#if DCHECK_IS_ON()
  // The clone function is used by FindPropertiesNeedingUpdate.h for recording
  // a scroll node before it has been updated, to later detect changes.
  PassRefPtr<ScrollPaintPropertyNode> clone() const {
    RefPtr<ScrollPaintPropertyNode> cloned =
        adoptRef(new ScrollPaintPropertyNode(
            m_parent, m_clip, m_bounds, m_userScrollableHorizontal,
            m_userScrollableVertical, m_mainThreadScrollingReasons,
            m_scrollClient));
    return cloned;
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if a scroll node has changed.
  bool operator==(const ScrollPaintPropertyNode& o) const {
    return m_parent == o.m_parent && m_clip == o.m_clip &&
           m_bounds == o.m_bounds &&
           m_userScrollableHorizontal == o.m_userScrollableHorizontal &&
           m_userScrollableVertical == o.m_userScrollableVertical &&
           m_mainThreadScrollingReasons == o.m_mainThreadScrollingReasons &&
           m_scrollClient == o.m_scrollClient;
  }

  String toTreeString() const;
#endif

  String toString() const;

 private:
  ScrollPaintPropertyNode(PassRefPtr<const ScrollPaintPropertyNode> parent,
                          IntSize clip,
                          IntSize bounds,
                          bool userScrollableHorizontal,
                          bool userScrollableVertical,
                          MainThreadScrollingReasons mainThreadScrollingReasons,
                          WebLayerScrollClient* scrollClient)
      : m_parent(parent),
        m_clip(clip),
        m_bounds(bounds),
        m_userScrollableHorizontal(userScrollableHorizontal),
        m_userScrollableVertical(userScrollableVertical),
        m_mainThreadScrollingReasons(mainThreadScrollingReasons),
        m_scrollClient(scrollClient) {}

  RefPtr<const ScrollPaintPropertyNode> m_parent;
  IntSize m_clip;
  IntSize m_bounds;
  bool m_userScrollableHorizontal : 1;
  bool m_userScrollableVertical : 1;
  MainThreadScrollingReasons m_mainThreadScrollingReasons;
  WebLayerScrollClient* m_scrollClient;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const ScrollPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // ScrollPaintPropertyNode_h
