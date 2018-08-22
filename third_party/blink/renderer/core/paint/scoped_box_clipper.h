// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCOPED_BOX_CLIPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCOPED_BOX_CLIPPER_H_

#include "base/optional.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class DisplayItemClient;
class FragmentData;
class LayoutBox;
class NGPaintFragment;
struct PaintInfo;

// Within the scope of this object:
// - OverflowClip is applied if it exists
// - If it doesn't exist, then InnerBorderRadiusClip is applied if it exists.
// - If it doesn't exist, then properties are not modified.
// TODO(vmpstr): It should be possible to make this apply ContentsProperties
// instead of a contents clip only. However, there are situation where the
// LocalBorderBoxProperties().Clip(), which would be the fallback if neither
// OverflowClip nor InnerBorderRadiusClip exist, is not the correct clip to
// apply. We need to audit the usage to figure in which situations we want
// BoxClipper and in which situations we want OverflowClip or
// InnerBorderRadiusClip only.
class ScopedBoxClipper {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  ScopedBoxClipper(const LayoutBox&, const PaintInfo&);
  ScopedBoxClipper(const NGPaintFragment&, const PaintInfo&);

 private:
  void InitializeScopedProperties(const FragmentData*,
                                  const DisplayItemClient&,
                                  const PaintInfo&);

  base::Optional<ScopedPaintChunkProperties> scoped_properties_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SCOPED_BOX_BOX_CLIPPER_H_
