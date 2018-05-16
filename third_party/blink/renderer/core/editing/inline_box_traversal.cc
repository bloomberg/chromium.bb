// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/inline_box_traversal.h"

#include "third_party/blink/renderer/core/editing/inline_box_position.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/line/inline_box.h"
#include "third_party/blink/renderer/core/layout/line/root_inline_box.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"

// TODO(xiaochengh): Rename this file to |bidi_adjustment.cc|

namespace blink {

namespace {

// TODO(xiaochengh): Bidi adjustment should check base direction of containing
// line instead of containing block, to handle unicode-bidi correctly.
TextDirection ParagraphDirectionOf(const InlineBox& box) {
  return box.Root().Block().Style()->Direction();
}

// |SideAffinity| represents the left or right side of a leaf inline box.
// For example, with text box "abc", "|abc" is the left side, and
// "abc|" is the right side.
enum SideAffinity { kLeft, kRight };

// Returns whether |box_position| is at the left or right side of its InlineBox.
SideAffinity GetSideAffinity(const InlineBoxPosition& box_position) {
  DCHECK(box_position.inline_box);
  const InlineBox* box = box_position.inline_box;
  const int offset = box_position.offset_in_box;
  DCHECK(offset == box->CaretLeftmostOffset() ||
         offset == box->CaretRightmostOffset());
  return offset == box->CaretLeftmostOffset() ? SideAffinity::kLeft
                                              : SideAffinity::kRight;
}

// An abstraction of a caret position that is at the left or right side of a
// leaf inline box.
class InlineBoxAndSideAffinity {
  STACK_ALLOCATED();

 public:
  InlineBoxAndSideAffinity(const InlineBox& box, SideAffinity side)
      : box_(box), side_(side) {}

  explicit InlineBoxAndSideAffinity(const InlineBoxPosition& box_position)
      : box_(*box_position.inline_box), side_(GetSideAffinity(box_position)) {}

  InlineBoxPosition ToInlineBoxPosition() const {
    return InlineBoxPosition(&box_, side_ == SideAffinity::kLeft
                                        ? box_.CaretLeftmostOffset()
                                        : box_.CaretRightmostOffset());
  }

  const InlineBox& GetBox() const { return box_; }
  bool AtLeftSide() const { return side_ == SideAffinity::kLeft; }
  bool AtRightSide() const { return side_ == SideAffinity::kRight; }

 private:
  const InlineBox& box_;
  SideAffinity side_;
};

struct TraverseRight;

// "Left" traversal strategy
struct TraverseLeft {
  STATIC_ONLY(TraverseLeft);

  using Backwards = TraverseRight;

  static const InlineBox* Forward(const InlineBox& box) {
    return box.PrevLeafChild();
  }

  static const InlineBox* ForwardIgnoringLineBreak(const InlineBox& box) {
    return box.PrevLeafChildIgnoringLineBreak();
  }

  static const InlineBox* Backward(const InlineBox& box);
  static const InlineBox* BackwardIgnoringLineBreak(const InlineBox& box);

  static SideAffinity ForwardSideAffinity() { return SideAffinity::kLeft; }
};

// "Left" traversal strategy
struct TraverseRight {
  STATIC_ONLY(TraverseRight);

  using Backwards = TraverseLeft;

  static const InlineBox* Forward(const InlineBox& box) {
    return box.NextLeafChild();
  }

  static const InlineBox* ForwardIgnoringLineBreak(const InlineBox& box) {
    return box.NextLeafChildIgnoringLineBreak();
  }

  static const InlineBox* Backward(const InlineBox& box) {
    return Backwards::Forward(box);
  }

  static const InlineBox* BackwardIgnoringLineBreak(const InlineBox& box) {
    return Backwards::ForwardIgnoringLineBreak(box);
  }

