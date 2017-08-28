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

#include "platform/fonts/shaping/ShapeResult.h"

#include <hb.h>
#include <memory>
#include "platform/fonts/Font.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"
#include "platform/fonts/shaping/ShapeResultSpacing.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

float ShapeResult::RunInfo::XPositionForVisualOffset(
    unsigned offset,
    AdjustMidCluster adjust_mid_cluster) const {
  DCHECK_LT(offset, num_characters_);
  if (Rtl())
    offset = num_characters_ - offset - 1;
  return XPositionForOffset(offset, adjust_mid_cluster);
}

float ShapeResult::RunInfo::XPositionForOffset(
    unsigned offset,
    AdjustMidCluster adjust_mid_cluster) const {
  DCHECK_LE(offset, num_characters_);
  const unsigned num_glyphs = glyph_data_.size();
  unsigned glyph_index = 0;
  float position = 0;
  if (Rtl()) {
    while (glyph_index < num_glyphs &&
           glyph_data_[glyph_index].character_index > offset) {
      position += glyph_data_[glyph_index].advance;
      ++glyph_index;
    }
    // Adjust offset if it's not on the cluster boundary. In RTL, this means
    // that the adjusted position is the left side of the character.
    if (adjust_mid_cluster == kAdjustToEnd &&
        (glyph_index < num_glyphs ? glyph_data_[glyph_index].character_index
                                  : num_characters_) < offset) {
      return position;
    }
    // For RTL, we need to return the right side boundary of the character.
    // Add advance of glyphs which are part of the character.
    while (glyph_index < num_glyphs - 1 &&
           glyph_data_[glyph_index].character_index ==
               glyph_data_[glyph_index + 1].character_index) {
      position += glyph_data_[glyph_index].advance;
      ++glyph_index;
    }
    position += glyph_data_[glyph_index].advance;
  } else {
    while (glyph_index < num_glyphs &&
           glyph_data_[glyph_index].character_index < offset) {
      position += glyph_data_[glyph_index].advance;
      ++glyph_index;
    }
    // Adjust offset if it's not on the cluster boundary.
    if (adjust_mid_cluster == kAdjustToStart && glyph_index &&
        (glyph_index < num_glyphs ? glyph_data_[glyph_index].character_index
                                  : num_characters_) > offset) {
      offset = glyph_data_[--glyph_index].character_index;
      for (; glyph_data_[glyph_index].character_index == offset;
           --glyph_index) {
        position -= glyph_data_[glyph_index].advance;
        if (!glyph_index)
          break;
      }
    }
  }
  return position;
}

static bool TargetPastEdge(bool rtl, float target_x, float next_x) {
  // In LTR, the edge belongs to the character on right.
  if (!rtl)
    return target_x < next_x;

  // In RTL, the edge belongs to the character on left.
  return target_x <= next_x;
}

int ShapeResult::RunInfo::CharacterIndexForXPosition(
    float target_x,
    bool include_partial_glyphs) const {
  DCHECK(target_x >= 0 && target_x <= width_);
  if (target_x <= 0)
    return !Rtl() ? 0 : num_characters_;
  const unsigned num_glyphs = glyph_data_.size();
  float current_x = 0;
  float current_advance = 0;
  unsigned glyph_index = 0;
  unsigned prev_character_index = num_characters_;  // used only when rtl()

  while (glyph_index < num_glyphs) {
    float prev_advance = current_advance;
    unsigned current_character_index = glyph_data_[glyph_index].character_index;
    current_advance = glyph_data_[glyph_index].advance;
    while (glyph_index < num_glyphs - 1 &&
           current_character_index ==
               glyph_data_[glyph_index + 1].character_index)
      current_advance += glyph_data_[++glyph_index].advance;
    float next_x;
    if (include_partial_glyphs) {
      // For hit testing, find the closest caret point by incuding
      // end-half of the previous character and start-half of the current
      // character.
      current_advance = current_advance / 2.0;
      next_x = current_x + prev_advance + current_advance;
      // When include_partial_glyphs, "<=" or "<" is not a big deal because
      // |next_x| is not at the character boundary.
      if (target_x <= next_x)
        return Rtl() ? prev_character_index : current_character_index;
    } else {
      next_x = current_x + current_advance;
      if (TargetPastEdge(Rtl(), target_x, next_x))
        return current_character_index;
    }
    current_x = next_x;
    prev_character_index = current_character_index;
    ++glyph_index;
  }

  return Rtl() ? 0 : num_characters_;
}

