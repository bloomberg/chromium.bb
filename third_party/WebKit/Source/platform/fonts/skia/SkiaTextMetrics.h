// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SkiaTextMetrics_h
#define SkiaTextMetrics_h

#include "platform/fonts/Glyph.h"

#include <SkPaint.h>
#include <hb.h>

namespace blink {

class SkiaTextMetrics final {
 public:
  SkiaTextMetrics(const SkPaint*);

  void GetGlyphWidthForHarfBuzz(hb_codepoint_t, hb_position_t* width);
  void GetGlyphExtentsForHarfBuzz(hb_codepoint_t, hb_glyph_extents_t*);

  void GetSkiaBoundsForGlyph(Glyph, SkRect* bounds);
  float GetSkiaWidthForGlyph(Glyph);

  static hb_position_t SkiaScalarToHarfBuzzPosition(SkScalar value);

 private:
  const SkPaint* paint_;
};

}  // namespace blink

#endif
