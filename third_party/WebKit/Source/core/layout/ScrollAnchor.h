// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollAnchor_h
#define ScrollAnchor_h

#include "core/CoreExport.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/heap/Handle.h"

namespace blink {

class LayoutObject;
class ScrollableArea;

// Scrolls to compensate for layout movements (bit.ly/scroll-anchoring).
class CORE_EXPORT ScrollAnchor final {
  DISALLOW_NEW();

 public:
  ScrollAnchor();
  explicit ScrollAnchor(ScrollableArea*);
  ~ScrollAnchor();

  // The scroller that is scrolled to componsate for layout movements. Note
  // that the scroller can only be initialized once.
  void setScroller(ScrollableArea*);

  // Returns true if the underlying scroller is set.
  bool hasScroller() const { return m_scroller; }

  // The LayoutObject we are currently anchored to. Lazily computed during
  // save() and cached until the next call to clear().
  LayoutObject* anchorObject() const { return m_anchorObject; }

  // Indicates that the next save() should compute a new anchor for the
  // containing scroller and all ancestor scrollers.
  void clear();

  // Indicates that the next save() should compute a new anchor for the
  // containing scroller.
  void clearSelf();

  // Records the anchor's location in relation to the scroller. Should be
  // called when the scroller is about to be laid out.
  void save();

  // Scrolls to compensate for any change in the anchor's relative location
  // since the most recent call to save(). Should be called immediately after
  // the scroller has been laid out.
  void restore();

  enum class Corner {
    TopLeft = 0,
    TopRight,
  };
  // Which corner of the anchor object we are currently anchored to.
  // Only meaningful if anchorObject() is non-null.
  Corner corner() const { return m_corner; }

  // Checks if we hold any references to the specified object.
  bool refersTo(const LayoutObject*) const;

  // Notifies us that an object will be removed from the layout tree.
  void notifyRemoved(LayoutObject*);

  DEFINE_INLINE_TRACE() { visitor->trace(m_scroller); }

 private:
  // Releases the anchor and conditionally clears the IsScrollAnchorObject bit
  // on the LayoutObject.
  void clearSelf(bool unconditionally);

  void findAnchor();
  bool computeScrollAnchorDisablingStyleChanged();

  enum WalkStatus { Skip = 0, Constrain, Continue, Return };
  struct ExamineResult {
    ExamineResult(WalkStatus s)
        : status(s), viable(false), corner(Corner::TopLeft) {}

    ExamineResult(WalkStatus s, Corner c)
        : status(s), viable(true), corner(c) {}

    WalkStatus status;
    bool viable;
    Corner corner;
  };
  ExamineResult examine(const LayoutObject*) const;

  IntSize computeAdjustment() const;

  // The scroller that owns and is adjusted by this ScrollAnchor.
  Member<ScrollableArea> m_scroller;

  // The LayoutObject we should anchor to.
  LayoutObject* m_anchorObject;

  // Which corner of m_anchorObject's bounding box to anchor to.
  Corner m_corner;

  // Location of m_layoutObject relative to scroller at time of save().
  LayoutPoint m_savedRelativeOffset;

  // We suppress scroll anchoring after a style change on the anchor node or
  // one of its ancestors, if that change might have caused the node to move.
  // This bit tracks whether we have had a scroll-anchor-disabling style
  // change since the last layout.  It is recomputed in save(), and used to
  // suppress the adjustment in restore().  More at http://bit.ly/sanaclap.
  bool m_scrollAnchorDisablingStyleChanged;

  // True iff save has been called, and restore has not been called since
  // the call to save.  In this state, additional calls to save are ignored,
  // to make things easier for multi-pass layout modes such as flexbox.
  // TODO(skobes): explore anchoring at frame boundaries instead of layouts,
  // which would allow this field to be removed.
  bool m_saved;
};

}  // namespace blink

#endif  // ScrollAnchor_h