  static SideAffinity ForwardSideAffinity() { return SideAffinity::kRight; }
};

// static
const InlineBox* TraverseLeft::Backward(const InlineBox& box) {
  return Backwards::Forward(box);
}

// static
const InlineBox* TraverseLeft::BackwardIgnoringLineBreak(const InlineBox& box) {
  return Backwards::ForwardIgnoringLineBreak(box);
}

template <typename TraversalStrategy>
using Backwards = typename TraversalStrategy::Backwards;

template <typename TraversalStrategy>
InlineBoxAndSideAffinity InlineBoxAndForwardSideAffinity(const InlineBox& box) {
  return InlineBoxAndSideAffinity(box,
                                  TraversalStrategy::ForwardSideAffinity());
}

template <typename TraversalStrategy>
InlineBoxAndSideAffinity InlineBoxAndBackwardSideAffinity(
    const InlineBox& box) {
  return InlineBoxAndForwardSideAffinity<Backwards<TraversalStrategy>>(box);
}

// Template algorithms for traversing in bidi runs

// Traverses from |start|, and returns the first box with bidi level less than
// or equal to |bidi_level| (excluding |start| itself). Returns a null box when
// such a box doesn't exist.
template <typename TraversalStrategy>
const InlineBox* FindBidiRun(const InlineBox& start, unsigned bidi_level) {
  for (const InlineBox* runner = TraversalStrategy::Forward(start); runner;
       runner = TraversalStrategy::Forward(*runner)) {
    if (runner->BidiLevel() <= bidi_level)
      return runner;
  }
  return nullptr;
}

// Traverses from |start|, and returns the last non-linebreak box with bidi
// level greater than |bidi_level| (including |start| itself).
template <typename TraversalStrategy>
const InlineBox& FindBoundaryOfBidiRunIgnoringLineBreak(const InlineBox& start,
                                                        unsigned bidi_level) {
  const InlineBox* last_runner = &start;
  for (const InlineBox* runner =
           TraversalStrategy::ForwardIgnoringLineBreak(start);
       runner; runner = TraversalStrategy::ForwardIgnoringLineBreak(*runner)) {
    if (runner->BidiLevel() <= bidi_level)
      return *last_runner;
    last_runner = runner;
  }
  return *last_runner;
}

// Traverses from |start|, and returns the last box with bidi level greater than
// or equal to |bidi_level| (including |start| itself). Line break boxes may or
// may not be ignored, depending of the passed |forward| function.
const InlineBox& FindBoundaryOfEntireBidiRunInternal(
    const InlineBox& start,
    unsigned bidi_level,
    std::function<const InlineBox*(const InlineBox&)> forward) {
  const InlineBox* last_runner = &start;
  for (const InlineBox* runner = forward(start); runner;
       runner = forward(*runner)) {
    if (runner->BidiLevel() < bidi_level)
      return *last_runner;
    last_runner = runner;
  }
  return *last_runner;
}

// Variant of |FindBoundaryOfEntireBidiRun| preserving line break boxes.
template <typename TraversalStrategy>
const InlineBox& FindBoundaryOfEntireBidiRun(const InlineBox& start,
                                             unsigned bidi_level) {
  return FindBoundaryOfEntireBidiRunInternal(start, bidi_level,
                                             TraversalStrategy::Forward);
}

// Variant of |FindBoundaryOfEntireBidiRun| ignoring line break boxes.
template <typename TraversalStrategy>
const InlineBox& FindBoundaryOfEntireBidiRunIgnoringLineBreak(
    const InlineBox& start,
    unsigned bidi_level) {
  return FindBoundaryOfEntireBidiRunInternal(
      start, bidi_level, TraversalStrategy::ForwardIgnoringLineBreak);
}

// Adjustment algorithm at the end of caret position resolution.
template <typename TraversalStrategy>
class CaretPositionResolutionAdjuster {
  STATIC_ONLY(CaretPositionResolutionAdjuster);

 public:
  static InlineBoxAndSideAffinity UnadjustedCaretPosition(
      const InlineBox& box) {
    return InlineBoxAndBackwardSideAffinity<TraversalStrategy>(box);
  }

  // Returns true if |box| starts different direction of embedded text run.
  // See [1] for details.
  // [1] UNICODE BIDIRECTIONAL ALGORITHM, http://unicode.org/reports/tr9/
  static bool IsStartOfDifferentDirection(const InlineBox&);

  static InlineBoxAndSideAffinity AdjustForPrimaryDirectionAlgorithm(
      const InlineBox& box) {
    if (IsStartOfDifferentDirection(box))
      return UnadjustedCaretPosition(box);

    const unsigned level = TraversalStrategy::Backward(box)->BidiLevel();
    const InlineBox* forward_box = FindBidiRun<TraversalStrategy>(box, level);

    // For example, abc FED 123 ^ CBA when adjusting right side of 123
    if (forward_box && forward_box->BidiLevel() == level)
      return UnadjustedCaretPosition(box);

    // For example, abc 123 ^ CBA when adjusting right side of 123
    const InlineBox& result_box =
        FindBoundaryOfEntireBidiRun<Backwards<TraversalStrategy>>(box, level);
    return InlineBoxAndBackwardSideAffinity<TraversalStrategy>(result_box);
  }

