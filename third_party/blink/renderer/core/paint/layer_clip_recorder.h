// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LAYER_CLIP_RECORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LAYER_CLIP_RECORDER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FloatRoundedRect;
class LayoutPoint;
class PaintLayer;

// TODO(wangxianzhu): Either rename the class, or merge the code into the only
// caller of LayerClipRecorder::CollectRoundedRectClips().

class CORE_EXPORT LayerClipRecorder {
 public:
  enum BorderRadiusClippingRule {
    kIncludeSelfForBorderRadius,
    kDoNotIncludeSelfForBorderRadius
  };

  // Build a vector of the border radius clips that should be applied to
  // the given PaintLayer, walking up the paint layer tree to the clip_root.
  // The fragment_offset is an offset to apply to the clip to position it
  // in the required clipping coordinates (for cases when the painting
  // coordinate system is offset from the layer coordinate system).
  // cross_composited_scrollers should be true when the search for clips should
  // continue even if the clipping layer is painting into a composited scrolling
  // layer, as when painting a mask for a child of the scroller.
  // The BorderRadiusClippingRule defines whether clips on the PaintLayer itself
  // are included. Output is appended to rounded_rect_clips.
  static void CollectRoundedRectClips(
      const PaintLayer&,
      const PaintLayer* clip_root,
      const LayoutPoint& fragment_offset,
      bool cross_composited_scrollers,
      BorderRadiusClippingRule,
      Vector<FloatRoundedRect>& rounded_rect_clips);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LAYER_CLIP_RECORDER_H_
