// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MapCoordinatesFlags_h
#define MapCoordinatesFlags_h

namespace blink {

enum MapCoordinatesMode {
  kIsFixed = 1 << 0,
  kUseTransforms = 1 << 1,

  // When walking up the containing block chain, applies a container flip for
  // the first element found, if any, for which isFlippedBlocksWritingMode is
  // true. This option should generally be used when mapping a source rect in
  // the "physical coordinates with flipped block-flow" coordinate space (see
  // LayoutBoxModelObject.h) to one in a physical destination space.
  kApplyContainerFlip = 1 << 2,
  kTraverseDocumentBoundaries = 1 << 3,

  // Applies to LayoutView::mapLocalToAncestor() and LayoutView::
  // mapToVisualRectInAncestorSpace() only, to indicate the input point or rect
  // is in frame coordinates instead of frame contents coordinates. This
  // disables view clipping and scroll offset adjustment.
  // TODO(wangxianzhu): Remove this when root-layer-scrolls launches.
  kInputIsInFrameCoordinates = 1 << 4,

  // Ignore offset adjustments caused by position:sticky calculations when
  // walking the chain.
  kIgnoreStickyOffset = 1 << 5,
};
typedef unsigned MapCoordinatesFlags;

}  // namespace blink

#endif  // MapCoordinatesFlags_h
