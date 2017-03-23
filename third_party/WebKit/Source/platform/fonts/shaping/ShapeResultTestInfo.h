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
  unsigned numberOfRunsForTesting() const;
  bool runInfoForTesting(unsigned runIndex,
                         unsigned& startIndex,
                         unsigned& numGlyphs,
                         hb_script_t&) const;
  uint16_t glyphForTesting(unsigned runIndex, size_t glyphIndex) const;
  float advanceForTesting(unsigned runIndex, size_t glyphIndex) const;
  SimpleFontData* fontDataForTesting(unsigned runIndex) const;
};

class PLATFORM_EXPORT ShapeResultBloberizerTestInfo {
 public:
  static const SimpleFontData* pendingRunFontData(
      const ShapeResultBloberizer& bloberizer) {
    return bloberizer.m_pendingFontData;
  }

  static const Vector<Glyph, 1024>& pendingRunGlyphs(
      const ShapeResultBloberizer& bloberizer) {
    return bloberizer.m_pendingGlyphs;
  }

  static const Vector<float, 1024>& pendingRunOffsets(
      const ShapeResultBloberizer& bloberizer) {
    return bloberizer.m_pendingOffsets;
  }

  static bool hasPendingRunVerticalOffsets(
      const ShapeResultBloberizer& bloberizer) {
    return bloberizer.hasPendingVerticalOffsets();
  }

  static size_t pendingBlobRunCount(const ShapeResultBloberizer& bloberizer) {
    return bloberizer.m_builderRunCount;
  }

  static size_t committedBlobCount(const ShapeResultBloberizer& bloberizer) {
    return bloberizer.m_blobs.size();
  }
};

}  // namespace blink

#endif  // ShapeResultTestInfo_h
