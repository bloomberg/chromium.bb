// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapeResultTestInfo_h
#define ShapeResultTestInfo_h

#include "platform/fonts/shaping/HarfBuzzShaper.h"
#include "platform/fonts/shaping/ShapeResultBloberizer.h"

#include <hb.h>

namespace blink {

class PLATFORM_EXPORT ShapeResultTestInfo : public ShapeResult {
 public:
  unsigned NumberOfRunsForTesting() const;
  bool RunInfoForTesting(unsigned run_index,
                         unsigned& start_index,
                         unsigned& num_glyphs,
                         hb_script_t&) const;
  bool RunInfoForTesting(unsigned run_index,
                         unsigned& start_index,
                         unsigned& num_characters,
                         unsigned& num_glyphs,
                         hb_script_t&) const;
  uint16_t GlyphForTesting(unsigned run_index, size_t glyph_index) const;
  float AdvanceForTesting(unsigned run_index, size_t glyph_index) const;
  SimpleFontData* FontDataForTesting(unsigned run_index) const;
};

class PLATFORM_EXPORT ShapeResultBloberizerTestInfo {
 public:
  static const SimpleFontData* PendingRunFontData(
      const ShapeResultBloberizer& bloberizer) {
    return bloberizer.pending_font_data_;
  }

  static const Vector<Glyph, 1024>& PendingRunGlyphs(
      const ShapeResultBloberizer& bloberizer) {
    return bloberizer.pending_glyphs_;
  }

  static const Vector<float, 1024>& PendingRunOffsets(
      const ShapeResultBloberizer& bloberizer) {
    return bloberizer.pending_offsets_;
  }

  static bool HasPendingRunVerticalOffsets(
      const ShapeResultBloberizer& bloberizer) {
    return bloberizer.HasPendingVerticalOffsets();
  }

  static size_t PendingBlobRunCount(const ShapeResultBloberizer& bloberizer) {
    return bloberizer.builder_run_count_;
  }

  static size_t CommittedBlobCount(const ShapeResultBloberizer& bloberizer) {
    return bloberizer.blobs_.size();
  }
};

}  // namespace blink

#endif  // ShapeResultTestInfo_h
