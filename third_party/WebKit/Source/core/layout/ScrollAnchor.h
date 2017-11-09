// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollAnchor_h
#define ScrollAnchor_h

#include "core/CoreExport.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class Document;
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
  void SetScroller(ScrollableArea*);

  // Returns true if the underlying scroller is set.
  bool HasScroller() const { return scroller_; }

  // The LayoutObject we are currently anchored to. Lazily computed during
  // notifyBeforeLayout() and cached until the next call to clear().
  LayoutObject* AnchorObject() const { return anchor_object_; }

  // Indicates that this ScrollAnchor, and all ancestor ScrollAnchors, should
  // compute new anchor nodes on their next notifyBeforeLayout().
  void Clear();

  // Indicates that this ScrollAnchor should compute a new anchor node on the
  // next call to notifyBeforeLayout().
  void ClearSelf();

  // Records the anchor's location in relation to the scroller. Should be
  // called when the scroller is about to be laid out.
  void NotifyBeforeLayout();

  // Scrolls to compensate for any change in the anchor's relative location.
  // Should be called at the end of the animation frame.
  void Adjust();

  enum class Corner {
    kTopLeft = 0,
    kTopRight,
  };
  // Which corner of the anchor object we are currently anchored to.
  // Only meaningful if anchorObject() is non-null.
  Corner GetCorner() const { return corner_; }

  struct SerializedAnchor {
    SerializedAnchor() : simhash(0) {}
    SerializedAnchor(const String& s, const LayoutPoint& p)
        : selector(s), relative_offset(p), simhash(0) {}
    SerializedAnchor(const String& s, const LayoutPoint& p, uint64_t hash)
        : selector(s), relative_offset(p), simhash(hash) {}

    bool IsValid() { return !selector.IsEmpty(); }

    // Used to locate an element previously used as a scroll anchor.
    const String selector;
    // Used to restore the previous offset of the element within its scroller.
    const LayoutPoint relative_offset;
    // Used to compare the similarity of a prospective anchor's contents to the
    // contents at the time the previous anchor was saved.
    const uint64_t simhash;
  };

  // Attempt to restore |serialized_anchor| by scrolling to the element
  // identified by its selector, adjusting by its relative_offset.
  bool RestoreAnchor(Document*, const SerializedAnchor&);

  // Create a serialized representation of the current anchor_object_.
  const SerializedAnchor SerializeAnchor();

  // Checks if we hold any references to the specified object.
  bool RefersTo(const LayoutObject*) const;

  // Notifies us that an object will be removed from the layout tree.
  void NotifyRemoved(LayoutObject*);

  void Trace(blink::Visitor* visitor) { visitor->Trace(scroller_); }

 private:
  void FindAnchor();
  // Returns true if searching should stop. Stores result in m_anchorObject.
  bool FindAnchorRecursive(LayoutObject*);
  bool ComputeScrollAnchorDisablingStyleChanged();

  enum WalkStatus { kSkip = 0, kConstrain, kContinue, kReturn };
  struct ExamineResult {
    ExamineResult(WalkStatus s)
        : status(s), viable(false), corner(Corner::kTopLeft) {}

    ExamineResult(WalkStatus s, Corner c)
        : status(s), viable(true), corner(c) {}

    WalkStatus status;
    bool viable;
    Corner corner;
  };
  ExamineResult Examine(const LayoutObject*) const;

  IntSize ComputeAdjustment() const;

  // The scroller to be adjusted by this ScrollAnchor. This is also the scroller
  // that owns us, unless it is the RootFrameViewport in which case we are owned
  // by the layout viewport.
  Member<ScrollableArea> scroller_;

  // The LayoutObject we should anchor to.
  LayoutObject* anchor_object_;

  // Which corner of m_anchorObject's bounding box to anchor to.
  Corner corner_;

  // Location of m_layoutObject relative to scroller at time of
  // notifyBeforeLayout().
  LayoutPoint saved_relative_offset_;

  // We suppress scroll anchoring after a style change on the anchor node or
  // one of its ancestors, if that change might have caused the node to move.
  // This bit tracks whether we have had a scroll-anchor-disabling style
  // change since the last layout.  It is recomputed in notifyBeforeLayout(),
  // and used to suppress adjustment in adjust().  See http://bit.ly/sanaclap.
  bool scroll_anchor_disabling_style_changed_;

  // True iff an adjustment check has been queued with the FrameView but not yet
  // performed.
  bool queued_;
};

}  // namespace blink

#endif  // ScrollAnchor_h
