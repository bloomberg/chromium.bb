// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

#include <iterator>
#include "base/containers/adapters.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/shaping/glyph_bounds_accumulator.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"

namespace blink {

struct ShapeResultView::RunInfoPart {
  USING_FAST_MALLOC(RunInfoPart);

 public:
  RunInfoPart(scoped_refptr<ShapeResult::RunInfo> run,
              ShapeResult::RunInfo::GlyphDataRange range,
              unsigned start_index,
              unsigned offset,
              unsigned num_characters,
              float width)
      : run_(run),
        range_(range),
        start_index_(start_index),
        offset_(offset),
        num_characters_(num_characters),
        width_(width) {}

  using const_iterator = const HarfBuzzRunGlyphData*;
  const_iterator begin() const { return range_.begin; }
  const_iterator end() const { return range_.end; }
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }
  const HarfBuzzRunGlyphData& GlyphAt(unsigned index) const {
    return *(range_.begin + index);
  }

  bool Rtl() const { return run_->Rtl(); }
  bool IsHorizontal() const { return run_->IsHorizontal(); }
  unsigned NumCharacters() const { return num_characters_; }
  unsigned NumGlyphs() const { return range_.end - range_.begin; }
  float Width() const { return width_; }

  unsigned PreviousSafeToBreakOffset(unsigned offset) const;
  size_t GlyphToCharacterIndex(size_t i) const {
    return run_->GlyphToCharacterIndex(i);
  }

  scoped_refptr<ShapeResult::RunInfo> run_;
  ShapeResult::RunInfo::GlyphDataRange range_;

  // Start index for partial run, adjusted to ensure that runs are continuous.
  unsigned start_index_;

  // Offset relative to start index for the original run.
  unsigned offset_;

  unsigned num_characters_;
  float width_;
};

unsigned ShapeResultView::RunInfoPart::PreviousSafeToBreakOffset(
    unsigned offset) const {
  if (offset >= NumCharacters())
    return NumCharacters();
  if (!Rtl()) {
    for (const auto& glyph : base::Reversed(*this)) {
      if (glyph.safe_to_break_before && glyph.character_index <= offset)
        return glyph.character_index;
    }
  } else {
    for (const auto& glyph : *this) {
      if (glyph.safe_to_break_before && glyph.character_index <= offset)
        return glyph.character_index;
    }
  }

  // Next safe break is at the start of the run.
  return 0;
}

ShapeResultView::ShapeResultView(const ShapeResult* other)
    : primary_font_(other->primary_font_),
      start_index_(0),
      num_characters_(0),
      num_glyphs_(0),
      direction_(other->direction_),
      has_vertical_offsets_(other->has_vertical_offsets_),
      width_(0) {}

ShapeResultView::~ShapeResultView() = default;

scoped_refptr<ShapeResult> ShapeResultView::CreateShapeResult() const {
  ShapeResult* new_result =
      new ShapeResult(primary_font_, num_characters_, Direction());
  new_result->runs_.ReserveCapacity(parts_.size());
  for (const auto& part : parts_) {
    auto new_run = ShapeResult::RunInfo::Create(
        part->run_->font_data_.get(), part->run_->direction_,
        part->run_->canvas_rotation_, part->run_->script_, part->start_index_,
        part->NumGlyphs(), part->num_characters_);
    std::copy(part->range_.begin, part->range_.end,
              new_run->glyph_data_.begin());
    for (HarfBuzzRunGlyphData& glyph_data : new_run->glyph_data_) {
      glyph_data.character_index -= part->offset_;
    }

    new_run->start_index_ += char_index_offset_;
    new_run->width_ = part->width_;
    new_run->num_characters_ = part->num_characters_;
    new_result->runs_.push_back(std::move(new_run));
  }

  new_result->start_index_ = start_index_ + char_index_offset_;
  new_result->num_glyphs_ = num_glyphs_;
  new_result->has_vertical_offsets_ = has_vertical_offsets_;
  new_result->width_ = width_;
  new_result->glyph_bounding_box_ = glyph_bounding_box_;

  return base::AdoptRef(new_result);
}

