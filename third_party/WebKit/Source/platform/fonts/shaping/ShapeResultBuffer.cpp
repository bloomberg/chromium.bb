// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultBuffer.h"

#include "platform/fonts/CharacterRange.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/shaping/ShapeResultBloberizer.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/text/Character.h"
#include "platform/text/TextBreakIterator.h"
#include "platform/text/TextDirection.h"

namespace blink {

namespace {

inline bool IsSkipInkException(const ShapeResultBloberizer& bloberizer,
                               const TextRun& run,
                               unsigned character_index) {
  // We want to skip descenders in general, but it is undesirable renderings for
  // CJK characters.
  return bloberizer.GetType() == ShapeResultBloberizer::Type::kTextIntercepts &&
         !run.Is8Bit() &&
         Character::IsCJKIdeographOrSymbol(run.CodepointAt(character_index));
}

inline void AddGlyphToBloberizer(ShapeResultBloberizer& bloberizer,
                                 float advance,
                                 hb_direction_t direction,
                                 const SimpleFontData* font_data,
                                 const HarfBuzzRunGlyphData& glyph_data,
                                 const TextRun& run,
                                 unsigned character_index) {
  FloatPoint start_offset = HB_DIRECTION_IS_HORIZONTAL(direction)
                                ? FloatPoint(advance, 0)
                                : FloatPoint(0, advance);
  if (!IsSkipInkException(bloberizer, run, character_index))
    bloberizer.Add(glyph_data.glyph, font_data,
                   start_offset + glyph_data.offset);
}

inline void AddEmphasisMark(ShapeResultBloberizer& bloberizer,
                            const GlyphData& emphasis_data,
                            FloatPoint glyph_center,
                            float mid_glyph_offset) {
  const SimpleFontData* emphasis_font_data = emphasis_data.font_data;
  DCHECK(emphasis_font_data);

  bool is_vertical =
      emphasis_font_data->PlatformData().IsVerticalAnyUpright() &&
      emphasis_font_data->VerticalData();

  if (!is_vertical) {
    bloberizer.Add(emphasis_data.glyph, emphasis_font_data,
                   mid_glyph_offset - glyph_center.X());
  } else {
    bloberizer.Add(
        emphasis_data.glyph, emphasis_font_data,
        FloatPoint(-glyph_center.X(), mid_glyph_offset - glyph_center.Y()));
  }
}

inline unsigned CountGraphemesInCluster(const UChar* str,
                                        unsigned str_length,
                                        uint16_t start_index,
                                        uint16_t end_index) {
  if (start_index > end_index) {
    uint16_t temp_index = start_index;
    start_index = end_index;
    end_index = temp_index;
  }
  uint16_t length = end_index - start_index;
  DCHECK_LE(static_cast<unsigned>(start_index + length), str_length);
  TextBreakIterator* cursor_pos_iterator =
      CursorMovementIterator(&str[start_index], length);

  int cursor_pos = cursor_pos_iterator->current();
  int num_graphemes = -1;
  while (0 <= cursor_pos) {
    cursor_pos = cursor_pos_iterator->next();
    num_graphemes++;
  }
  return std::max(0, num_graphemes);
}

}  // anonymous namespace

float ShapeResultBuffer::FillGlyphsForResult(ShapeResultBloberizer& bloberizer,
                                             const ShapeResult& result,
                                             const TextRunPaintInfo& run_info,
                                             float initial_advance,
                                             unsigned run_offset) {
  auto total_advance = initial_advance;

  for (const auto& run : result.runs_) {
    total_advance = run->ForEachGlyphInRange(
        total_advance, run_info.from, run_info.to, run_offset,
        [&](const HarfBuzzRunGlyphData& glyph_data, float total_advance,
            uint16_t character_index) -> bool {

          AddGlyphToBloberizer(bloberizer, total_advance, run->direction_,
                               run->font_data_.Get(), glyph_data, run_info.run,
                               character_index);
          return true;
        });
  }

  return total_advance;
}

float ShapeResultBuffer::FillTextEmphasisGlyphsForRun(
    ShapeResultBloberizer& bloberizer,
    const ShapeResult::RunInfo* run,
    const TextRunPaintInfo& run_info,
    const GlyphData& emphasis_data,
    float initial_advance,
    unsigned run_offset) {
  if (!run)
    return 0;

  unsigned graphemes_in_cluster = 1;
  float cluster_advance = 0;

  FloatPoint glyph_center =
      emphasis_data.font_data->BoundsForGlyph(emphasis_data.glyph).Center();

  const auto& text_run = run_info.run;
  const auto from = run_info.from;
  const auto to = run_info.to;

  TextDirection direction = text_run.Direction();

  // A "cluster" in this context means a cluster as it is used by HarfBuzz:
  // The minimal group of characters and corresponding glyphs, that cannot be
  // broken down further from a text shaping point of view.  A cluster can
  // contain multiple glyphs and grapheme clusters, with mutually overlapping
  // boundaries. Below we count grapheme clusters per HarfBuzz clusters, then
  // linearly split the sum of corresponding glyph advances by the number of
  // grapheme clusters in order to find positions for emphasis mark drawing.
  uint16_t cluster_start = static_cast<uint16_t>(
      direction == TextDirection::kRtl
          ? run->start_index_ + run->num_characters_ + run_offset
          : run->GlyphToCharacterIndex(0) + run_offset);

  float advance_so_far = initial_advance;
  const unsigned num_glyphs = run->glyph_data_.size();
  for (unsigned i = 0; i < num_glyphs; ++i) {
    const HarfBuzzRunGlyphData& glyph_data = run->glyph_data_[i];
    uint16_t current_character_index =
        run->start_index_ + glyph_data.character_index + run_offset;
    bool is_run_end = (i + 1 == num_glyphs);
    bool is_cluster_end =
        is_run_end || (run->GlyphToCharacterIndex(i + 1) + run_offset !=
                       current_character_index);

    if ((direction == TextDirection::kRtl && current_character_index >= to) ||
        (direction != TextDirection::kRtl && current_character_index < from)) {
      advance_so_far += glyph_data.advance;
      direction == TextDirection::kRtl ? --cluster_start : ++cluster_start;
      continue;
    }

    cluster_advance += glyph_data.advance;

    if (text_run.Is8Bit()) {
      float glyph_advance_x = glyph_data.advance;
      if (Character::CanReceiveTextEmphasis(
              text_run[current_character_index])) {
        AddEmphasisMark(bloberizer, emphasis_data, glyph_center,
                        advance_so_far + glyph_advance_x / 2);
      }
      advance_so_far += glyph_advance_x;
    } else if (is_cluster_end) {
      uint16_t cluster_end;
      if (direction == TextDirection::kRtl)
        cluster_end = current_character_index;
      else
        cluster_end = static_cast<uint16_t>(
            is_run_end ? run->start_index_ + run->num_characters_ + run_offset
                       : run->GlyphToCharacterIndex(i + 1) + run_offset);

      graphemes_in_cluster = CountGraphemesInCluster(
          text_run.Characters16(), text_run.CharactersLength(), cluster_start,
          cluster_end);
      if (!graphemes_in_cluster || !cluster_advance)
        continue;

      float glyph_advance_x = cluster_advance / graphemes_in_cluster;
      for (unsigned j = 0; j < graphemes_in_cluster; ++j) {
        // Do not put emphasis marks on space, separator, and control
        // characters.
        if (Character::CanReceiveTextEmphasis(
                text_run[current_character_index]))
          AddEmphasisMark(bloberizer, emphasis_data, glyph_center,
                          advance_so_far + glyph_advance_x / 2);
        advance_so_far += glyph_advance_x;
      }
      cluster_start = cluster_end;
      cluster_advance = 0;
    }
  }
  return advance_so_far - initial_advance;
}

float ShapeResultBuffer::FillFastHorizontalGlyphs(
    const TextRun& text_run,
    ShapeResultBloberizer& bloberizer) const {
  DCHECK(!HasVerticalOffsets());
  DCHECK_NE(bloberizer.GetType(), ShapeResultBloberizer::Type::kTextIntercepts);

  float advance = 0;

  for (unsigned i = 0; i < results_.size(); ++i) {
    const auto& word_result = IsLeftToRightDirection(text_run.Direction())
                                  ? results_[i]
                                  : results_[results_.size() - 1 - i];
    DCHECK(!word_result->HasVerticalOffsets());

    for (const auto& run : word_result->runs_) {
      DCHECK(run);
      DCHECK(HB_DIRECTION_IS_HORIZONTAL(run->direction_));

      advance = run->ForEachGlyph(
          advance,
          [&](const HarfBuzzRunGlyphData& glyph_data,
              float total_advance) -> bool {
            DCHECK(!glyph_data.offset.Height());
            bloberizer.Add(glyph_data.glyph, run->font_data_.Get(),
                           total_advance + glyph_data.offset.Width());
            return true;
          });
    }
  }

  return advance;
}

float ShapeResultBuffer::FillGlyphs(const TextRunPaintInfo& run_info,
                                    ShapeResultBloberizer& bloberizer) const {
  // Fast path: full run with no vertical offsets, no text intercepts.
  if (!run_info.from && run_info.to == run_info.run.length() &&
      !HasVerticalOffsets() &&
      bloberizer.GetType() != ShapeResultBloberizer::Type::kTextIntercepts) {
    return FillFastHorizontalGlyphs(run_info.run, bloberizer);
  }

  float advance = 0;

  if (run_info.run.Rtl()) {
    unsigned word_offset = run_info.run.length();
    for (unsigned j = 0; j < results_.size(); j++) {
      unsigned resolved_index = results_.size() - 1 - j;
      const RefPtr<const ShapeResult>& word_result = results_[resolved_index];
      word_offset -= word_result->NumCharacters();
      advance = FillGlyphsForResult(bloberizer, *word_result, run_info, advance,
                                    word_offset);
    }
  } else {
    unsigned word_offset = 0;
    for (const auto& word_result : results_) {
      advance = FillGlyphsForResult(bloberizer, *word_result, run_info, advance,
                                    word_offset);
      word_offset += word_result->NumCharacters();
    }
  }

  return advance;
}

void ShapeResultBuffer::FillTextEmphasisGlyphs(
    const TextRunPaintInfo& run_info,
    const GlyphData& emphasis_data,
    ShapeResultBloberizer& bloberizer) const {
  float advance = 0;
  unsigned word_offset = run_info.run.Rtl() ? run_info.run.length() : 0;

  for (unsigned j = 0; j < results_.size(); j++) {
    unsigned resolved_index = run_info.run.Rtl() ? results_.size() - 1 - j : j;
    const RefPtr<const ShapeResult>& word_result = results_[resolved_index];
    for (unsigned i = 0; i < word_result->runs_.size(); i++) {
      unsigned resolved_offset =
          word_offset - (run_info.run.Rtl() ? word_result->NumCharacters() : 0);
      advance += FillTextEmphasisGlyphsForRun(
          bloberizer, word_result->runs_[i].get(), run_info, emphasis_data,
          advance, resolved_offset);
    }
    word_offset += word_result->NumCharacters() * (run_info.run.Rtl() ? -1 : 1);
  }
}

// TODO(eae): This is a bit of a hack to allow reuse of the implementation
// for both ShapeResultBuffer and single ShapeResult use cases. Ideally the
// logic should move into ShapeResult itself and then the ShapeResultBuffer
// implementation may wrap that.
CharacterRange ShapeResultBuffer::GetCharacterRange(
    RefPtr<const ShapeResult> result,
    TextDirection direction,
    float total_width,
    unsigned from,
    unsigned to) {
  Vector<RefPtr<const ShapeResult>, 64> results;
  results.push_back(result);
  return GetCharacterRangeInternal(results, direction, total_width, from, to);
}

CharacterRange ShapeResultBuffer::GetCharacterRangeInternal(
    const Vector<RefPtr<const ShapeResult>, 64>& results,
    TextDirection direction,
    float total_width,
    unsigned absolute_from,
    unsigned absolute_to) {
  float current_x = 0;
  float from_x = 0;
  float to_x = 0;
  bool found_from_x = false;
  bool found_to_x = false;

  if (direction == TextDirection::kRtl)
    current_x = total_width;

  // The absoluteFrom and absoluteTo arguments represent the start/end offset
  // for the entire run, from/to are continuously updated to be relative to
  // the current word (ShapeResult instance).
  int from = absolute_from;
  int to = absolute_to;

  unsigned total_num_characters = 0;
  for (unsigned j = 0; j < results.size(); j++) {
    const RefPtr<const ShapeResult> result = results[j];
    if (direction == TextDirection::kRtl) {
      // Convert logical offsets to visual offsets, because results are in
      // logical order while runs are in visual order.
      if (!found_from_x && from >= 0 &&
          static_cast<unsigned>(from) < result->NumCharacters())
        from = result->NumCharacters() - from - 1;
      if (!found_to_x && to >= 0 &&
          static_cast<unsigned>(to) < result->NumCharacters())
        to = result->NumCharacters() - to - 1;
      current_x -= result->Width();
    }
    for (unsigned i = 0; i < result->runs_.size(); i++) {
      if (!result->runs_[i])
        continue;
      DCHECK_EQ(direction == TextDirection::kRtl, result->runs_[i]->Rtl());
      int num_characters = result->runs_[i]->num_characters_;
      if (!found_from_x && from >= 0 && from < num_characters) {
        from_x =
            result->runs_[i]->XPositionForVisualOffset(from, kAdjustToStart) +
            current_x;
        found_from_x = true;
      } else {
        from -= num_characters;
      }

      if (!found_to_x && to >= 0 && to < num_characters) {
        to_x = result->runs_[i]->XPositionForVisualOffset(to, kAdjustToEnd) +
               current_x;
        found_to_x = true;
      } else {
        to -= num_characters;
      }

      if (found_from_x && found_to_x)
        break;
      current_x += result->runs_[i]->width_;
    }
    if (direction == TextDirection::kRtl)
      current_x -= result->Width();
    total_num_characters += result->NumCharacters();
  }

  // The position in question might be just after the text.
  if (!found_from_x && absolute_from == total_num_characters) {
    from_x = direction == TextDirection::kRtl ? 0 : total_width;
    found_from_x = true;
  }
  if (!found_to_x && absolute_to == total_num_characters) {
    to_x = direction == TextDirection::kRtl ? 0 : total_width;
    found_to_x = true;
  }
  if (!found_from_x)
    from_x = 0;
  if (!found_to_x)
    to_x = direction == TextDirection::kRtl ? 0 : total_width;

  // None of our runs is part of the selection, possibly invalid arguments.
  if (!found_to_x && !found_from_x)
    from_x = to_x = 0;
  if (from_x < to_x)
    return CharacterRange(from_x, to_x);
  return CharacterRange(to_x, from_x);
}

CharacterRange ShapeResultBuffer::GetCharacterRange(TextDirection direction,
                                                    float total_width,
                                                    unsigned from,
                                                    unsigned to) const {
  return GetCharacterRangeInternal(results_, direction, total_width, from, to);
}

void ShapeResultBuffer::AddRunInfoRanges(const ShapeResult::RunInfo& run_info,
                                         float offset,
                                         Vector<CharacterRange>& ranges) {
  Vector<float> character_widths(run_info.num_characters_);
  for (const auto& glyph : run_info.glyph_data_)
    character_widths[glyph.character_index] += glyph.advance;

  for (unsigned character_index = 0; character_index < run_info.num_characters_;
       character_index++) {
    float start = offset;
    offset += character_widths[character_index];
    float end = offset;

    // To match getCharacterRange we flip ranges to ensure start <= end.
    if (end < start)
      ranges.push_back(CharacterRange(end, start));
    else
      ranges.push_back(CharacterRange(start, end));
  }
}

Vector<CharacterRange> ShapeResultBuffer::IndividualCharacterRanges(
    TextDirection direction,
    float total_width) const {
  Vector<CharacterRange> ranges;
  float current_x = direction == TextDirection::kRtl ? total_width : 0;
  for (const RefPtr<const ShapeResult> result : results_) {
    if (direction == TextDirection::kRtl)
      current_x -= result->Width();
    unsigned run_count = result->runs_.size();
    for (unsigned index = 0; index < run_count; index++) {
      unsigned run_index =
          direction == TextDirection::kRtl ? run_count - 1 - index : index;
      AddRunInfoRanges(*result->runs_[run_index], current_x, ranges);
      current_x += result->runs_[run_index]->width_;
    }
    if (direction == TextDirection::kRtl)
      current_x -= result->Width();
  }
  return ranges;
}

int ShapeResultBuffer::OffsetForPosition(const TextRun& run,
                                         float target_x,
                                         bool include_partial_glyphs) const {
  unsigned total_offset;
  if (run.Rtl()) {
    total_offset = run.length();
    for (unsigned i = results_.size(); i; --i) {
      const RefPtr<const ShapeResult>& word_result = results_[i - 1];
      if (!word_result)
        continue;
      total_offset -= word_result->NumCharacters();
      if (target_x >= 0 && target_x <= word_result->Width()) {
        int offset_for_word =
            word_result->OffsetForPosition(target_x, include_partial_glyphs);
        return total_offset + offset_for_word;
      }
      target_x -= word_result->Width();
    }
  } else {
    total_offset = 0;
    for (const auto& word_result : results_) {
      if (!word_result)
        continue;
      int offset_for_word =
          word_result->OffsetForPosition(target_x, include_partial_glyphs);
      DCHECK_GE(offset_for_word, 0);
      total_offset += offset_for_word;
      if (target_x >= 0 && target_x <= word_result->Width())
        return total_offset;
      target_x -= word_result->Width();
    }
  }
  return total_offset;
}

Vector<ShapeResultBuffer::RunFontData> ShapeResultBuffer::GetRunFontData()
    const {
  Vector<RunFontData> font_data;

  for (const auto& result : results_) {
    for (const auto& run : result->runs_) {
      font_data.push_back(
          RunFontData({run->font_data_.Get(), run->glyph_data_.size()}));
    }
  }
  return font_data;
}

GlyphData ShapeResultBuffer::EmphasisMarkGlyphData(
    const FontDescription& font_description) const {
  for (const auto& result : results_) {
    for (const auto& run : result->runs_) {
      DCHECK(run->font_data_);
      if (run->glyph_data_.IsEmpty())
        continue;

      return GlyphData(
          run->glyph_data_[0].glyph,
          run->font_data_->EmphasisMarkFontData(font_description).Get());
    }
  }

  return GlyphData();
}

}  // namespace blink
