// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/PaintInvalidationReason.h"

#include "platform/wtf/Assertions.h"

namespace blink {

const char* PaintInvalidationReasonToString(PaintInvalidationReason reason) {
  switch (reason) {
    case PaintInvalidationReason::kNone:
      return "none";
    case PaintInvalidationReason::kIncremental:
      return "incremental";
    case PaintInvalidationReason::kRectangle:
      return "invalidate paint rectangle";
    case PaintInvalidationReason::kFull:
      return "full";
    case PaintInvalidationReason::kStyle:
      return "style change";
    case PaintInvalidationReason::kGeometry:
      return "geometry";
    case PaintInvalidationReason::kCompositing:
      return "compositing update";
    case PaintInvalidationReason::kBackground:
      return "background";
    case PaintInvalidationReason::kAppeared:
      return "appeared";
    case PaintInvalidationReason::kDisappeared:
      return "disappeared";
    case PaintInvalidationReason::kScroll:
      return "scroll";
    case PaintInvalidationReason::kScrollControl:
      return "scroll control";
    case PaintInvalidationReason::kSelection:
      return "selection";
    case PaintInvalidationReason::kOutline:
      return "outline";
    case PaintInvalidationReason::kSubtree:
      return "subtree";
    case PaintInvalidationReason::kSVGResource:
      return "SVG resource change";
    case PaintInvalidationReason::kBackgroundOnScrollingContentsLayer:
      return "background on scrolling contents layer";
    case PaintInvalidationReason::kCaret:
      return "caret";
    case PaintInvalidationReason::kDocumentMarker:
      return "DocumentMarker change";
    case PaintInvalidationReason::kImage:
      return "image";
    case PaintInvalidationReason::kChunkUncacheable:
      return "chunk uncacheable";
    case PaintInvalidationReason::kChunkReordered:
      return "chunk reordered";
    case PaintInvalidationReason::kFullLayer:
      return "full layer";
    case PaintInvalidationReason::kPaintProperty:
      return "paint property change";
    case PaintInvalidationReason::kForTesting:
      return "for testing";
    case PaintInvalidationReason::kDelayedFull:
      return "delayed full";
  }
  NOTREACHED();
  return "";
}

std::ostream& operator<<(std::ostream& out, PaintInvalidationReason reason) {
  return out << PaintInvalidationReasonToString(reason);
}

}  // namespace blink