  static InlineBoxAndSideAffinity AdjustFor(const InlineBox& box,
                                            UnicodeBidi unicode_bidi) {
    // TODO(xiaochengh): We should check line direction instead, and get rid of
    // ad hoc handling of 'unicode-bidi'.
    const TextDirection primary_direction = ParagraphDirectionOf(box);
    if (box.Direction() == primary_direction)
      return AdjustForPrimaryDirectionAlgorithm(box);

    if (unicode_bidi == UnicodeBidi::kPlaintext)
      return UnadjustedCaretPosition(box);

    const unsigned char level = box.BidiLevel();
    const InlineBox* backward_box =
        TraversalStrategy::BackwardIgnoringLineBreak(box);
    if (!backward_box || backward_box->BidiLevel() < level) {
      // Backward side of a secondary run. Set to the forward side of the entire
      // run.
      const InlineBox& result_box =
          FindBoundaryOfEntireBidiRunIgnoringLineBreak<TraversalStrategy>(
              box, level);
      return InlineBoxAndForwardSideAffinity<TraversalStrategy>(result_box);
    }

    if (backward_box->BidiLevel() <= level)
      return UnadjustedCaretPosition(box);

    // Forward side of a "tertiary" run. Set to the backward side of that run.
    const InlineBox& result_box =
        FindBoundaryOfBidiRunIgnoringLineBreak<Backwards<TraversalStrategy>>(
            box, level);
    return InlineBoxAndBackwardSideAffinity<TraversalStrategy>(result_box);
  }
};

// TODO(editing-dev): Try to unify the algorithms for both directions.
template <>
bool CaretPositionResolutionAdjuster<TraverseLeft>::IsStartOfDifferentDirection(
    const InlineBox& box) {
  const InlineBox* backward_box = TraverseRight::Forward(box);
  if (!backward_box)
    return true;
  return backward_box->BidiLevel() >= box.BidiLevel();
}

template <>
bool CaretPositionResolutionAdjuster<
    TraverseRight>::IsStartOfDifferentDirection(const InlineBox& box) {
  const InlineBox* backward_box = TraverseLeft::Forward(box);
  if (!backward_box)
    return true;
  if (backward_box->Direction() == box.Direction())
    return true;
  return backward_box->BidiLevel() > box.BidiLevel();
}

}  // namespace

const InlineBox* InlineBoxTraversal::FindLeftBidiRun(const InlineBox& box,
                                                     unsigned bidi_level) {
  return FindBidiRun<TraverseLeft>(box, bidi_level);
}

const InlineBox* InlineBoxTraversal::FindRightBidiRun(const InlineBox& box,
                                                      unsigned bidi_level) {
  return FindBidiRun<TraverseRight>(box, bidi_level);
}

const InlineBox& InlineBoxTraversal::FindLeftBoundaryOfBidiRunIgnoringLineBreak(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoundaryOfBidiRunIgnoringLineBreak<TraverseLeft>(inline_box,
                                                              bidi_level);
}

const InlineBox& InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRun(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoundaryOfEntireBidiRun<TraverseLeft>(inline_box, bidi_level);
}

const InlineBox&
InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRunIgnoringLineBreak(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoundaryOfEntireBidiRunIgnoringLineBreak<TraverseLeft>(inline_box,
                                                                    bidi_level);
}

const InlineBox&
InlineBoxTraversal::FindRightBoundaryOfBidiRunIgnoringLineBreak(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoundaryOfBidiRunIgnoringLineBreak<TraverseRight>(inline_box,
                                                               bidi_level);
}

const InlineBox& InlineBoxTraversal::FindRightBoundaryOfEntireBidiRun(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoundaryOfEntireBidiRun<TraverseRight>(inline_box, bidi_level);
}

const InlineBox&
InlineBoxTraversal::FindRightBoundaryOfEntireBidiRunIgnoringLineBreak(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoundaryOfEntireBidiRunIgnoringLineBreak<TraverseRight>(
      inline_box, bidi_level);
}

InlineBoxPosition BidiAdjustment::AdjustForCaretPositionResolution(
    const InlineBoxPosition& caret_position,
    UnicodeBidi unicode_bidi) {
  const InlineBoxAndSideAffinity unadjusted(caret_position);
  const InlineBoxAndSideAffinity adjusted =
      unadjusted.AtLeftSide()
          ? CaretPositionResolutionAdjuster<TraverseRight>::AdjustFor(
                unadjusted.GetBox(), unicode_bidi)
          : CaretPositionResolutionAdjuster<TraverseLeft>::AdjustFor(
                unadjusted.GetBox(), unicode_bidi);
  return adjusted.ToInlineBoxPosition();
}

}  // namespace blink
