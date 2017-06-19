// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/InlineBoxTraversal.h"

#include "core/layout/line/InlineBox.h"

namespace blink {

namespace {

// "Left" Traversal strategy
struct TraverseLeft {
  STATIC_ONLY(TraverseLeft);

  static InlineBox* Forward(const InlineBox& inline_box) {
    return inline_box.PrevLeafChild();
  }
};

// "Left" Traversal strategy ignoring line break
struct TraverseLeftIgnoringLineBreak {
  STATIC_ONLY(TraverseLeftIgnoringLineBreak);

  static InlineBox* Forward(const InlineBox& inline_box) {
    return inline_box.PrevLeafChildIgnoringLineBreak();
  }
};

// "Right" Traversal strategy
struct TraverseRight {
  STATIC_ONLY(TraverseRight);

  static InlineBox* Forward(const InlineBox& inline_box) {
    return inline_box.NextLeafChild();
  }
};

// "Right" Traversal strategy ignoring line break
struct TraverseRightIgnoringLineBreak {
  STATIC_ONLY(TraverseRightIgnoringLineBreak);

  static InlineBox* Forward(const InlineBox& inline_box) {
    return inline_box.NextLeafChildIgnoringLineBreak();
  }
};

template <typename TraversalStrategy>
InlineBox* FindBidiRun(const InlineBox& start, unsigned bidi_level) {
  for (InlineBox* runner = TraversalStrategy::Forward(start); runner;
       runner = TraversalStrategy::Forward(*runner)) {
    if (runner->BidiLevel() <= bidi_level)
      return runner;
  }
  return nullptr;
}

template <typename TraversalStrategy>
InlineBox* FindBoudnaryOfBidiRun(const InlineBox& start, unsigned bidi_level) {
  InlineBox* result = const_cast<InlineBox*>(&start);
  for (InlineBox* runner = TraversalStrategy::Forward(start); runner;
       runner = TraversalStrategy::Forward(*runner)) {
    if (runner->BidiLevel() <= bidi_level)
      return result;
    result = runner;
  }
  return result;
}

template <typename TraversalStrategy>
InlineBox* FindBoudnaryOfEntireBidiRun(const InlineBox& start,
                                       unsigned bidi_level) {
  InlineBox* result = const_cast<InlineBox*>(&start);
  for (InlineBox* runner = TraversalStrategy::Forward(start); runner;
       runner = TraversalStrategy::Forward(*runner)) {
    if (runner->BidiLevel() < bidi_level)
      return result;
    result = runner;
  }
  return result;
}

}  // namespace

InlineBox* InlineBoxTraversal::FindLeftBidiRun(const InlineBox& box,
                                               unsigned bidi_level) {
  return FindBidiRun<TraverseLeft>(box, bidi_level);
}

InlineBox* InlineBoxTraversal::FindRightBidiRun(const InlineBox& box,
                                                unsigned bidi_level) {
  return FindBidiRun<TraverseRight>(box, bidi_level);
}

InlineBox* InlineBoxTraversal::FindLeftBoundaryOfBidiRunIgnoringLineBreak(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoudnaryOfBidiRun<TraverseLeftIgnoringLineBreak>(inline_box,
                                                              bidi_level);
}

InlineBox* InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRun(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoudnaryOfEntireBidiRun<TraverseLeft>(inline_box, bidi_level);
}

InlineBox* InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRunIgnoringLineBreak(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoudnaryOfEntireBidiRun<TraverseLeftIgnoringLineBreak>(inline_box,
                                                                    bidi_level);
}

InlineBox* InlineBoxTraversal::FindRightBoundaryOfBidiRunIgnoringLineBreak(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoudnaryOfBidiRun<TraverseRightIgnoringLineBreak>(inline_box,
                                                               bidi_level);
}

InlineBox* InlineBoxTraversal::FindRightBoundaryOfEntireBidiRun(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoudnaryOfEntireBidiRun<TraverseRight>(inline_box, bidi_level);
}

InlineBox*
InlineBoxTraversal::FindRightBoundaryOfEntireBidiRunIgnoringLineBreak(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoudnaryOfEntireBidiRun<TraverseRightIgnoringLineBreak>(
      inline_box, bidi_level);
}

}  // namespace blink