void ShapeResult::RunInfo::SetGlyphAndPositions(unsigned index,
                                                uint16_t glyph_id,
                                                float advance,
                                                float offset_x,
                                                float offset_y) {
  HarfBuzzRunGlyphData& data = glyph_data_[index];
  data.glyph = glyph_id;
  data.advance = advance;
  data.offset = FloatSize(offset_x, offset_y);
}

ShapeResult::ShapeResult(const Font* font,
                         unsigned num_characters,
                         TextDirection direction)
    : width_(0),
      primary_font_(const_cast<SimpleFontData*>(font->PrimaryFont())),
      num_characters_(num_characters),
      num_glyphs_(0),
      direction_(static_cast<unsigned>(direction)),
      has_vertical_offsets_(0) {}

ShapeResult::ShapeResult(const ShapeResult& other)
    : width_(other.width_),
      glyph_bounding_box_(other.glyph_bounding_box_),
      primary_font_(other.primary_font_),
      num_characters_(other.num_characters_),
      num_glyphs_(other.num_glyphs_),
      direction_(other.direction_),
      has_vertical_offsets_(other.has_vertical_offsets_) {
  runs_.ReserveCapacity(other.runs_.size());
  for (const auto& run : other.runs_)
    runs_.push_back(base::MakeUnique<RunInfo>(*run));
}

ShapeResult::~ShapeResult() {}

size_t ShapeResult::ByteSize() const {
  size_t self_byte_size = sizeof(this);
  for (unsigned i = 0; i < runs_.size(); ++i) {
    self_byte_size += runs_[i]->ByteSize();
  }
  return self_byte_size;
}

unsigned ShapeResult::StartIndexForResult() const {
  return !Rtl() ? runs_.front()->start_index_ : runs_.back()->start_index_;
}

unsigned ShapeResult::EndIndexForResult() const {
  return StartIndexForResult() + NumCharacters();
}

// If the position is outside of the result, returns the start or the end offset
// depends on the position.
unsigned ShapeResult::OffsetForPosition(float target_x,
                                        bool include_partial_glyphs) const {
  unsigned characters_so_far = 0;
  float current_x = 0;

  if (Rtl()) {
    if (target_x <= 0)
      return num_characters_;
    characters_so_far = num_characters_;
    for (unsigned i = 0; i < runs_.size(); ++i) {
      if (!runs_[i])
        continue;
      characters_so_far -= runs_[i]->num_characters_;
      float next_x = current_x + runs_[i]->width_;
      float offset_for_run = target_x - current_x;
      if (offset_for_run >= 0 && offset_for_run <= runs_[i]->width_) {
        // The x value in question is within this script run.
        const unsigned index = runs_[i]->CharacterIndexForXPosition(
            offset_for_run, include_partial_glyphs);
        return characters_so_far + index;
      }
      current_x = next_x;
    }
  } else {
    if (target_x <= 0)
      return 0;
    for (unsigned i = 0; i < runs_.size(); ++i) {
      if (!runs_[i])
        continue;
      float next_x = current_x + runs_[i]->width_;
      float offset_for_run = target_x - current_x;
      if (offset_for_run >= 0 && offset_for_run <= runs_[i]->width_) {
        const unsigned index = runs_[i]->CharacterIndexForXPosition(
            offset_for_run, include_partial_glyphs);
        return characters_so_far + index;
      }
      characters_so_far += runs_[i]->num_characters_;
      current_x = next_x;
    }
  }

  return characters_so_far;
}

float ShapeResult::PositionForOffset(unsigned absolute_offset) const {
  float x = 0;
  float offset_x = 0;

  // The absoluteOffset argument represents the offset for the entire
  // ShapeResult while offset is continuously updated to be relative to the
  // current run.
  unsigned offset = absolute_offset;

  if (Rtl()) {
    // Convert logical offsets to visual offsets, because results are in
    // logical order while runs are in visual order.
    x = width_;
    if (offset < NumCharacters())
      offset = NumCharacters() - offset - 1;
    x -= Width();
  }

  for (unsigned i = 0; i < runs_.size(); i++) {
    if (!runs_[i])
      continue;
    DCHECK_EQ(Rtl(), runs_[i]->Rtl());
    unsigned num_characters = runs_[i]->num_characters_;

    if (!offset_x && offset < num_characters) {
      offset_x = runs_[i]->XPositionForVisualOffset(offset, kAdjustToEnd) + x;
      break;
    }

    offset -= num_characters;
    x += runs_[i]->width_;
  }

  // The position in question might be just after the text.
  if (!offset_x && absolute_offset == NumCharacters())
    return Rtl() ? 0 : width_;

  return offset_x;
}

