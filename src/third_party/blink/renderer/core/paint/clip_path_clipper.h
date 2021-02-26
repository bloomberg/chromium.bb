// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CLIP_PATH_CLIPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CLIP_PATH_CLIPPER_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/path.h"

namespace blink {

class DisplayItemClient;
class GraphicsContext;
class LayoutObject;
struct PhysicalOffset;

class CORE_EXPORT ClipPathClipper {
  STATIC_ONLY(ClipPathClipper);

 public:
  static void PaintClipPathAsMaskImage(GraphicsContext&,
                                       const LayoutObject&,
                                       const DisplayItemClient&,
                                       const PhysicalOffset& paint_offset);

  // Returns the reference box used by CSS clip-path. For HTML objects,
  // this is the border box of the element. For SVG objects this is the
  // object bounding box.
  static FloatRect LocalReferenceBox(const LayoutObject&);

  // Returns the bounding box of the computed clip path, which could be
  // smaller or bigger than the reference box. Returns nullopt if the
  // clip path is invalid.
  static base::Optional<FloatRect> LocalClipPathBoundingBox(
      const LayoutObject&);

  // Returns true if the object has a clip-path that must be implemented with
  // a mask.
  static bool ShouldUseMaskBasedClip(const LayoutObject&);

  // The argument |clip_path_owner| is the layout object that owns the
  // ClipPathOperation we are currently processing. Usually it is the
  // same as the layout object getting clipped, but in the case of nested
  // clip-path, it could be one of the SVG clip path in the chain.
  // Returns the path if the clip-path can use path-based clip.
  static base::Optional<Path> PathBasedClip(
      const LayoutObject& clip_path_owner);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_CLIP_PATH_CLIPPER_H_