void ShapeResultView::CreateViewsForResult(const ShapeResult* other,
                                           unsigned start_index,
                                           unsigned end_index) {
  bool first_result = num_characters_ == 0;
  for (const auto& run : other->runs_) {
    if (!run)
      continue;
    unsigned part_start = run->start_index_;
    unsigned run_end = part_start + run->num_characters_;
    if (start_index < run_end && end_index > part_start) {
      ShapeResult::RunInfo::GlyphDataRange range;

      unsigned adjusted_start =
          start_index > part_start ? start_index - part_start : 0;
      unsigned adjusted_end = std::min(end_index, run_end) - part_start;
      DCHECK(adjusted_end > adjusted_start);
      unsigned part_characters = adjusted_end - adjusted_start;
      float part_width;

      // Avoid O(log n) find operation if the entire run is in range.
      if (part_start >= start_index && run_end <= end_index) {
        range = {run->glyph_data_.begin(), run->glyph_data_.end()};
        part_width = run->width_;
      } else {
        range = run->FindGlyphDataRange(adjusted_start, adjusted_end);
        part_width = 0;
        for (auto* glyph = range.begin; glyph != range.end; glyph++)
          part_width += glyph->advance;
      }

      // Adjust start_index for runs to be continuous.
      unsigned part_start_index;
      unsigned part_offset;
      if (!run->Rtl()) {  // Left-to-right
        part_start_index = start_index_ + num_characters_;
        part_offset = adjusted_start;
      } else {  // Right-to-left
        part_start_index = run->start_index_ + adjusted_start;
        part_offset = adjusted_start;
      }

      parts_.push_back(std::make_unique<RunInfoPart>(
          run, range, part_start_index, part_offset, part_characters,
          part_width));

      num_characters_ += part_characters;
      num_glyphs_ += range.end - range.begin;
      width_ += part_width;
    }
  }

  if (first_result || Rtl())
    start_index_ = ComputeStartIndex();
}

scoped_refptr<ShapeResultView> ShapeResultView::Create(const Segment* segments,
                                                       size_t segment_count) {
  ShapeResultView* out = new ShapeResultView(segments[0].result);
  out->AddSegments(segments, segment_count);
  return base::AdoptRef(out);
}

scoped_refptr<ShapeResultView> ShapeResultView::Create(
    const ShapeResult* result,
    unsigned start_index,
    unsigned end_index) {
  Segment segment = {result, start_index, end_index};
  return Create(&segment, 1);
}

scoped_refptr<ShapeResultView> ShapeResultView::Create(
    const ShapeResult* result) {
  // This specialization is an optimization to allow the bounding box to be
  // re-used.
  ShapeResultView* out = new ShapeResultView(result);
  out->char_index_offset_ = out->Rtl() ? 0 : result->StartIndex();
  out->CreateViewsForResult(result, 0, std::numeric_limits<unsigned>::max());
  out->has_vertical_offsets_ = result->has_vertical_offsets_;
  out->glyph_bounding_box_ = result->glyph_bounding_box_;
  return base::AdoptRef(out);
}

void ShapeResultView::AddSegments(const Segment* segments,
                                  size_t segment_count) {
  // This method assumes that no parts have been added yet.
  DCHECK_EQ(parts_.size(), 0u);

  // Segments are in logical order, runs and parts are in visual order. Iterate
  // over segments back-to-front for RTL.
  DCHECK_GT(segment_count, 0u);
  unsigned last_segment_index = segment_count - 1;

  // Compute start index offset for the overall run. This is added to the start
  // index of each glyph to ensure consistency with ShapeResult::SubRange
  if (!Rtl()) {  // Left-to-right
    char_index_offset_ =
        std::max(segments[0].result->StartIndex(), segments[0].start_index);
  } else {  // Right to left
    char_index_offset_ = 0;
  }

  for (unsigned i = 0; i < segment_count; i++) {
    const Segment& segment = segments[Rtl() ? last_segment_index - i : i];
    DCHECK_EQ(segment.result->Direction(), Direction());
    CreateViewsForResult(segment.result, segment.start_index,
                         segment.end_index);
    has_vertical_offsets_ |= segment.result->has_vertical_offsets_;
  }

  float origin = 0;
  for (const auto& part : parts_) {
    if (part->IsHorizontal())
      ComputeBoundsForPart<true>(*part, origin);
    else
      ComputeBoundsForPart<false>(*part, origin);
    origin += part->width_;
  }
}

