// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapeResultBuffer_h
#define ShapeResultBuffer_h

#include "platform/PlatformExport.h"
#include "platform/fonts/shaping/ShapeResult.h"
#include "wtf/Allocator.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

struct CharacterRange;
class FontDescription;
struct GlyphData;
class ShapeResultBloberizer;
class TextRun;
struct TextRunPaintInfo;

class PLATFORM_EXPORT ShapeResultBuffer {
  WTF_MAKE_NONCOPYABLE(ShapeResultBuffer);
  STACK_ALLOCATED();

 public:
  ShapeResultBuffer() : m_hasVerticalOffsets(false) {}

  void appendResult(PassRefPtr<const ShapeResult> result) {
    m_hasVerticalOffsets |= result->hasVerticalOffsets();
    m_results.push_back(result);
  }

  bool hasVerticalOffsets() const { return m_hasVerticalOffsets; }

  float fillGlyphs(const TextRunPaintInfo&, ShapeResultBloberizer&) const;
  void fillTextEmphasisGlyphs(const TextRunPaintInfo&,
                              const GlyphData& emphasisData,
                              ShapeResultBloberizer&) const;
  int offsetForPosition(const TextRun&,
                        float targetX,
                        bool includePartialGlyphs) const;
  CharacterRange getCharacterRange(TextDirection,
                                   float totalWidth,
                                   unsigned from,
                                   unsigned to) const;
  Vector<CharacterRange> individualCharacterRanges(TextDirection,
                                                   float totalWidth) const;

  static CharacterRange getCharacterRange(RefPtr<const ShapeResult>,
                                          TextDirection,
                                          float totalWidth,
                                          unsigned from,
                                          unsigned to);

  struct RunFontData {
      SimpleFontData* m_fontData;
      size_t m_glyphCount;
  };

  Vector<RunFontData> runFontData() const;

  GlyphData emphasisMarkGlyphData(const FontDescription&) const;

 private:
  static CharacterRange getCharacterRangeInternal(
      const Vector<RefPtr<const ShapeResult>, 64>&,
      TextDirection,
      float totalWidth,
      unsigned from,
      unsigned to);

  float fillFastHorizontalGlyphs(const TextRun&, ShapeResultBloberizer&) const;

  static float fillGlyphsForResult(ShapeResultBloberizer&,
                                   const ShapeResult&,
                                   const TextRunPaintInfo&,
                                   float initialAdvance,
                                   unsigned runOffset);
  static float fillTextEmphasisGlyphsForRun(ShapeResultBloberizer&,
                                            const ShapeResult::RunInfo*,
                                            const TextRunPaintInfo&,
                                            const GlyphData&,
                                            float initialAdvance,
                                            unsigned runOffset);

  static void addRunInfoRanges(const ShapeResult::RunInfo&,
                               float offset,
                               Vector<CharacterRange>&);

  // Empirically, cases where we get more than 50 ShapeResults are extremely
  // rare.
  Vector<RefPtr<const ShapeResult>, 64> m_results;
  bool m_hasVerticalOffsets;
};

}  // namespace blink

#endif  // ShapeResultBuffer_h
