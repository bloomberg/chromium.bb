// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipPathClipper_h
#define ClipPathClipper_h

#include "platform/graphics/paint/ClipPathRecorder.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/wtf/Optional.h"

namespace blink {

class ClipPathOperation;
class FloatPoint;
class FloatRect;
class GraphicsContext;
class LayoutSVGResourceClipper;
class LayoutObject;

enum class ClipperState { kNotApplied, kAppliedPath, kAppliedMask };

class ClipPathClipper {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  ClipPathClipper(GraphicsContext&,
                  ClipPathOperation&,
                  const LayoutObject&,
                  const FloatRect& reference_box,
                  const FloatPoint& origin);
  ~ClipPathClipper();

  bool UsingMask() const {
    return clipper_state_ == ClipperState::kAppliedMask;
  }

 private:
  // Returns false if there is a problem drawing the mask.
  bool PrepareEffect(const FloatRect& target_bounding_box,
                     const FloatRect& visual_rect,
                     const FloatPoint& layer_position_offset);
  bool DrawClipAsMask(const FloatRect& target_bounding_box,
                      const AffineTransform&,
                      const FloatPoint&);
  void FinishEffect();

  LayoutSVGResourceClipper* resource_clipper_;
  ClipperState clipper_state_;
  const LayoutObject& layout_object_;
  GraphicsContext& context_;

  // TODO(pdr): This pattern should be cleaned up so that the recorders are just
  // on the stack.
  Optional<ClipPathRecorder> clip_path_recorder_;
  Optional<CompositingRecorder> mask_clip_recorder_;
  Optional<CompositingRecorder> mask_content_recorder_;
};

}  // namespace blink

#endif  // ClipPathClipper_h
