// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollPaintPropertyNode_h
#define ScrollPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

#include <iosfwd>

namespace blink {

using MainThreadScrollingReasons = uint32_t;

// A scroll node contains auxiliary scrolling information for threaded scrolling
// which includes how far an area can be scrolled, which transform node contains
// the scroll offset, etc.
//
// Main thread scrolling reasons force scroll updates to go to the main thread
// and can have dependencies on other nodes. For example, all parents of a
// scroll node with background attachment fixed set should also have it set.
class PLATFORM_EXPORT ScrollPaintPropertyNode
    : public RefCounted<ScrollPaintPropertyNode> {
 public:
  static ScrollPaintPropertyNode* root();

  static PassRefPtr<ScrollPaintPropertyNode> create(
      PassRefPtr<const ScrollPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> scrollOffsetTranslation,
      const IntSize& clip,
      const IntSize& bounds,
      bool userScrollableHorizontal,
      bool userScrollableVertical,
      MainThreadScrollingReasons mainThreadScrollingReasons) {
    return adoptRef(new ScrollPaintPropertyNode(
        std::move(parent), std::move(scrollOffsetTranslation), clip, bounds,
        userScrollableHorizontal, userScrollableVertical,
        mainThreadScrollingReasons));
  }

  void update(
      PassRefPtr<const ScrollPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> scrollOffsetTranslation,
      const IntSize& clip,
      const IntSize& bounds,
      bool userScrollableHorizontal,
      bool userScrollableVertical,
      MainThreadScrollingReasons mainThreadScrollingReasons) {
    DCHECK(!isRoot());
    DCHECK(parent != this);
    m_parent = parent;
    DCHECK(scrollOffsetTranslation->matrix().isIdentityOr2DTranslation());
    m_scrollOffsetTranslation = scrollOffsetTranslation;
    m_clip = clip;
    m_bounds = bounds;
    m_userScrollableHorizontal = userScrollableHorizontal;
    m_userScrollableVertical = userScrollableVertical;
    m_mainThreadScrollingReasons = mainThreadScrollingReasons;
  }

  const ScrollPaintPropertyNode* parent() const { return m_parent.get(); }
  bool isRoot() const { return !m_parent; }

  // Transform that the scroll is relative to.
  const TransformPaintPropertyNode* scrollOffsetTranslation() const {
    return m_scrollOffsetTranslation.get();
  }

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

#if DCHECK_IS_ON()
  // The clone function is used by FindPropertiesNeedingUpdate.h for recording
  // a scroll node before it has been updated, to later detect changes.
  PassRefPtr<ScrollPaintPropertyNode> clone() const {
    RefPtr<ScrollPaintPropertyNode> cloned =
        adoptRef(new ScrollPaintPropertyNode(
            m_parent, m_scrollOffsetTranslation, m_clip, m_bounds,
            m_userScrollableHorizontal, m_userScrollableVertical,
            m_mainThreadScrollingReasons));
    return cloned;
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if a scroll node has changed.
  bool operator==(const ScrollPaintPropertyNode& o) const {
    return m_parent == o.m_parent &&
           m_scrollOffsetTranslation == o.m_scrollOffsetTranslation &&
           m_clip == o.m_clip && m_bounds == o.m_bounds &&
           m_userScrollableHorizontal == o.m_userScrollableHorizontal &&
           m_userScrollableVertical == o.m_userScrollableVertical &&
           m_mainThreadScrollingReasons == o.m_mainThreadScrollingReasons;
  }
#endif

  String toString() const;

 private:
  ScrollPaintPropertyNode(
      PassRefPtr<const ScrollPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> scrollOffsetTranslation,
      IntSize clip,
      IntSize bounds,
      bool userScrollableHorizontal,
      bool userScrollableVertical,
      MainThreadScrollingReasons mainThreadScrollingReasons)
      : m_parent(parent),
        m_scrollOffsetTranslation(scrollOffsetTranslation),
        m_clip(clip),
        m_bounds(bounds),
        m_userScrollableHorizontal(userScrollableHorizontal),
        m_userScrollableVertical(userScrollableVertical),
        m_mainThreadScrollingReasons(mainThreadScrollingReasons) {
    DCHECK(m_scrollOffsetTranslation->matrix().isIdentityOr2DTranslation());
  }

  RefPtr<const ScrollPaintPropertyNode> m_parent;
  RefPtr<const TransformPaintPropertyNode> m_scrollOffsetTranslation;
  IntSize m_clip;
  IntSize m_bounds;
  bool m_userScrollableHorizontal : 1;
  bool m_userScrollableVertical : 1;
  MainThreadScrollingReasons m_mainThreadScrollingReasons;
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const ScrollPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // ScrollPaintPropertyNode_h