template <bool is_horizontal_run>
void ShapeResultView::ComputeBoundsForPart(const RunInfoPart& part,
                                           float origin) {
  GlyphBoundsAccumulator bounds(origin);
  const auto& run = part.run_;
  const SimpleFontData* font_data = run->font_data_.get();
  for (const auto& glyph_data : part) {
    FloatRect glyph_bounds = glyph_data.HasValidGlyphBounds()
                                 ? FloatRect(glyph_data.GlyphBoundsBefore(), 0,
                                             glyph_data.GlyphBoundsAfter(), 0)
                                 : font_data->BoundsForGlyph(glyph_data.glyph);

    bounds.Unite<is_horizontal_run>(glyph_data, glyph_bounds);
    bounds.origin += glyph_data.advance;
  }
  if (!is_horizontal_run)
    bounds.ConvertVerticalRunToLogical(font_data->GetFontMetrics());
  glyph_bounding_box_.Unite(bounds.bounds);
}

unsigned ShapeResultView::ComputeStartIndex() const {
  if (UNLIKELY(parts_.IsEmpty()))
    return 0;
  const RunInfoPart& first_part = *parts_.front();
  if (!Rtl())  // Left-to-right.
    return first_part.start_index_;
  // Right-to-left.
  unsigned end_index = first_part.start_index_ + first_part.num_characters_;
  return end_index - num_characters_;
}

unsigned ShapeResultView::PreviousSafeToBreakOffset(unsigned index) const {
  for (auto it = parts_.rbegin(); it != parts_.rend(); ++it) {
    const auto& part = *it;
    if (!part)
      continue;

    unsigned run_start = part->start_index_;
    if (index >= run_start) {
      unsigned offset = index - run_start;
      if (offset <= part->num_characters_) {
        return part->PreviousSafeToBreakOffset(offset) + run_start;
      }
      if (!Rtl()) {
        return run_start + part->num_characters_;
      }
    } else if (Rtl()) {
      if (it == parts_.rbegin())
        return part->start_index_;
      const auto& previous_run = *--it;
      return previous_run->start_index_ + previous_run->num_characters_;
    }
  }

  return StartIndex();
}

void ShapeResultView::GetRunFontData(
    Vector<ShapeResult::RunFontData>* font_data) const {
  for (const auto& part : parts_) {
    font_data->push_back(ShapeResult::RunFontData(
        {part->run_->font_data_.get(), part->end() - part->begin()}));
  }
}

void ShapeResultView::FallbackFonts(
    HashSet<const SimpleFontData*>* fallback) const {
  DCHECK(fallback);
  DCHECK(primary_font_);
  for (const auto& part : parts_) {
    if (part->run_->font_data_ && part->run_->font_data_ != primary_font_) {
      fallback->insert(part->run_->font_data_.get());
    }
  }
}

float ShapeResultView::ForEachGlyph(float initial_advance,
                                    GlyphCallback glyph_callback,
                                    void* context) const {
  auto total_advance = initial_advance;
  for (const auto& part : parts_) {
    const auto& run = part->run_;
    bool is_horizontal = HB_DIRECTION_IS_HORIZONTAL(run->direction_);
    const SimpleFontData* font_data = run->font_data_.get();
    for (const auto& glyph_data : *part) {
      unsigned character_index = glyph_data.character_index +
                                 part->start_index_ + char_index_offset_ -
                                 part->offset_;
      glyph_callback(context, character_index, glyph_data.glyph,
                     glyph_data.offset, total_advance, is_horizontal,
                     run->canvas_rotation_, font_data);
      total_advance += glyph_data.advance;
    }
  }

  return total_advance;
}