void ShapeResult::FallbackFonts(
    HashSet<const SimpleFontData*>* fallback) const {
  DCHECK(fallback);
  DCHECK(primary_font_);
  for (unsigned i = 0; i < runs_.size(); ++i) {
    if (runs_[i] && runs_[i]->font_data_ &&
        runs_[i]->font_data_ != primary_font_ &&
        !runs_[i]->font_data_->IsTextOrientationFallbackOf(
            primary_font_.Get())) {
      fallback->insert(runs_[i]->font_data_.Get());
    }
  }
}

template <typename TextContainerType>
void ShapeResult::ApplySpacing(ShapeResultSpacing<TextContainerType>& spacing,
                               const TextContainerType& text) {
  float offset_x, offset_y;
  float& offset = spacing.IsVerticalOffset() ? offset_y : offset_x;
  float total_space = 0;
  for (auto& run : runs_) {
    if (!run)
      continue;
    float total_space_for_run = 0;
    for (size_t i = 0; i < run->glyph_data_.size(); i++) {
      HarfBuzzRunGlyphData& glyph_data = run->glyph_data_[i];

      // Skip if it's not a grapheme cluster boundary.
      if (i + 1 < run->glyph_data_.size() &&
          glyph_data.character_index ==
              run->glyph_data_[i + 1].character_index) {
      } else {
        offset_x = offset_y = 0;
        float space = spacing.ComputeSpacing(
            text, run->start_index_ + glyph_data.character_index, offset);
        glyph_data.advance += space;
        total_space_for_run += space;
        glyph_data.offset.Expand(offset_x, offset_y);
      }
      has_vertical_offsets_ |= (glyph_data.offset.Height() != 0);
    }
    run->width_ += total_space_for_run;
    total_space += total_space_for_run;
  }
  width_ += total_space;
  // Glyph bounding box is in logical space.
  glyph_bounding_box_.SetWidth(glyph_bounding_box_.Width() + total_space);
}

void ShapeResult::ApplySpacing(ShapeResultSpacing<String>& spacing) {
  ApplySpacing(spacing, spacing.Text());
}

PassRefPtr<ShapeResult> ShapeResult::ApplySpacingToCopy(
    ShapeResultSpacing<TextRun>& spacing,
    const TextRun& run) const {
  RefPtr<ShapeResult> result = ShapeResult::Create(*this);
  result->ApplySpacing(spacing, run);
  return result;
}

static inline float HarfBuzzPositionToFloat(hb_position_t value) {
  return static_cast<float>(value) / (1 << 16);
}

