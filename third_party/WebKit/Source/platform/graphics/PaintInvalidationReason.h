// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintInvalidationReason_h
#define PaintInvalidationReason_h

#include <iosfwd>
#include "platform/PlatformExport.h"

namespace blink {

enum PaintInvalidationReason {
  kPaintInvalidationNone,
  kPaintInvalidationIncremental,
  kPaintInvalidationRectangle,
  // The following reasons will all cause full invalidation of the LayoutObject.
  kPaintInvalidationFull,  // Any unspecified reason of full invalidation.
  kPaintInvalidationStyleChange,
  kPaintInvalidationForcedByLayout,
  kPaintInvalidationCompositingUpdate,
  kPaintInvalidationBorderBoxChange,
  kPaintInvalidationContentBoxChange,
  kPaintInvalidationLayoutOverflowBoxChange,
  kPaintInvalidationBoundsChange,
  kPaintInvalidationLocationChange,
  kPaintInvalidationBackgroundObscurationChange,
  kPaintInvalidationBecameVisible,
  kPaintInvalidationBecameInvisible,
  kPaintInvalidationScroll,
  kPaintInvalidationSelection,
  kPaintInvalidationOutline,
  kPaintInvalidationSubtree,
  kPaintInvalidationLayoutObjectInsertion,
  kPaintInvalidationLayoutObjectRemoval,
  kPaintInvalidationSVGResourceChange,
  kPaintInvalidationBackgroundOnScrollingContentsLayer,
  kPaintInvalidationCaret,
  kPaintInvalidationViewBackground,
  kPaintInvalidationDocumentMarkerChange,
  kPaintInvalidationForTesting,
  // PaintInvalidationDelayedFull means that PaintInvalidationFull is needed in
  // order to fully paint the content, but that painting of the object can be
  // delayed until a future frame. This can be the case for an object whose
  // content is not visible to the user.
  kPaintInvalidationDelayedFull,

  kPaintInvalidationReasonMax = kPaintInvalidationDelayedFull
};

PLATFORM_EXPORT const char* PaintInvalidationReasonToString(
    PaintInvalidationReason);

inline bool IsFullPaintInvalidationReason(PaintInvalidationReason reason) {
  return reason >= kPaintInvalidationFull;
}

inline bool IsImmediateFullPaintInvalidationReason(
    PaintInvalidationReason reason) {
  return IsFullPaintInvalidationReason(reason) &&
         reason != kPaintInvalidationDelayedFull;
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&,
                                         PaintInvalidationReason);

}  // namespace blink

#endif  // PaintInvalidationReason_h
