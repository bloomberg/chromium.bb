// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/inline_box_traversal.h"

#include "third_party/blink/renderer/core/layout/line/inline_box.h"

namespace blink {

namespace {

// "Left" Traversal strategy
struct TraverseLeft {
  STATIC_ONLY(TraverseLeft);

  static const InlineBox* Forward(const InlineBox& inline_box) {
    return inline_box.PrevLeafChild();
  }
};

// "Left" Traversal strategy ignoring line break
struct TraverseLeftIgnoringLineBreak {
  STATIC_ONLY(TraverseLeftIgnoringLineBreak);

  static const InlineBox* Forward(const InlineBox& inline_box) {
    return inline_box.PrevLeafChildIgnoringLineBreak();
  }
};

// "Right" Traversal strategy
struct TraverseRight {
  STATIC_ONLY(TraverseRight);

  static const InlineBox* Forward(const InlineBox& inline_box) {
    return inline_box.NextLeafChild();
  }
};

// "Right" Traversal strategy ignoring line break
struct TraverseRightIgnoringLineBreak {
  STATIC_ONLY(TraverseRightIgnoringLineBreak);

  static const InlineBox* Forward(const InlineBox& inline_box) {
    return inline_box.NextLeafChildIgnoringLineBreak();
  }
};

template <typename TraversalStrategy>
const InlineBox* FindBidiRun(const InlineBox& start, unsigned bidi_level) {
  for (const InlineBox* runner = TraversalStrategy::Forward(start); runner;
       runner = TraversalStrategy::Forward(*runner)) {
    if (runner->BidiLevel() <= bidi_level)
      return runner;
  }
  return nullptr;
}

template <typename TraversalStrategy>
const InlineBox& FindBoudnaryOfBidiRun(const InlineBox& start,
                                       unsigned bidi_level) {
  const InlineBox* result = &start;
  for (const InlineBox* runner = TraversalStrategy::Forward(start); runner;
       runner = TraversalStrategy::Forward(*runner)) {
    if (runner->BidiLevel() <= bidi_level)
      return *result;
    result = runner;
  }
  return *result;
}

template <typename TraversalStrategy>
const InlineBox& FindBoudnaryOfEntireBidiRun(const InlineBox& start,
                                             unsigned bidi_level) {
  const InlineBox* result = &start;
  for (const InlineBox* runner = TraversalStrategy::Forward(start); runner;
       runner = TraversalStrategy::Forward(*runner)) {
    if (runner->BidiLevel() < bidi_level)
      return *result;
    result = runner;
  }
  return *result;
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
  return FindBoudnaryOfBidiRun<TraverseLeftIgnoringLineBreak>(inline_box,
                                                              bidi_level);
}

const InlineBox& InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRun(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoudnaryOfEntireBidiRun<TraverseLeft>(inline_box, bidi_level);
}

const InlineBox&
InlineBoxTraversal::FindLeftBoundaryOfEntireBidiRunIgnoringLineBreak(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoudnaryOfEntireBidiRun<TraverseLeftIgnoringLineBreak>(inline_box,
                                                                    bidi_level);
}

const InlineBox&
InlineBoxTraversal::FindRightBoundaryOfBidiRunIgnoringLineBreak(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoudnaryOfBidiRun<TraverseRightIgnoringLineBreak>(inline_box,
                                                               bidi_level);
}

const InlineBox& InlineBoxTraversal::FindRightBoundaryOfEntireBidiRun(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoudnaryOfEntireBidiRun<TraverseRight>(inline_box, bidi_level);
}

const InlineBox&
InlineBoxTraversal::FindRightBoundaryOfEntireBidiRunIgnoringLineBreak(
    const InlineBox& inline_box,
    unsigned bidi_level) {
  return FindBoudnaryOfEntireBidiRun<TraverseRightIgnoringLineBreak>(
      inline_box, bidi_level);
}

}  // namespace blink