float ShapeResultView::ForEachGlyph(float initial_advance,
                                    unsigned from,
                                    unsigned to,
                                    unsigned index_offset,
                                    GlyphCallback glyph_callback,
                                    void* context) const {
  auto total_advance = initial_advance;

  for (const auto& part : parts_) {
    const auto& run = part->run_;
    bool is_horizontal = HB_DIRECTION_IS_HORIZONTAL(run->direction_);
    const SimpleFontData* font_data = run->font_data_.get();

    if (!run->Rtl()) {  // Left-to-right
      for (const auto& glyph_data : *part) {
        unsigned character_index = glyph_data.character_index +
                                   part->start_index_ + char_index_offset_ -
                                   part->offset_;
        if (character_index >= to)
          break;
        if (character_index >= from) {
          glyph_callback(context, character_index, glyph_data.glyph,
                         glyph_data.offset, total_advance, is_horizontal,
                         run->canvas_rotation_, font_data);
        }
        total_advance += glyph_data.advance;
      }

    } else {  // Right-to-left
      for (const auto& glyph_data : *part) {
        unsigned character_index = glyph_data.character_index +
                                   part->start_index_ + char_index_offset_ -
                                   part->offset_;
        if (character_index < from)
          break;
        if (character_index < to) {
          glyph_callback(context, character_index, glyph_data.glyph,
                         glyph_data.offset, total_advance, is_horizontal,
                         run->canvas_rotation_, font_data);
        }
        total_advance += glyph_data.advance;
      }
    }
  }
  return total_advance;
}

float ShapeResultView::ForEachGraphemeClusters(const StringView& text,
                                               float initial_advance,
                                               unsigned from,
                                               unsigned to,
                                               unsigned index_offset,
                                               GraphemeClusterCallback callback,
                                               void* context) const {
  unsigned run_offset = index_offset;
  float advance_so_far = initial_advance;

  for (const auto& part : parts_) {
    const auto& run = part->run_;
    unsigned graphemes_in_cluster = 1;
    float cluster_advance = 0;
    bool rtl = Direction() == TextDirection::kRtl;

    // A "cluster" in this context means a cluster as it is used by HarfBuzz:
    // The minimal group of characters and corresponding glyphs, that cannot be
    // broken down further from a text shaping point of view.  A cluster can
    // contain multiple glyphs and grapheme clusters, with mutually overlapping
    // boundaries.
    uint16_t cluster_start = static_cast<uint16_t>(
        rtl ? run->start_index_ + run->num_characters_ + run_offset
            : run->GlyphToCharacterIndex(0) + run_offset);

    const unsigned num_glyphs = part->NumGlyphs();
    for (unsigned i = 0; i < num_glyphs; ++i) {
      const HarfBuzzRunGlyphData& glyph_data = part->GlyphAt(i);
      uint16_t current_character_index = glyph_data.character_index +
                                         part->start_index_ +
                                         char_index_offset_ - part->offset_;

      bool is_run_end = (i + 1 == num_glyphs);
      bool is_cluster_end =
          is_run_end || (run->GlyphToCharacterIndex(i + 1) + run_offset !=
                         current_character_index);

      if ((rtl && current_character_index >= to) ||
          (!rtl && current_character_index < from)) {
        advance_so_far += glyph_data.advance;
        rtl ? --cluster_start : ++cluster_start;
        continue;
      }

      cluster_advance += glyph_data.advance;

      if (text.Is8Bit()) {
        callback(context, current_character_index, advance_so_far, 1,
                 glyph_data.advance, run->canvas_rotation_);

        advance_so_far += glyph_data.advance;
      } else if (is_cluster_end) {
        uint16_t cluster_end;
        if (rtl) {
          cluster_end = current_character_index;
        } else {
          cluster_end = static_cast<uint16_t>(
              is_run_end ? run->start_index_ + run->num_characters_ + run_offset
                         : run->GlyphToCharacterIndex(i + 1) + run_offset);
        }
        graphemes_in_cluster = ShapeResult::CountGraphemesInCluster(
            text.Characters16(), text.length(), cluster_start, cluster_end);
        if (!graphemes_in_cluster || !cluster_advance)
          continue;

        callback(context, current_character_index, advance_so_far,
                 graphemes_in_cluster, cluster_advance, run->canvas_rotation_);
        advance_so_far += cluster_advance;

        cluster_start = cluster_end;
        cluster_advance = 0;
      }
    }
  }
  return advance_so_far;
}

}  // namespace blink
