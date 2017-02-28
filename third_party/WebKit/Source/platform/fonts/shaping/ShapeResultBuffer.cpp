// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultBuffer.h"

#include "platform/fonts/CharacterRange.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/shaping/ShapeResultInlineHeaders.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/text/Character.h"
#include "platform/text/TextBreakIterator.h"
#include "platform/text/TextDirection.h"

namespace blink {

namespace {

inline bool isSkipInkException(const GlyphBuffer& glyphBuffer,
                               const TextRun& run,
                               unsigned characterIndex) {
  // We want to skip descenders in general, but it is undesirable renderings for
  // CJK characters.
  return glyphBuffer.type() == GlyphBuffer::Type::TextIntercepts &&
         !run.is8Bit() &&
         Character::isCJKIdeographOrSymbol(run.codepointAt(characterIndex));
}

inline void addGlyphToBuffer(GlyphBuffer* glyphBuffer,
                             float advance,
                             hb_direction_t direction,
                             const SimpleFontData* fontData,
                             const HarfBuzzRunGlyphData& glyphData,
                             const TextRun& run,
                             unsigned characterIndex) {
  FloatPoint startOffset = HB_DIRECTION_IS_HORIZONTAL(direction)
                               ? FloatPoint(advance, 0)
                               : FloatPoint(0, advance);
  if (!isSkipInkException(*glyphBuffer, run, characterIndex)) {
    glyphBuffer->add(glyphData.glyph, fontData, startOffset + glyphData.offset);
  }
}

inline void addEmphasisMark(GlyphBuffer* buffer,
                            const GlyphData* emphasisData,
                            FloatPoint glyphCenter,
                            float midGlyphOffset) {
  ASSERT(buffer);
  ASSERT(emphasisData);

  const SimpleFontData* emphasisFontData = emphasisData->fontData;
  ASSERT(emphasisFontData);

  bool isVertical = emphasisFontData->platformData().isVerticalAnyUpright() &&
                    emphasisFontData->verticalData();

  if (!isVertical) {
    buffer->add(emphasisData->glyph, emphasisFontData,
                midGlyphOffset - glyphCenter.x());
  } else {
    buffer->add(emphasisData->glyph, emphasisFontData,
                FloatPoint(-glyphCenter.x(), midGlyphOffset - glyphCenter.y()));
  }
}

inline unsigned countGraphemesInCluster(const UChar* str,
                                        unsigned strLength,
                                        uint16_t startIndex,
                                        uint16_t endIndex) {
  if (startIndex > endIndex) {
    uint16_t tempIndex = startIndex;
    startIndex = endIndex;
    endIndex = tempIndex;
  }
  uint16_t length = endIndex - startIndex;
  ASSERT(static_cast<unsigned>(startIndex + length) <= strLength);
  TextBreakIterator* cursorPosIterator =
      cursorMovementIterator(&str[startIndex], length);

  int cursorPos = cursorPosIterator->current();
  int numGraphemes = -1;
  while (0 <= cursorPos) {
    cursorPos = cursorPosIterator->next();
    numGraphemes++;
  }
  return std::max(0, numGraphemes);
}

}  // anonymous namespace

float ShapeResultBuffer::fillGlyphBufferForResult(GlyphBuffer* glyphBuffer,
                                                  const ShapeResult& result,
                                                  const TextRun& textRun,
                                                  float initialAdvance,
                                                  unsigned from,
                                                  unsigned to,
                                                  unsigned runOffset) {
  auto totalAdvance = initialAdvance;

  for (const auto& run : result.m_runs) {
    totalAdvance = run->forEachGlyphInRange(
        totalAdvance, from, to, runOffset,
        [&](const HarfBuzzRunGlyphData& glyphData, float totalAdvance,
            uint16_t characterIndex) -> bool {

          addGlyphToBuffer(glyphBuffer, totalAdvance, run->m_direction,
                           run->m_fontData.get(), glyphData, textRun,
                           characterIndex);
          return true;
        });
  }

  return totalAdvance;
}

float ShapeResultBuffer::fillGlyphBufferForTextEmphasisRun(
    GlyphBuffer* glyphBuffer,
    const ShapeResult::RunInfo* run,
    const TextRun& textRun,
    const GlyphData* emphasisData,
    float initialAdvance,
    unsigned from,
    unsigned to,
    unsigned runOffset) {
  if (!run)
    return 0;

  unsigned graphemesInCluster = 1;
  float clusterAdvance = 0;

  FloatPoint glyphCenter =
      emphasisData->fontData->boundsForGlyph(emphasisData->glyph).center();

  TextDirection direction = textRun.direction();

  // A "cluster" in this context means a cluster as it is used by HarfBuzz:
  // The minimal group of characters and corresponding glyphs, that cannot be
  // broken down further from a text shaping point of view.  A cluster can
  // contain multiple glyphs and grapheme clusters, with mutually overlapping
  // boundaries. Below we count grapheme clusters per HarfBuzz clusters, then
  // linearly split the sum of corresponding glyph advances by the number of
  // grapheme clusters in order to find positions for emphasis mark drawing.
  uint16_t clusterStart = static_cast<uint16_t>(
      direction == TextDirection::kRtl
          ? run->m_startIndex + run->m_numCharacters + runOffset
          : run->glyphToCharacterIndex(0) + runOffset);

  float advanceSoFar = initialAdvance;
  const unsigned numGlyphs = run->m_glyphData.size();
  for (unsigned i = 0; i < numGlyphs; ++i) {
    const HarfBuzzRunGlyphData& glyphData = run->m_glyphData[i];
    uint16_t currentCharacterIndex =
        run->m_startIndex + glyphData.characterIndex + runOffset;
    bool isRunEnd = (i + 1 == numGlyphs);
    bool isClusterEnd =
        isRunEnd || (run->glyphToCharacterIndex(i + 1) + runOffset !=
                     currentCharacterIndex);

    if ((direction == TextDirection::kRtl && currentCharacterIndex >= to) ||
        (direction != TextDirection::kRtl && currentCharacterIndex < from)) {
      advanceSoFar += glyphData.advance;
      direction == TextDirection::kRtl ? --clusterStart : ++clusterStart;
      continue;
    }

    clusterAdvance += glyphData.advance;

    if (textRun.is8Bit()) {
      float glyphAdvanceX = glyphData.advance;
      if (Character::canReceiveTextEmphasis(textRun[currentCharacterIndex])) {
        addEmphasisMark(glyphBuffer, emphasisData, glyphCenter,
                        advanceSoFar + glyphAdvanceX / 2);
      }
      advanceSoFar += glyphAdvanceX;
    } else if (isClusterEnd) {
      uint16_t clusterEnd;
      if (direction == TextDirection::kRtl)
        clusterEnd = currentCharacterIndex;
      else
        clusterEnd = static_cast<uint16_t>(
            isRunEnd ? run->m_startIndex + run->m_numCharacters + runOffset
                     : run->glyphToCharacterIndex(i + 1) + runOffset);

      graphemesInCluster = countGraphemesInCluster(textRun.characters16(),
                                                   textRun.charactersLength(),
                                                   clusterStart, clusterEnd);
      if (!graphemesInCluster || !clusterAdvance)
        continue;

      float glyphAdvanceX = clusterAdvance / graphemesInCluster;
      for (unsigned j = 0; j < graphemesInCluster; ++j) {
        // Do not put emphasis marks on space, separator, and control
        // characters.
        if (Character::canReceiveTextEmphasis(textRun[currentCharacterIndex]))
          addEmphasisMark(glyphBuffer, emphasisData, glyphCenter,
                          advanceSoFar + glyphAdvanceX / 2);
        advanceSoFar += glyphAdvanceX;
      }
      clusterStart = clusterEnd;
      clusterAdvance = 0;
    }
  }
  return advanceSoFar - initialAdvance;
}

float ShapeResultBuffer::fillFastHorizontalGlyphBuffer(
    GlyphBuffer* glyphBuffer,
    const TextRun& textRun) const {
  DCHECK(!hasVerticalOffsets());
  DCHECK_NE(glyphBuffer->type(), GlyphBuffer::Type::TextIntercepts);

  float advance = 0;

  for (unsigned i = 0; i < m_results.size(); ++i) {
    const auto& wordResult = isLeftToRightDirection(textRun.direction())
                                 ? m_results[i]
                                 : m_results[m_results.size() - 1 - i];
    DCHECK(!wordResult->hasVerticalOffsets());

    for (const auto& run : wordResult->m_runs) {
      DCHECK(run);
      DCHECK(HB_DIRECTION_IS_HORIZONTAL(run->m_direction));

      advance = run->forEachGlyph(
          advance,
          [&](const HarfBuzzRunGlyphData& glyphData,
              float totalAdvance) -> bool {
            DCHECK(!glyphData.offset.height());

            glyphBuffer->add(glyphData.glyph, run->m_fontData.get(),
                             totalAdvance + glyphData.offset.width());
            return true;
          });
    }
  }

  ASSERT(!glyphBuffer->hasVerticalOffsets());

  return advance;
}

float ShapeResultBuffer::fillGlyphBuffer(GlyphBuffer* glyphBuffer,
                                         const TextRun& textRun,
                                         unsigned from,
                                         unsigned to) const {
  // Fast path: full run with no vertical offsets, no text intercepts.
  if (!from && to == textRun.length() && !hasVerticalOffsets() &&
      glyphBuffer->type() != GlyphBuffer::Type::TextIntercepts)
    return fillFastHorizontalGlyphBuffer(glyphBuffer, textRun);

  float advance = 0;

  if (textRun.rtl()) {
    unsigned wordOffset = textRun.length();
    for (unsigned j = 0; j < m_results.size(); j++) {
      unsigned resolvedIndex = m_results.size() - 1 - j;
      const RefPtr<const ShapeResult>& wordResult = m_results[resolvedIndex];
      wordOffset -= wordResult->numCharacters();
      advance = fillGlyphBufferForResult(glyphBuffer, *wordResult, textRun,
                                         advance, from, to, wordOffset);
    }
  } else {
    unsigned wordOffset = 0;
    for (const auto& wordResult : m_results) {
      advance = fillGlyphBufferForResult(glyphBuffer, *wordResult, textRun,
                                         advance, from, to, wordOffset);
      wordOffset += wordResult->numCharacters();
    }
  }

  return advance;
}

float ShapeResultBuffer::fillGlyphBufferForTextEmphasis(
    GlyphBuffer* glyphBuffer,
    const TextRun& textRun,
    const GlyphData* emphasisData,
    unsigned from,
    unsigned to) const {
  float advance = 0;
  unsigned wordOffset = textRun.rtl() ? textRun.length() : 0;

  for (unsigned j = 0; j < m_results.size(); j++) {
    unsigned resolvedIndex = textRun.rtl() ? m_results.size() - 1 - j : j;
    const RefPtr<const ShapeResult>& wordResult = m_results[resolvedIndex];
    for (unsigned i = 0; i < wordResult->m_runs.size(); i++) {
      unsigned resolvedOffset =
          wordOffset - (textRun.rtl() ? wordResult->numCharacters() : 0);
      advance += fillGlyphBufferForTextEmphasisRun(
          glyphBuffer, wordResult->m_runs[i].get(), textRun, emphasisData,
          advance, from, to, resolvedOffset);
    }
    wordOffset += wordResult->numCharacters() * (textRun.rtl() ? -1 : 1);
  }

  return advance;
}

// TODO(eae): This is a bit of a hack to allow reuse of the implementation
// for both ShapeResultBuffer and single ShapeResult use cases. Ideally the
// logic should move into ShapeResult itself and then the ShapeResultBuffer
// implementation may wrap that.
CharacterRange ShapeResultBuffer::getCharacterRange(
    RefPtr<const ShapeResult> result,
    TextDirection direction,
    float totalWidth,
    unsigned from,
    unsigned to) {
  Vector<RefPtr<const ShapeResult>, 64> results;
  results.push_back(result);
  return getCharacterRangeInternal(results, direction, totalWidth, from, to);
}

CharacterRange ShapeResultBuffer::getCharacterRangeInternal(
    const Vector<RefPtr<const ShapeResult>, 64>& results,
    TextDirection direction,
    float totalWidth,
    unsigned absoluteFrom,
    unsigned absoluteTo) {
  float currentX = 0;
  float fromX = 0;
  float toX = 0;
  bool foundFromX = false;
  bool foundToX = false;

  if (direction == TextDirection::kRtl)
    currentX = totalWidth;

  // The absoluteFrom and absoluteTo arguments represent the start/end offset
  // for the entire run, from/to are continuously updated to be relative to
  // the current word (ShapeResult instance).
  int from = absoluteFrom;
  int to = absoluteTo;

  unsigned totalNumCharacters = 0;
  for (unsigned j = 0; j < results.size(); j++) {
    const RefPtr<const ShapeResult> result = results[j];
    if (direction == TextDirection::kRtl) {
      // Convert logical offsets to visual offsets, because results are in
      // logical order while runs are in visual order.
      if (!foundFromX && from >= 0 &&
          static_cast<unsigned>(from) < result->numCharacters())
        from = result->numCharacters() - from - 1;
      if (!foundToX && to >= 0 &&
          static_cast<unsigned>(to) < result->numCharacters())
        to = result->numCharacters() - to - 1;
      currentX -= result->width();
    }
    for (unsigned i = 0; i < result->m_runs.size(); i++) {
      if (!result->m_runs[i])
        continue;
      DCHECK_EQ(direction == TextDirection::kRtl, result->m_runs[i]->rtl());
      int numCharacters = result->m_runs[i]->m_numCharacters;
      if (!foundFromX && from >= 0 && from < numCharacters) {
        fromX =
            result->m_runs[i]->xPositionForVisualOffset(from, AdjustToStart) +
            currentX;
        foundFromX = true;
      } else {
        from -= numCharacters;
      }

      if (!foundToX && to >= 0 && to < numCharacters) {
        toX = result->m_runs[i]->xPositionForVisualOffset(to, AdjustToEnd) +
              currentX;
        foundToX = true;
      } else {
        to -= numCharacters;
      }

      if (foundFromX && foundToX)
        break;
      currentX += result->m_runs[i]->m_width;
    }
    if (direction == TextDirection::kRtl)
      currentX -= result->width();
    totalNumCharacters += result->numCharacters();
  }

  // The position in question might be just after the text.
  if (!foundFromX && absoluteFrom == totalNumCharacters) {
    fromX = direction == TextDirection::kRtl ? 0 : totalWidth;
    foundFromX = true;
  }
  if (!foundToX && absoluteTo == totalNumCharacters) {
    toX = direction == TextDirection::kRtl ? 0 : totalWidth;
    foundToX = true;
  }
  if (!foundFromX)
    fromX = 0;
  if (!foundToX)
    toX = direction == TextDirection::kRtl ? 0 : totalWidth;

  // None of our runs is part of the selection, possibly invalid arguments.
  if (!foundToX && !foundFromX)
    fromX = toX = 0;
  if (fromX < toX)
    return CharacterRange(fromX, toX);
  return CharacterRange(toX, fromX);
}

CharacterRange ShapeResultBuffer::getCharacterRange(TextDirection direction,
                                                    float totalWidth,
                                                    unsigned from,
                                                    unsigned to) const {
  return getCharacterRangeInternal(m_results, direction, totalWidth, from, to);
}

void ShapeResultBuffer::addRunInfoRanges(const ShapeResult::RunInfo& runInfo,
                                         float offset,
                                         Vector<CharacterRange>& ranges) {
  Vector<float> characterWidths(runInfo.m_numCharacters);
  for (const auto& glyph : runInfo.m_glyphData)
    characterWidths[glyph.characterIndex] += glyph.advance;

  for (unsigned characterIndex = 0; characterIndex < runInfo.m_numCharacters;
       characterIndex++) {
    float start = offset;
    offset += characterWidths[characterIndex];
    float end = offset;

    // To match getCharacterRange we flip ranges to ensure start <= end.
    if (end < start)
      ranges.push_back(CharacterRange(end, start));
    else
      ranges.push_back(CharacterRange(start, end));
  }
}

Vector<CharacterRange> ShapeResultBuffer::individualCharacterRanges(
    TextDirection direction,
    float totalWidth) const {
  Vector<CharacterRange> ranges;
  float currentX = direction == TextDirection::kRtl ? totalWidth : 0;
  for (const RefPtr<const ShapeResult> result : m_results) {
    if (direction == TextDirection::kRtl)
      currentX -= result->width();
    unsigned runCount = result->m_runs.size();
    for (unsigned index = 0; index < runCount; index++) {
      unsigned runIndex =
          direction == TextDirection::kRtl ? runCount - 1 - index : index;
      addRunInfoRanges(*result->m_runs[runIndex], currentX, ranges);
      currentX += result->m_runs[runIndex]->m_width;
    }
    if (direction == TextDirection::kRtl)
      currentX -= result->width();
  }
  return ranges;
}

int ShapeResultBuffer::offsetForPosition(const TextRun& run,
                                         float targetX,
                                         bool includePartialGlyphs) const {
  unsigned totalOffset;
  if (run.rtl()) {
    totalOffset = run.length();
    for (unsigned i = m_results.size(); i; --i) {
      const RefPtr<const ShapeResult>& wordResult = m_results[i - 1];
      if (!wordResult)
        continue;
      totalOffset -= wordResult->numCharacters();
      if (targetX >= 0 && targetX <= wordResult->width()) {
        int offsetForWord =
            wordResult->offsetForPosition(targetX, includePartialGlyphs);
        return totalOffset + offsetForWord;
      }
      targetX -= wordResult->width();
    }
  } else {
    totalOffset = 0;
    for (const auto& wordResult : m_results) {
      if (!wordResult)
        continue;
      int offsetForWord =
          wordResult->offsetForPosition(targetX, includePartialGlyphs);
      ASSERT(offsetForWord >= 0);
      totalOffset += offsetForWord;
      if (targetX >= 0 && targetX <= wordResult->width())
        return totalOffset;
      targetX -= wordResult->width();
    }
  }
  return totalOffset;
}

Vector<ShapeResultBuffer::RunFontData>
ShapeResultBuffer::runFontData() const {
  Vector<RunFontData> fontData;

  for (const auto& result : m_results) {
    for (const auto& run : result->m_runs) {
      fontData.push_back(RunFontData({run->m_fontData.get(),
                                      run->m_glyphData.size()}));
    }
  }
  return fontData;
}

}  // namespace blink
