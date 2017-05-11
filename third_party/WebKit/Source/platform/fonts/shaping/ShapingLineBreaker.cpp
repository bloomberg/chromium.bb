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

ShapingLineBreaker::ShapingLineBreaker(const HarfBuzzShaper* shaper,
                                       const Font* font,
                                       const ShapeResult* result,
                                       const AtomicString locale,
                                       LineBreakType break_type)
    : shaper_(shaper),
      font_(font),
      result_(result),
      locale_(locale),
      break_type_(break_type) {
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
  // The start position in the original shape results.
  LayoutUnit start_position = result_->SnappedStartPositionForOffset(start);
  TextDirection direction = result_->Direction();

  // If the start offset is not at a safe-to-break boundary the content between
  // the start and the next safe-to-break boundary needs to be reshaped and the
  // available space adjusted to take the reshaping into account.
  RefPtr<ShapeResult> line_start_result;
  unsigned first_safe =
      NextSafeToBreakBefore(shaper_->GetText(), shaper_->TextLength(), start);
  if (first_safe != start) {
    LayoutUnit original_width =
        result_->SnappedEndPositionForOffset(first_safe) - start_position;
    line_start_result = shaper_->Shape(font_, direction, start, first_safe);
    available_space += line_start_result->SnappedWidth() - original_width;
  }

  // Find a candidate break opportunity by identifying the last offset before
  // exceeding the available space and the determine the closest valid break
  // preceding the candidate.
  LazyLineBreakIterator break_iterator(text_, locale_, break_type_);
  LayoutUnit end_position = start_position + available_space;
  unsigned candidate_break = result_->OffsetForPosition(end_position, false);
  unsigned break_opportunity =
      break_iterator.PreviousBreakOpportunity(candidate_break, start);
  if (break_opportunity <= start) {
    break_opportunity = break_iterator.NextBreakOpportunity(candidate_break);
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
    if (previous_safe != break_opportunity) {
      LayoutUnit safe_position =
          result_->SnappedStartPositionForOffset(previous_safe);
      while (break_opportunity > previous_safe && previous_safe > start) {
        line_end_result =
            shaper_->Shape(font_, direction, previous_safe, break_opportunity);
        if (safe_position + line_end_result->SnappedWidth() <= end_position)
          break;
        line_end_result = nullptr;
        break_opportunity = break_iterator.PreviousBreakOpportunity(
            break_opportunity - 1, start);
      }
    }

    if (break_opportunity > start) {
      last_safe = previous_safe;
      break;
    }

    // No suitable break opportunity, not exceeding the available space,
    // found. Choose the next valid one even though it will overflow.
    break_opportunity = break_iterator.NextBreakOpportunity(candidate_break);
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

  *break_offset = break_opportunity;
  return line_result.Release();
}

}  // namespace blink
