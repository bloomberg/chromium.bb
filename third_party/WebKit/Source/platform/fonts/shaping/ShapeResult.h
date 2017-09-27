/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ShapeResult_h
#define ShapeResult_h

#include <memory>
#include "platform/LayoutUnit.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/text/TextDirection.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"

struct hb_buffer_t;

namespace blink {

class Font;
template <typename TextContainerType>
class PLATFORM_EXPORT ShapeResultSpacing;
class SimpleFontData;
class TextRun;

class PLATFORM_EXPORT ShapeResult : public RefCounted<ShapeResult> {
 public:
  static RefPtr<ShapeResult> Create(const Font* font,
                                    unsigned num_characters,
                                    TextDirection direction) {
    return WTF::AdoptRef(new ShapeResult(font, num_characters, direction));
  }
  static RefPtr<ShapeResult> CreateForTabulationCharacters(
      const Font*,
      const TextRun&,
      float position_offset,
      unsigned count);
  ~ShapeResult();

  // Returns a mutable unique instance. If |this| has more than 1 ref count,
  // a clone is created.
  RefPtr<ShapeResult> MutableUnique() const;

  // The logical width of this result.
  float Width() const { return width_; }
  LayoutUnit SnappedWidth() const { return LayoutUnit::FromFloatCeil(width_); }
  // The glyph bounding box, in logical coordinates, using alphabetic baseline
  // even when the result is in vertical flow.
  const FloatRect& Bounds() const { return glyph_bounding_box_; }
  unsigned NumCharacters() const { return num_characters_; }
  // The character start/end index of a range shape result.
  unsigned StartIndexForResult() const;
  unsigned EndIndexForResult() const;
  void FallbackFonts(HashSet<const SimpleFontData*>*) const;
  TextDirection Direction() const {
    return static_cast<TextDirection>(direction_);
  }
  bool Rtl() const { return Direction() == TextDirection::kRtl; }

  // True if at least one glyph in this result has vertical offsets.
  //
  // Vertical result always has vertical offsets, but horizontal result may also
  // have vertical offsets.
  bool HasVerticalOffsets() const { return has_vertical_offsets_; }

  // For memory reporting.
  size_t ByteSize() const;

  // Returns the next or previous offsets respectively at which it is safe to
  // break without reshaping.
  unsigned NextSafeToBreakOffset(unsigned offset) const;
  unsigned PreviousSafeToBreakOffset(unsigned offset) const;

  unsigned OffsetForPosition(float target_x, bool include_partial_glyphs) const;
  float PositionForOffset(unsigned offset) const;
  LayoutUnit SnappedStartPositionForOffset(unsigned offset) const {
    return LayoutUnit::FromFloatFloor(PositionForOffset(offset));
  }
  LayoutUnit SnappedEndPositionForOffset(unsigned offset) const {
    return LayoutUnit::FromFloatCeil(PositionForOffset(offset));
  }

  // Apply spacings (letter-spacing, word-spacing, and justification) as
  // configured to |ShapeResultSpacing|.
  // |text_start_offset| adjusts the character index in the ShapeResult before
  // giving it to |ShapeResultSpacing|. It can be negative if
  // |StartIndexForResult()| is larger than the text in |ShapeResultSpacing|.
  void ApplySpacing(ShapeResultSpacing<String>&, int text_start_offset = 0);
  RefPtr<ShapeResult> ApplySpacingToCopy(ShapeResultSpacing<TextRun>&,
                                         const TextRun&) const;

  void CopyRange(unsigned start, unsigned end, ShapeResult*) const;

 protected:
  struct RunInfo;

  ShapeResult(const Font*, unsigned num_characters, TextDirection);
  ShapeResult(const ShapeResult&);

  static RefPtr<ShapeResult> Create(const ShapeResult& other) {
    return WTF::AdoptRef(new ShapeResult(other));
  }

  template <typename TextContainerType>
  void ApplySpacingImpl(ShapeResultSpacing<TextContainerType>&,
                        int text_start_offset = 0);
  template <bool is_horizontal_run>
  void ComputeGlyphPositions(ShapeResult::RunInfo*,
                             unsigned start_glyph,
                             unsigned num_glyphs,
                             hb_buffer_t*,
                             FloatRect* glyph_bounding_box);
  void InsertRun(std::unique_ptr<ShapeResult::RunInfo>,
                 unsigned start_glyph,
                 unsigned num_glyphs,
                 hb_buffer_t*);
  void ReorderRtlRuns(unsigned run_size_before);

  float width_;
  FloatRect glyph_bounding_box_;
  Vector<std::unique_ptr<RunInfo>> runs_;
  RefPtr<SimpleFontData> primary_font_;

  unsigned num_characters_;
  unsigned num_glyphs_ : 30;

  // Overall direction for the TextRun, dictates which order each individual
  // sub run (represented by RunInfo structs in the m_runs vector) can have a
  // different text direction.
  unsigned direction_ : 1;

  // Tracks whether any runs contain glyphs with a y-offset != 0.
  unsigned has_vertical_offsets_ : 1;

  friend class HarfBuzzShaper;
  friend class ShapeResultBuffer;
  friend class ShapeResultBloberizer;
};

}  // namespace blink

#endif  // ShapeResult_h