// Computes glyph positions, sets advance and offset of each glyph to RunInfo.
//
// Also computes glyph bounding box of the run. In this function, glyph bounding
// box is in physical.
template <bool is_horizontal_run>
void ShapeResult::ComputeGlyphPositions(ShapeResult::RunInfo* run,
                                        unsigned start_glyph,
                                        unsigned num_glyphs,
                                        hb_buffer_t* harf_buzz_buffer,
                                        FloatRect* glyph_bounding_box) {
  DCHECK_EQ(is_horizontal_run, run->IsHorizontal());
  const SimpleFontData* current_font_data = run->font_data_.Get();
  const hb_glyph_info_t* glyph_infos =
      hb_buffer_get_glyph_infos(harf_buzz_buffer, 0);
  const hb_glyph_position_t* glyph_positions =
      hb_buffer_get_glyph_positions(harf_buzz_buffer, 0);
  const unsigned start_cluster =
      HB_DIRECTION_IS_FORWARD(hb_buffer_get_direction(harf_buzz_buffer))
          ? glyph_infos[start_glyph].cluster
          : glyph_infos[start_glyph + num_glyphs - 1].cluster;

  // Compute glyph_origin and glyph_bounding_box in physical, since both offsets
  // and boudning box of glyphs are in physical. It's the caller's
  // responsibility to convert the united physical bounds to logical.
  float total_advance = 0.0f;
  FloatPoint glyph_origin;
  if (is_horizontal_run)
    glyph_origin.SetX(width_);
  else
    glyph_origin.SetY(width_);
  bool has_vertical_offsets = !is_horizontal_run;

  // HarfBuzz returns result in visual order, no need to flip for RTL.
  for (unsigned i = 0; i < num_glyphs; ++i) {
    uint16_t glyph = glyph_infos[start_glyph + i].codepoint;
    hb_glyph_position_t pos = glyph_positions[start_glyph + i];

    // Offset is primarily used when painting glyphs. Keep it in physical.
    float offset_x = HarfBuzzPositionToFloat(pos.x_offset);
    float offset_y = -HarfBuzzPositionToFloat(pos.y_offset);

    // One out of x_advance and y_advance is zero, depending on
    // whether the buffer direction is horizontal or vertical.
    // Convert to float and negate to avoid integer-overflow for ULONG_MAX.
    float advance;
    if (is_horizontal_run)
      advance = HarfBuzzPositionToFloat(pos.x_advance);
    else
      advance = -HarfBuzzPositionToFloat(pos.y_advance);

    run->glyph_data_[i].character_index =
        glyph_infos[start_glyph + i].cluster - start_cluster;

    run->SetGlyphAndPositions(i, glyph, advance, offset_x, offset_y);
    total_advance += advance;
    has_vertical_offsets |= (offset_y != 0);

    // SetGlyphAndPositions() above sets to draw glyphs at |glyph_origin +
    // offset_{x,y}|. Move glyph_bounds to that point.
    // Then move the current point by |advance| from |glyph_origin|.
    // All positions in hb_glyph_position_t are relative to the current point.
    // https://behdad.github.io/harfbuzz/harfbuzz-Buffers.html#hb-glyph-position-t-struct
    FloatRect glyph_bounds = current_font_data->BoundsForGlyph(glyph);
    if (!glyph_bounds.IsEmpty()) {
      glyph_bounds.Move(glyph_origin.X() + offset_x,
                        glyph_origin.Y() + offset_y);
      glyph_bounding_box->Unite(glyph_bounds);
    }
    if (is_horizontal_run)
      glyph_origin.SetX(glyph_origin.X() + advance);
    else
      glyph_origin.SetY(glyph_origin.Y() + advance);
  }

  run->width_ = std::max(0.0f, total_advance);
  has_vertical_offsets_ |= has_vertical_offsets;
}

void ShapeResult::InsertRun(std::unique_ptr<ShapeResult::RunInfo> run_to_insert,
                            unsigned start_glyph,
                            unsigned num_glyphs,
                            hb_buffer_t* harf_buzz_buffer) {
  DCHECK_GT(num_glyphs, 0u);
  std::unique_ptr<ShapeResult::RunInfo> run(std::move(run_to_insert));
  DCHECK_EQ(num_glyphs, run->glyph_data_.size());

  FloatRect glyph_bounding_box;
  if (run->IsHorizontal()) {
    // Inserting a horizontal run into a horizontal or vertical result. In both
    // cases, no adjustments are needed because |glyph_bounding_box_| is in
    // logical coordinates and uses alphabetic baseline.
    ComputeGlyphPositions<true>(run.get(), start_glyph, num_glyphs,
                                harf_buzz_buffer, &glyph_bounding_box);
  } else {
    // Inserting a vertical run to a vertical result.
    ComputeGlyphPositions<false>(run.get(), start_glyph, num_glyphs,
                                 harf_buzz_buffer, &glyph_bounding_box);
    // Convert physical glyph_bounding_box to logical.
    glyph_bounding_box = glyph_bounding_box.TransposedRect();
    // The glyph bounding box of a vertical run uses ideographic baseline.
    // Adjust the box Y position because the bounding box of a ShapeResult uses
    // alphabetic baseline.
    // See diagrams of base lines at
    // https://drafts.csswg.org/css-writing-modes-3/#intro-baselines
    const FontMetrics& font_metrics = run->font_data_->GetFontMetrics();
    int baseline_adjust = font_metrics.Ascent(kIdeographicBaseline) -
                          font_metrics.Ascent(kAlphabeticBaseline);
    glyph_bounding_box.SetY(glyph_bounding_box.Y() + baseline_adjust);
  }
  glyph_bounding_box_.Unite(glyph_bounding_box);
  width_ += run->width_;
  num_glyphs_ += num_glyphs;
  DCHECK_GE(num_glyphs_, num_glyphs);

  // The runs are stored in result->m_runs in visual order. For LTR, we place
  // the run to be inserted before the next run with a bigger character
  // start index. For RTL, we place the run before the next run with a lower
  // character index. Otherwise, for both directions, at the end.
  if (HB_DIRECTION_IS_FORWARD(run->direction_)) {
    for (size_t pos = 0; pos < runs_.size(); ++pos) {
      if (runs_.at(pos)->start_index_ > run->start_index_) {
        runs_.insert(pos, std::move(run));
        break;
      }
    }
  } else {
    for (size_t pos = 0; pos < runs_.size(); ++pos) {
      if (runs_.at(pos)->start_index_ < run->start_index_) {
        runs_.insert(pos, std::move(run));
        break;
      }
    }
  }
  // If we didn't find an existing slot to place it, append.
  if (run)
    runs_.push_back(std::move(run));
}

