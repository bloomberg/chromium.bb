// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShapeResultBuffer_h
#define ShapeResultBuffer_h

#include "platform/PlatformExport.h"
#include "platform/fonts/shaping/ShapeResult.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"

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
  ShapeResultBuffer() : has_vertical_offsets_(false) {}

  void AppendResult(PassRefPtr<const ShapeResult> result) {
    has_vertical_offsets_ |= result->HasVerticalOffsets();
    results_.push_back(std::move(result));
  }

  bool HasVerticalOffsets() const { return has_vertical_offsets_; }

  float FillGlyphs(const TextRunPaintInfo&, ShapeResultBloberizer&) const;
  void FillTextEmphasisGlyphs(const TextRunPaintInfo&,
                              const GlyphData& emphasis_data,
                              ShapeResultBloberizer&) const;
  int OffsetForPosition(const TextRun&,
                        float target_x,
                        bool include_partial_glyphs) const;
  CharacterRange GetCharacterRange(TextDirection,
                                   float total_width,
                                   unsigned from,
                                   unsigned to) const;
  Vector<CharacterRange> IndividualCharacterRanges(TextDirection,
                                                   float total_width) const;

  static CharacterRange GetCharacterRange(RefPtr<const ShapeResult>,
                                          TextDirection,
                                          float total_width,
                                          unsigned from,
                                          unsigned to);

  struct RunFontData {
    SimpleFontData* font_data_;
    size_t glyph_count_;
  };

  Vector<RunFontData> GetRunFontData() const;

  GlyphData EmphasisMarkGlyphData(const FontDescription&) const;

 private:
  static CharacterRange GetCharacterRangeInternal(
      const Vector<RefPtr<const ShapeResult>, 64>&,
      TextDirection,
      float total_width,
      unsigned from,
      unsigned to);

  float FillFastHorizontalGlyphs(const TextRun&, ShapeResultBloberizer&) const;

  static float FillGlyphsForResult(ShapeResultBloberizer&,
                                   const ShapeResult&,
                                   const TextRunPaintInfo&,
                                   float initial_advance,
                                   unsigned run_offset);
  static float FillTextEmphasisGlyphsForRun(ShapeResultBloberizer&,
                                            const ShapeResult::RunInfo*,
                                            const TextRunPaintInfo&,
                                            const GlyphData&,
                                            float initial_advance,
                                            unsigned run_offset);

  static void AddRunInfoRanges(const ShapeResult::RunInfo&,
                               float offset,
                               Vector<CharacterRange>&);

  // Empirically, cases where we get more than 50 ShapeResults are extremely
  // rare.
  Vector<RefPtr<const ShapeResult>, 64> results_;
  bool has_vertical_offsets_;
};

}  // namespace blink

#endif  // ShapeResultBuffer_h
