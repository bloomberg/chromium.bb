// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/PaintInvalidationReason.h"

#include "platform/wtf/Assertions.h"

namespace blink {

const char* PaintInvalidationReasonToString(PaintInvalidationReason reason) {
  switch (reason) {
    case kPaintInvalidationNone:
      return "none";
    case kPaintInvalidationIncremental:
      return "incremental";
    case kPaintInvalidationRectangle:
      return "invalidate paint rectangle";
    case kPaintInvalidationFull:
      return "full";
    case kPaintInvalidationStyleChange:
      return "style change";
    case kPaintInvalidationForcedByLayout:
      return "forced by layout";
    case kPaintInvalidationCompositingUpdate:
      return "compositing update";
    case kPaintInvalidationBorderBoxChange:
      return "border box change";
    case kPaintInvalidationContentBoxChange:
      return "content box change";
    case kPaintInvalidationLayoutOverflowBoxChange:
      return "layout overflow box change";
    case kPaintInvalidationBoundsChange:
      return "bounds change";
    case kPaintInvalidationLocationChange:
      return "location change";
    case kPaintInvalidationBackgroundObscurationChange:
      return "background obscuration change";
    case kPaintInvalidationBecameVisible:
      return "became visible";
    case kPaintInvalidationBecameInvisible:
      return "became invisible";
    case kPaintInvalidationScroll:
      return "scroll";
    case kPaintInvalidationSelection:
      return "selection";
    case kPaintInvalidationOutline:
      return "outline";
    case kPaintInvalidationSubtree:
      return "subtree";
    case kPaintInvalidationLayoutObjectInsertion:
      return "layoutObject insertion";
    case kPaintInvalidationLayoutObjectRemoval:
      return "layoutObject removal";
    case kPaintInvalidationSVGResourceChange:
      return "SVG resource change";
    case kPaintInvalidationBackgroundOnScrollingContentsLayer:
      return "background on scrolling contents layer";
    case kPaintInvalidationCaret:
      return "caret";
    case kPaintInvalidationViewBackground:
      return "view background";
    case kPaintInvalidationDocumentMarkerChange:
      return "DocumentMarker change";
    case kPaintInvalidationForTesting:
      return "for testing";
    case kPaintInvalidationDelayedFull:
      return "delayed full";
  }
  NOTREACHED();
  return "";
}

std::ostream& operator<<(std::ostream& out, PaintInvalidationReason reason) {
  return out << PaintInvalidationReasonToString(reason);
}

}  // namespace blink
