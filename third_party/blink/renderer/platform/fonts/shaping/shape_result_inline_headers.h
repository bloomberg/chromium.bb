/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 BlackBerry Limited. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_INLINE_HEADERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_INLINE_HEADERS_H_

#include <hb.h>
#include <memory>
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"

namespace blink {

class SimpleFontData;

struct HarfBuzzRunGlyphData {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

  static constexpr unsigned kMaxCharacterIndex = (1 << 15) - 1;

  uint16_t glyph;
  unsigned character_index : 15;
  unsigned safe_to_break_before : 1;
  float advance;
  FloatSize offset;

  void SetGlyphAndPositions(uint16_t glyph_id,
                            uint16_t character_index,
                            float advance,
                            const FloatSize& offset,
                            bool safe_to_break_before);
};

struct ShapeResult::RunInfo {
  USING_FAST_MALLOC(RunInfo);

 public:
  RunInfo(const SimpleFontData* font,
          hb_direction_t dir,
          CanvasRotationInVertical canvas_rotation,
          hb_script_t script,
          unsigned start_index,
          unsigned num_glyphs,
          unsigned num_characters)
      : font_data_(const_cast<SimpleFontData*>(font)),
        direction_(dir),
        canvas_rotation_(canvas_rotation),
        script_(script),
        glyph_data_(num_glyphs),
        start_index_(start_index),
        num_characters_(num_characters),
        width_(0.0f) {}

  RunInfo(const RunInfo& other)
      : font_data_(other.font_data_),
        direction_(other.direction_),
        canvas_rotation_(other.canvas_rotation_),
        script_(other.script_),
        glyph_data_(other.glyph_data_),
        start_index_(other.start_index_),
        num_characters_(other.num_characters_),
        width_(other.width_) {}

  unsigned NumGlyphs() const { return glyph_data_.size(); }
  bool Rtl() const { return HB_DIRECTION_IS_BACKWARD(direction_); }
  bool IsHorizontal() const { return HB_DIRECTION_IS_HORIZONTAL(direction_); }
  CanvasRotationInVertical CanvasRotation() const { return canvas_rotation_; }
  unsigned NextSafeToBreakOffset(unsigned) const;
  unsigned PreviousSafeToBreakOffset(unsigned) const;
  float XPositionForVisualOffset(unsigned, AdjustMidCluster) const;
  float XPositionForOffset(unsigned, AdjustMidCluster) const;
  void CharacterIndexForXPosition(float, GlyphIndexResult*) const;

  size_t GlyphToCharacterIndex(size_t i) const {
    return start_index_ + glyph_data_[i].character_index;
  }

  // For memory reporting.
  size_t ByteSize() const {
    return sizeof(this) + glyph_data_.size() * sizeof(HarfBuzzRunGlyphData);
  }

  // Creates a new RunInfo instance representing a subset of the current run.
  std::unique_ptr<RunInfo> CreateSubRun(unsigned start, unsigned end) {
    DCHECK(end > start);
    unsigned number_of_characters = std::min(end - start, num_characters_);

    // This is incorrect but is a resonable guess for the initial size of the
    // glyph_data_ vector. It'll be resized once we've determined the number of
    // glyphs in the sub run.
    unsigned number_of_glyphs = number_of_characters;
    auto run = std::make_unique<RunInfo>(
        font_data_.get(), direction_, canvas_rotation_, script_,
        start_index_ + start, number_of_glyphs, number_of_characters);

    number_of_glyphs = 0;
    float total_advance = 0;
    ForEachGlyphInRange(
        0, start_index_ + start, start_index_ + end, 0,
        [&](const HarfBuzzRunGlyphData& glyph_data, float, uint16_t) -> bool {
          auto& sub_glyph = run->glyph_data_[number_of_glyphs++];
          sub_glyph.glyph = glyph_data.glyph;
          sub_glyph.character_index = glyph_data.character_index - start;
          sub_glyph.safe_to_break_before = glyph_data.safe_to_break_before;
          sub_glyph.advance = glyph_data.advance;
          sub_glyph.offset = glyph_data.offset;
          total_advance += glyph_data.advance;
          return true;
        });

    // Shrink glyph_data_ as the number of glyphs in the sub run is likely
    // smaller than in the original.
    DCHECK(run->glyph_data_.size() >= number_of_glyphs);
    run->glyph_data_.resize(number_of_glyphs);

    run->width_ = total_advance;
    run->num_characters_ = number_of_characters;

    return run;
  }

  // Iterates over, and applies the functor to all the glyphs in this run.
  // Also tracks (and returns) a seeded total advance.
  //
  // Functor signature:
  //
  //   bool func(const HarfBuzzRunGlyphData& glyphData, float totalAdvance)
  //
  // where the returned bool signals whether iteration should continue (true)
  // or stop (false).
  template <typename Func>
  float ForEachGlyph(float initial_advance, Func func) const {
    float total_advance = initial_advance;

    for (const auto& glyph_data : glyph_data_) {
      if (!func(glyph_data, total_advance))
        break;
      total_advance += glyph_data.advance;
    }

    return total_advance;
  }

  // Same as the above, except it only applies the functor to glyphs in the
  // specified range, and stops after the range.
  template <typename Func>
  float ForEachGlyphInRange(float initial_advance,
                            unsigned from,
                            unsigned to,
                            unsigned index_offset,
                            Func func) const {
    return ForEachGlyph(
        initial_advance,
        [&](const HarfBuzzRunGlyphData& glyph_data,
            float total_advance) -> bool {
          const unsigned character_index =
              start_index_ + glyph_data.character_index + index_offset;

          if (character_index < from) {
            // Glyph out-of-range; before the range (and must continue
            // accumulating advance) in LTR.
            return !Rtl();
          }

          if (character_index >= to) {
            // Glyph out-of-range; before the range (and must continue
            // accumulating advance) in RTL.
            return Rtl();
          }

          // Glyph in range; apply functor.
          return func(glyph_data, total_advance, character_index);
        });
  }

  scoped_refptr<SimpleFontData> font_data_;
  hb_direction_t direction_;
  // For upright-in-vertical we need to tell the ShapeResultBloberizer to rotate
  // the canvas back 90deg for this RunInfo.
  CanvasRotationInVertical canvas_rotation_;
  hb_script_t script_;
  Vector<HarfBuzzRunGlyphData> glyph_data_;
  // List of character indecies before which it's safe to break without
  // reshaping.
  unsigned start_index_;
  unsigned num_characters_;
  float width_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_SHAPE_RESULT_INLINE_HEADERS_H_
