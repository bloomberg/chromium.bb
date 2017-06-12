// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapingLineBreaker.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"
#include "platform/fonts/shaping/ShapeResult.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"
#include "platform/text/TextBreakIterator.h"

namespace blink {

ShapingLineBreaker::ShapingLineBreaker(
    const HarfBuzzShaper* shaper,
    const Font* font,
    const ShapeResult* result,
    const LazyLineBreakIterator* break_iterator)
    : shaper_(shaper),
      font_(font),
      result_(result),
      break_iterator_(break_iterator) {
  text_ = String(shaper->GetText(), shaper->TextLength());
}

namespace {

unsigned PreviousSafeToBreakAfter(const UChar* text,
                                  unsigned start,
                                  unsigned offset) {
  // TODO(eae): This is quite incorrect. It should be changed to use the
  // HarfBuzzHarfBuzz safe to break info when available.
  for (; offset > start; offset--) {
    if (text[offset - 1] == kSpaceCharacter)
      break;
  }
  return offset;
}

unsigned NextSafeToBreakBefore(const UChar* text,
                               unsigned end,
                               unsigned offset) {
  // TODO(eae): This is quite incorrect. It should be changed to use the
  // HarfBuzzHarfBuzz safe to break info when available.
  for (; offset < end; offset++) {
    if (text[offset] == kSpaceCharacter)
      break;
  }
  return offset;
}

// ShapingLineBreaker computes using visual positions. This function flips
// logical advance to visual, or vice versa.
LayoutUnit FlipRtl(LayoutUnit value, TextDirection direction) {
  return direction != TextDirection::kRtl ? value : -value;
}

// Snaps a visual position to the line start direction.
LayoutUnit SnapStart(float value, TextDirection direction) {
  return direction != TextDirection::kRtl ? LayoutUnit::FromFloatFloor(value)
                                          : LayoutUnit::FromFloatCeil(value);
}

// Snaps a visual position to the line end direction.
LayoutUnit SnapEnd(float value, TextDirection direction) {
  return direction != TextDirection::kRtl ? LayoutUnit::FromFloatCeil(value)
                                          : LayoutUnit::FromFloatFloor(value);
}

}  // namespace

// Shapes a line of text by finding a valid and appropriate break opportunity
// based on the shaping results for the entire paragraph. Re-shapes the start
// and end of the line as needed.
//
// Definitions:
//   Candidate break opportunity: Ideal point to break, disregarding line
//                                breaking rules. May be in the middle of a word
//                                or inside a ligature.
//    Valid break opportunity:    A point where a break is allowed according to
//                                the relevant breaking rules.
//    Safe-to-break:              A point where a break may occur without
//                                affecting the rendering or metrics of the
//                                text. Breaking at safe-to-break point does not
//                                require reshaping.
//
// For example:
//   Given the string "Line breaking example", an available space of 100px and a
//   mono-space font where each glyph is 10px wide.
//
//   Line breaking example
//   |        |
//   0       100px
//
//   The candidate (or ideal) break opportunity would be at an offset of 10 as
//   the break would happen at exactly 100px in that case.
//   The previous valid break opportunity though is at an offset of 5.
//   If we further assume that the font kerns with space then even though it's a
//   valid break opportunity reshaping is required as the combined width of the
//   two segments "Line " and "breaking" may be different from "Line breaking".
PassRefPtr<ShapeResult> ShapingLineBreaker::ShapeLine(
    unsigned start,
    LayoutUnit available_space,
    unsigned* break_offset) {
  DCHECK_GE(available_space, LayoutUnit(0));
  unsigned range_start = result_->StartIndexForResult();
  unsigned range_end = result_->EndIndexForResult();
  DCHECK_GE(start, range_start);
  DCHECK_LT(start, range_end);

  // The start position in the original shape results.
  float start_position_float = result_->PositionForOffset(start - range_start);
  TextDirection direction = result_->Direction();
  LayoutUnit start_position = SnapStart(start_position_float, direction);

  // Find a candidate break opportunity by identifying the last offset before
  // exceeding the available space and the determine the closest valid break
  // preceding the candidate.
  LayoutUnit end_position = SnapEnd(start_position_float, direction) +
                            FlipRtl(available_space, direction);
  DCHECK_GE(FlipRtl(end_position - start_position, direction), LayoutUnit(0));
  unsigned candidate_break =
      result_->OffsetForPosition(end_position, false) + range_start;

  if (candidate_break >= range_end) {
    // The |result_| does not have glyphs to fill the available space,
    // and thus unable to compute. Return the result up to range_end.
    DCHECK_EQ(candidate_break, range_end);
    *break_offset = range_end;
    return ShapeToEnd(start, start_position, range_end);
  }

  // candidate_break should be >= start, but rounding errors can chime in when
  // comparing floats. See ShapeLineZeroAvailableWidth on Linux/Mac.
  candidate_break = std::max(candidate_break, start);
  unsigned break_opportunity =
      break_iterator_->PreviousBreakOpportunity(candidate_break, start);
  if (break_opportunity <= start) {
    break_opportunity = break_iterator_->NextBreakOpportunity(
        std::max(candidate_break, start + 1));
    // |range_end| may not be a break opportunity, but this function cannot
    // measure beyond it.
    break_opportunity = std::min(break_opportunity, range_end);
  }
  DCHECK_GT(break_opportunity, start);

  // If the start offset is not at a safe-to-break boundary the content between
  // the start and the next safe-to-break boundary needs to be reshaped and the
  // available space adjusted to take the reshaping into account.
  RefPtr<ShapeResult> line_start_result;
  unsigned first_safe =
      NextSafeToBreakBefore(shaper_->GetText(), shaper_->TextLength(), start);
  DCHECK_GE(first_safe, start);
  // Reshape takes place only when first_safe is before the break opportunity.
  // Otherwise reshape will be part of line_end_result.
  if (first_safe != start && first_safe < break_opportunity) {
    LayoutUnit original_width =
        FlipRtl(SnapEnd(result_->PositionForOffset(first_safe - range_start),
                        direction) -
                    start_position,
                direction);
    line_start_result = shaper_->Shape(font_, direction, start, first_safe);
    available_space += line_start_result->SnappedWidth() - original_width;
  }

  RefPtr<ShapeResult> line_end_result;
  unsigned last_safe = break_opportunity;
  while (break_opportunity > start) {
    // If the previous valid break opportunity is not at a safe-to-break
    // boundary reshape between the safe-to-break offset and the valid break
    // offset. If the resulting width exceeds the available space the
    // preceding boundary is tried until the available space is sufficient.
    unsigned previous_safe = std::max(
        PreviousSafeToBreakAfter(shaper_->GetText(), start, break_opportunity),
        start);
    DCHECK_LE(previous_safe, break_opportunity);
    if (previous_safe != break_opportunity) {
      LayoutUnit safe_position = SnapStart(
          result_->PositionForOffset(previous_safe - range_start), direction);
      while (break_opportunity > previous_safe && previous_safe >= start) {
        DCHECK_LE(break_opportunity, range_end);
        line_end_result =
            shaper_->Shape(font_, direction, previous_safe, break_opportunity);
        if (line_end_result->SnappedWidth() <=
            FlipRtl(end_position - safe_position, direction))
          break;
        // Doesn't fit after the reshape. Try previous break opportunity, or
        // overflow if there were none.
        unsigned previous_break_opportunity =
            break_iterator_->PreviousBreakOpportunity(break_opportunity - 1,
                                                      start);
        if (previous_break_opportunity <= start)
          break;
        break_opportunity = previous_break_opportunity;
        line_end_result = nullptr;
      }
    }

    if (break_opportunity > start) {
      last_safe = previous_safe;
      break;
    }

    // No suitable break opportunity, not exceeding the available space,
    // found. Choose the next valid one even though it will overflow.
    break_opportunity = break_iterator_->NextBreakOpportunity(candidate_break);
    // |range_end| may not be a break opportunity, but this function cannot
    // measure beyond it.
    break_opportunity = std::min(break_opportunity, range_end);
  }

  // Create shape results for the line by copying from the re-shaped result (if
  // reshaping was needed) and the original shape results.
  RefPtr<ShapeResult> line_result = ShapeResult::Create(font_, 0, direction);
  unsigned max_length = std::numeric_limits<unsigned>::max();
  if (line_start_result)
    line_start_result->CopyRange(0, max_length, line_result.Get());
  if (last_safe > first_safe)
    result_->CopyRange(first_safe, last_safe, line_result.Get());
  if (line_end_result)
    line_end_result->CopyRange(last_safe, max_length, line_result.Get());

  DCHECK_GT(break_opportunity, start);
  DCHECK_EQ(std::min(break_opportunity, range_end) - start,
            line_result->NumCharacters());

  *break_offset = break_opportunity;
  return line_result.Release();
}

// Shape from the specified offset to the end of the ShapeResult.
// If |start| is safe-to-break, this copies the subset of the result.
PassRefPtr<ShapeResult> ShapingLineBreaker::ShapeToEnd(
    unsigned start,
    LayoutUnit start_position,
    unsigned range_end) {
  unsigned first_safe =
      NextSafeToBreakBefore(shaper_->GetText(), shaper_->TextLength(), start);
  DCHECK_GE(first_safe, start);

  RefPtr<ShapeResult> line_result;
  TextDirection direction = result_->Direction();
  if (first_safe == start) {
    // If |start| is safe-to-break, reshape is not needed.
    line_result = ShapeResult::Create(font_, 0, direction);
    result_->CopyRange(start, range_end, line_result.Get());
  } else if (first_safe < range_end) {
    // Otherwise reshape to the first safe, then copy the rest.
    line_result = shaper_->Shape(font_, direction, start, first_safe);
    result_->CopyRange(first_safe, range_end, line_result.Get());
  } else {
    // If no safe-to-break in the ragne, reshape the whole range.
    line_result = shaper_->Shape(font_, direction, start, range_end);
  }
  return line_result.Release();
}

}  // namespace blink
