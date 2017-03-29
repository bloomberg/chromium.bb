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
                                       LineBreakType breakType)
    : m_shaper(shaper),
      m_font(font),
      m_result(result),
      m_locale(locale),
      m_breakType(breakType) {
  m_text = String(shaper->text(), shaper->textLength());
}

namespace {

unsigned previousSafeToBreakAfter(const UChar* text,
                                  unsigned start,
                                  unsigned offset) {
  // TODO(eae): This is quite incorrect. It should be changed to use the
  // HarfBuzzHarfBuzz safe to break info when available.
  for (; offset > start; offset--) {
    if (text[offset - 1] == spaceCharacter)
      break;
  }
  return offset;
}

unsigned nextSafeToBreakBefore(const UChar* text,
                               unsigned end,
                               unsigned offset) {
  // TODO(eae): This is quite incorrect. It should be changed to use the
  // HarfBuzzHarfBuzz safe to break info when available.
  for (; offset < end; offset++) {
    if (text[offset] == spaceCharacter)
      break;
  }
  return offset;
}

}  // namespace

// TODO(eae); Should take a const LazyLineBreakIterator& but that requires the
// LazyLineBreakIterator::isBreakable method to be updated to be const.
unsigned ShapingLineBreaker::previousBreakOpportunity(
    LazyLineBreakIterator* breakIterator,
    unsigned start,
    unsigned offset) {
  DCHECK(breakIterator);
  unsigned pos = std::min(start + offset, m_shaper->textLength());
  for (; pos > start; pos--) {
    int nextBreak = 0;
    if (breakIterator->isBreakable(pos, nextBreak, m_breakType))
      return pos;
  }
  return start;
}

unsigned ShapingLineBreaker::nextBreakOpportunity(
    LazyLineBreakIterator* breakIterator,
    unsigned offset) {
  DCHECK(breakIterator);
  int nextBreak = 0;
  breakIterator->isBreakable(offset, nextBreak, m_breakType);
  return nextBreak;
}

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
PassRefPtr<ShapeResult> ShapingLineBreaker::shapeLine(unsigned start,
                                                      LayoutUnit availableSpace,
                                                      unsigned* breakOffset) {
  // The start position in the original shape results.
  LayoutUnit startPosition = m_result->snappedStartPositionForOffset(start);
  TextDirection direction = m_result->direction();

  // If the start offset is not at a safe-to-break boundary the content between
  // the start and the next safe-to-break boundary needs to be reshaped and the
  // available space adjusted to take the reshaping into account.
  RefPtr<ShapeResult> lineStartResult;
  unsigned firstSafe =
      nextSafeToBreakBefore(m_shaper->text(), m_shaper->textLength(), start);
  if (firstSafe != start) {
    LayoutUnit originalWidth =
        m_result->snappedEndPositionForOffset(firstSafe) - startPosition;
    lineStartResult = m_shaper->shape(m_font, direction, start, firstSafe);
    availableSpace += lineStartResult->snappedWidth() - originalWidth;
  }

  // Find a candidate break opportunity by identifying the last offset before
  // exceeding the available space and the determine the closest valid break
  // preceding the candidate.
  LazyLineBreakIterator breakIterator(m_text, m_locale);
  LayoutUnit endPosition = startPosition + availableSpace;
  unsigned candidateBreak = m_result->offsetForPosition(endPosition, false);
  unsigned breakOpportunity =
      previousBreakOpportunity(&breakIterator, start, candidateBreak);
  if (breakOpportunity <= start) {
    breakOpportunity = nextBreakOpportunity(&breakIterator, candidateBreak);
  }

  RefPtr<ShapeResult> lineEndResult;
  unsigned lastSafe = breakOpportunity;
  while (breakOpportunity > start) {
    // If the previous valid break opportunity is not at a safe-to-break
    // boundary reshape between the safe-to-break offset and the valid break
    // offset. If the resulting width exceeds the available space the
    // preceding boundary is tried until the available space is sufficient.
    unsigned previousSafe = std::max(
        previousSafeToBreakAfter(m_shaper->text(), start, breakOpportunity),
        start);
    if (previousSafe != breakOpportunity) {
      LayoutUnit safePosition =
          m_result->snappedStartPositionForOffset(previousSafe);
      while (breakOpportunity > previousSafe && previousSafe > start) {
        lineEndResult =
            m_shaper->shape(m_font, direction, previousSafe, breakOpportunity);
        if (safePosition + lineEndResult->snappedWidth() <= endPosition)
          break;
        lineEndResult = nullptr;
        breakOpportunity = previousBreakOpportunity(&breakIterator, start,
                                                    breakOpportunity - 1);
      }
    }

    if (breakOpportunity > start) {
      lastSafe = previousSafe;
      break;
    }

    // No suitable break opportunity, not exceeding the available space,
    // found. Choose the next valid one even though it will overflow.
    breakOpportunity = nextBreakOpportunity(&breakIterator, candidateBreak);
  }

  // Create shape results for the line by copying from the re-shaped result (if
  // reshaping was needed) and the original shape results.
  RefPtr<ShapeResult> lineResult = ShapeResult::create(m_font, 0, direction);
  unsigned maxLength = std::numeric_limits<unsigned>::max();
  if (lineStartResult)
    lineStartResult->copyRange(0, maxLength, lineResult.get());
  if (lastSafe > firstSafe)
    m_result->copyRange(firstSafe, lastSafe, lineResult.get());
  if (lineEndResult)
    lineEndResult->copyRange(lastSafe, maxLength, lineResult.get());

  *breakOffset = breakOpportunity;
  return lineResult.release();
}

}  // namespace blink