void ShapeResult::CopyRange(unsigned start_offset,
                            unsigned end_offset,
                            ShapeResult* target) const {
  unsigned index = target->num_characters_;
  float total_width = 0;
  for (const auto& run : runs_) {
    unsigned run_start = (*run).start_index_;
    unsigned run_end = run_start + (*run).num_characters_;

    if (start_offset < run_end && end_offset > run_start) {
      unsigned start = start_offset > run_start ? start_offset - run_start : 0;
      unsigned end = std::min(end_offset, run_end) - run_start;
      DCHECK(end > start);

      auto sub_run = (*run).CreateSubRun(start, end);
      sub_run->start_index_ = index;
      total_width += sub_run->width_;
      index += sub_run->num_characters_;
      target->runs_.push_back(std::move(sub_run));
    }
  }

  // Compute new glyph bounding box.
  // If |start_offset| or |end_offset| are the start/end of |this|, use
  // |glyph_bounding_box_| from |this| for the side. Otherwise, we cannot
  // compute accurate glyph bounding box; approximate by assuming there are no
  // glyph overflow nor underflow.
  float left = target->width_;
  target->width_ += total_width;
  float right = target->width_;
  if (start_offset <= StartIndexForResult())
    left += glyph_bounding_box_.X();
  if (end_offset >= EndIndexForResult())
    right += glyph_bounding_box_.MaxX() - width_;
  FloatRect adjusted_box(left, glyph_bounding_box_.Y(),
                         std::max(right - left, 0.0f),
                         glyph_bounding_box_.Height());
  target->glyph_bounding_box_.UniteIfNonZero(adjusted_box);

  DCHECK_EQ(index - target->num_characters_,
            std::min(end_offset, EndIndexForResult()) -
                std::max(start_offset, StartIndexForResult()));
  target->num_characters_ = index;
}

PassRefPtr<ShapeResult> ShapeResult::CreateForTabulationCharacters(
    const Font* font,
    const TextRun& text_run,
    float position_offset,
    unsigned count) {
  const SimpleFontData* font_data = font->PrimaryFont();
  // Tab characters are always LTR or RTL, not TTB, even when
  // isVerticalAnyUpright().
  std::unique_ptr<ShapeResult::RunInfo> run = base::MakeUnique<RunInfo>(
      font_data, text_run.Rtl() ? HB_DIRECTION_RTL : HB_DIRECTION_LTR,
      HB_SCRIPT_COMMON, 0, count, count);
  float position = text_run.XPos() + position_offset;
  float start_position = position;
  for (unsigned i = 0; i < count; i++) {
    float advance = font->TabWidth(font_data, text_run.GetTabSize(), position);
    run->glyph_data_[i].character_index = i;
    run->SetGlyphAndPositions(i, font_data->SpaceGlyph(), advance, 0, 0);
    position += advance;
  }
  run->width_ = position - start_position;

  RefPtr<ShapeResult> result =
      ShapeResult::Create(font, count, text_run.Direction());
  result->width_ = run->width_;
  result->num_glyphs_ = count;
  DCHECK_EQ(result->num_glyphs_, count);  // no overflow
  result->has_vertical_offsets_ =
      font_data->PlatformData().IsVerticalAnyUpright();
  result->runs_.push_back(std::move(run));
  return result;
}

}  // namespace blink
