// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipPathClipper_h
#define ClipPathClipper_h

#include "core/paint/FloatClipRecorder.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/ClipPathRecorder.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/wtf/Optional.h"

namespace blink {

class GraphicsContext;
class LayoutObject;

class ClipPathClipper {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  ClipPathClipper(GraphicsContext&,
                  const LayoutObject&,
                  const LayoutPoint& paint_offset);
  ~ClipPathClipper();

  bool IsIsolationInstalled() const { return !!mask_isolation_recorder_; }

  // Returns the reference box used by CSS clip-path. For HTML objects,
  // this is the border box of the element. For SVG objects this is the
  // object bounding box.
  static FloatRect LocalReferenceBox(const LayoutObject&);
  // Returns the bounding box of the computed clip path, which could be
  // smaller or bigger than the reference box. Returns nullopt if the
  // clip path is invalid.
  static Optional<FloatRect> LocalClipPathBoundingBox(const LayoutObject&);
  // The argument |clip_path_owner| is the layout object that owns the
  // ClipPathOperation we are currently processing. Usually it is the
  // same as the layout object getting clipped, but in the case of nested
  // clip-path, it could be one of the SVG clip path in the chain.
  // The output is tri-state:
  // is_valid == false: The clip path is invalid. Always returns nullopt.
  // is_valid == true && return == nullopt: The clip path is valid,
  //   but cannot use path-based clip.
  // is_valid == true && return != nullopt: The clip path can be applied
  //   as path-based clip, and the computed path is returned.
  static Optional<Path> PathBasedClip(const LayoutObject& clip_path_owner,
                                      bool is_svg_child,
                                      const FloatRect& reference_box,
                                      bool& is_valid);

 private:
  GraphicsContext& context_;
  const LayoutObject& layout_object_;
  LayoutPoint paint_offset_;

  Optional<FloatClipRecorder> clip_recorder_;
  Optional<ClipPathRecorder> clip_path_recorder_;
  Optional<CompositingRecorder> mask_isolation_recorder_;
};

}  // namespace blink

#endif  // ClipPathClipper_h
