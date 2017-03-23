/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (c) 2007, 2008, 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "platform/fonts/Font.h"

#include "platform/LayoutTestSupport.h"
#include "platform/LayoutUnit.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/CharacterRange.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontFallbackIterator.h"
#include "platform/fonts/FontFallbackList.h"
#include "platform/fonts/SimpleFontData.h"
#include "platform/fonts/shaping/CachingWordShaper.h"
#include "platform/fonts/shaping/ShapeResultBloberizer.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/text/BidiResolver.h"
#include "platform/text/Character.h"
#include "platform/text/TextRun.h"
#include "platform/text/TextRunIterator.h"
#include "platform/transforms/AffineTransform.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/Unicode.h"

using namespace WTF;
using namespace Unicode;

namespace blink {

Font::Font() : m_canShapeWordByWord(0), m_shapeWordByWordComputed(0) {}

Font::Font(const FontDescription& fd)
    : m_fontDescription(fd),
      m_canShapeWordByWord(0),
      m_shapeWordByWordComputed(0) {}

Font::Font(const Font& other)
    : m_fontDescription(other.m_fontDescription),
      m_fontFallbackList(other.m_fontFallbackList),
      // TODO(yosin): We should have a comment the reason why don't we copy
      // |m_canShapeWordByWord| and |m_shapeWordByWordComputed| from |other|,
      // since |operator=()| copies them from |other|.
      m_canShapeWordByWord(0),
      m_shapeWordByWordComputed(0) {}

Font& Font::operator=(const Font& other) {
  m_fontDescription = other.m_fontDescription;
  m_fontFallbackList = other.m_fontFallbackList;
  m_canShapeWordByWord = other.m_canShapeWordByWord;
  m_shapeWordByWordComputed = other.m_shapeWordByWordComputed;
  return *this;
}

bool Font::operator==(const Font& other) const {
  FontSelector* first =
      m_fontFallbackList ? m_fontFallbackList->getFontSelector() : 0;
  FontSelector* second = other.m_fontFallbackList
                             ? other.m_fontFallbackList->getFontSelector()
                             : 0;

  return first == second && m_fontDescription == other.m_fontDescription &&
         (m_fontFallbackList ? m_fontFallbackList->fontSelectorVersion() : 0) ==
             (other.m_fontFallbackList
                  ? other.m_fontFallbackList->fontSelectorVersion()
                  : 0) &&
         (m_fontFallbackList ? m_fontFallbackList->generation() : 0) ==
             (other.m_fontFallbackList ? other.m_fontFallbackList->generation()
                                       : 0);
}

void Font::update(FontSelector* fontSelector) const {
  // FIXME: It is pretty crazy that we are willing to just poke into a RefPtr,
  // but it ends up being reasonably safe (because inherited fonts in the render
  // tree pick up the new style anyway. Other copies are transient, e.g., the
  // state in the GraphicsContext, and won't stick around long enough to get you
  // in trouble). Still, this is pretty disgusting, and could eventually be
  // rectified by using RefPtrs for Fonts themselves.
  if (!m_fontFallbackList)
    m_fontFallbackList = FontFallbackList::create();
  m_fontFallbackList->invalidate(fontSelector);
}

namespace {

void drawBlobs(PaintCanvas* canvas,
               const PaintFlags& flags,
               const ShapeResultBloberizer::BlobBuffer& blobs,
               const FloatPoint& point) {
  for (const auto& blobInfo : blobs) {
    DCHECK(blobInfo.blob);
    PaintCanvasAutoRestore autoRestore(canvas, false);
    if (blobInfo.rotation == ShapeResultBloberizer::BlobRotation::CCWRotation) {
      canvas->save();

      SkMatrix m;
      m.setSinCos(-1, 0, point.x(), point.y());
      canvas->concat(m);
    }

    canvas->drawTextBlob(blobInfo.blob, point.x(), point.y(), flags);
  }
}

}  // anonymous ns

bool Font::drawText(PaintCanvas* canvas,
                    const TextRunPaintInfo& runInfo,
                    const FloatPoint& point,
                    float deviceScaleFactor,
                    const PaintFlags& flags) const {
  // Don't draw anything while we are using custom fonts that are in the process
  // of loading.
  if (shouldSkipDrawing())
    return false;

  ShapeResultBloberizer bloberizer(*this, deviceScaleFactor);
  CachingWordShaper(*this).fillGlyphs(runInfo, bloberizer);
  drawBlobs(canvas, flags, bloberizer.blobs(), point);
  return true;
}

bool Font::drawBidiText(PaintCanvas* canvas,
                        const TextRunPaintInfo& runInfo,
                        const FloatPoint& point,
                        CustomFontNotReadyAction customFontNotReadyAction,
                        float deviceScaleFactor,
                        const PaintFlags& flags) const {
  // Don't draw anything while we are using custom fonts that are in the process
  // of loading, except if the 'force' argument is set to true (in which case it
  // will use a fallback font).
  if (shouldSkipDrawing() &&
      customFontNotReadyAction == DoNotPaintIfFontNotReady)
    return false;

  // sub-run painting is not supported for Bidi text.
  const TextRun& run = runInfo.run;
  ASSERT((runInfo.from == 0) && (runInfo.to == run.length()));
  BidiResolver<TextRunIterator, BidiCharacterRun> bidiResolver;
  bidiResolver.setStatus(
      BidiStatus(run.direction(), run.directionalOverride()));
  bidiResolver.setPositionIgnoringNestedIsolates(TextRunIterator(&run, 0));

  // FIXME: This ownership should be reversed. We should pass BidiRunList
  // to BidiResolver in createBidiRunsForLine.
  BidiRunList<BidiCharacterRun>& bidiRuns = bidiResolver.runs();
  bidiResolver.createBidiRunsForLine(TextRunIterator(&run, run.length()));
  if (!bidiRuns.runCount())
    return true;

  FloatPoint currPoint = point;
  BidiCharacterRun* bidiRun = bidiRuns.firstRun();
  while (bidiRun) {
    TextRun subrun =
        run.subRun(bidiRun->start(), bidiRun->stop() - bidiRun->start());
    bool isRTL = bidiRun->level() % 2;
    subrun.setDirection(isRTL ? TextDirection::kRtl : TextDirection::kLtr);
    subrun.setDirectionalOverride(bidiRun->dirOverride(false));

    TextRunPaintInfo subrunInfo(subrun);
    subrunInfo.bounds = runInfo.bounds;

    ShapeResultBloberizer bloberizer(*this, deviceScaleFactor);
    float runWidth =
        CachingWordShaper(*this).fillGlyphs(subrunInfo, bloberizer);
    drawBlobs(canvas, flags, bloberizer.blobs(), currPoint);

    bidiRun = bidiRun->next();
    currPoint.move(runWidth, 0);
  }

  bidiRuns.deleteRuns();
  return true;
}

void Font::drawEmphasisMarks(PaintCanvas* canvas,
                             const TextRunPaintInfo& runInfo,
                             const AtomicString& mark,
                             const FloatPoint& point,
                             float deviceScaleFactor,
                             const PaintFlags& flags) const {
  if (shouldSkipDrawing())
    return;

  FontCachePurgePreventer purgePreventer;

  const auto emphasisGlyphData = getEmphasisMarkGlyphData(mark);
  if (!emphasisGlyphData.fontData)
    return;

  ShapeResultBloberizer bloberizer(*this, deviceScaleFactor);
  CachingWordShaper(*this).fillTextEmphasisGlyphs(runInfo, emphasisGlyphData,
                                                  bloberizer);
  drawBlobs(canvas, flags, bloberizer.blobs(), point);
}

float Font::width(const TextRun& run,
                  HashSet<const SimpleFontData*>* fallbackFonts,
                  FloatRect* glyphBounds) const {
  FontCachePurgePreventer purgePreventer;
  CachingWordShaper shaper(*this);
  return shaper.width(run, fallbackFonts, glyphBounds);
}

static int getInterceptsFromBlobs(
    const ShapeResultBloberizer::BlobBuffer& blobs,
    const SkPaint& paint,
    const std::tuple<float, float>& bounds,
    SkScalar* interceptsBuffer) {
  SkScalar boundsArray[2] = {std::get<0>(bounds), std::get<1>(bounds)};

  int numIntervals = 0;
  for (const auto& blobInfo : blobs) {
    DCHECK(blobInfo.blob);

    // ShapeResultBloberizer splits for a new blob rotation, but does not split
    // for a change in font. A TextBlob can contain runs with differing fonts
    // and the getTextBlobIntercepts method handles multiple fonts for us. For
    // upright in vertical blobs we currently have to bail, see crbug.com/655154
    if (blobInfo.rotation == ShapeResultBloberizer::BlobRotation::CCWRotation)
      continue;

    SkScalar* offsetInterceptsBuffer = nullptr;
    if (interceptsBuffer)
      offsetInterceptsBuffer = &interceptsBuffer[numIntervals];
    numIntervals += paint.getTextBlobIntercepts(
        blobInfo.blob.get(), boundsArray, offsetInterceptsBuffer);
  }
  return numIntervals;
}

void Font::getTextIntercepts(const TextRunPaintInfo& runInfo,
                             float deviceScaleFactor,
                             const PaintFlags& flags,
                             const std::tuple<float, float>& bounds,
                             Vector<TextIntercept>& intercepts) const {
  if (shouldSkipDrawing())
    return;

  ShapeResultBloberizer bloberizer(*this, deviceScaleFactor,
                                   ShapeResultBloberizer::Type::TextIntercepts);
  CachingWordShaper(*this).fillGlyphs(runInfo, bloberizer);
  const auto& blobs = bloberizer.blobs();

  // Get the number of intervals, without copying the actual values by
  // specifying nullptr for the buffer, following the Skia allocation model for
  // retrieving text intercepts.
  SkPaint paint(ToSkPaint(flags));
  int numIntervals = getInterceptsFromBlobs(blobs, paint, bounds, nullptr);
  if (!numIntervals)
    return;
  DCHECK_EQ(numIntervals % 2, 0);
  intercepts.resize(numIntervals / 2);

  getInterceptsFromBlobs(blobs, paint, bounds,
                         reinterpret_cast<SkScalar*>(intercepts.data()));
}

static inline FloatRect pixelSnappedSelectionRect(FloatRect rect) {
  // Using roundf() rather than ceilf() for the right edge as a compromise to
  // ensure correct caret positioning.
  float roundedX = roundf(rect.x());
  return FloatRect(roundedX, rect.y(), roundf(rect.maxX() - roundedX),
                   rect.height());
}

FloatRect Font::selectionRectForText(const TextRun& run,
                                     const FloatPoint& point,
                                     int height,
                                     int from,
                                     int to,
                                     bool accountForGlyphBounds) const {
  to = (to == -1 ? run.length() : to);

  TextRunPaintInfo runInfo(run);
  runInfo.from = from;
  runInfo.to = to;

  FontCachePurgePreventer purgePreventer;

  CachingWordShaper shaper(*this);
  CharacterRange range = shaper.getCharacterRange(run, from, to);

  return pixelSnappedSelectionRect(
      FloatRect(point.x() + range.start, point.y(), range.width(), height));
}

int Font::offsetForPosition(const TextRun& run,
                            float xFloat,
                            bool includePartialGlyphs) const {
  FontCachePurgePreventer purgePreventer;
  CachingWordShaper shaper(*this);
  return shaper.offsetForPosition(run, xFloat, includePartialGlyphs);
}

ShapeCache* Font::shapeCache() const {
  return m_fontFallbackList->shapeCache(m_fontDescription);
}

bool Font::canShapeWordByWord() const {
  if (!m_shapeWordByWordComputed) {
    m_canShapeWordByWord = computeCanShapeWordByWord();
    m_shapeWordByWordComputed = true;
  }
  return m_canShapeWordByWord;
};

bool Font::computeCanShapeWordByWord() const {
  if (!getFontDescription().getTypesettingFeatures())
    return true;

  if (!primaryFont())
    return false;

  const FontPlatformData& platformData = primaryFont()->platformData();
  TypesettingFeatures features = getFontDescription().getTypesettingFeatures();
  return !platformData.hasSpaceInLigaturesOrKerning(features);
};

void Font::willUseFontData(const String& text) const {
  const FontFamily& family = getFontDescription().family();
  if (m_fontFallbackList && m_fontFallbackList->getFontSelector() &&
      !family.familyIsEmpty())
    m_fontFallbackList->getFontSelector()->willUseFontData(
        getFontDescription(), family.family(), text);
}

PassRefPtr<FontFallbackIterator> Font::createFontFallbackIterator(
    FontFallbackPriority fallbackPriority) const {
  return FontFallbackIterator::create(m_fontDescription, m_fontFallbackList,
                                      fallbackPriority);
}

GlyphData Font::getEmphasisMarkGlyphData(const AtomicString& mark) const {
  if (mark.isEmpty())
    return GlyphData();

  TextRun emphasisMarkRun(mark, mark.length());
  return CachingWordShaper(*this).emphasisMarkGlyphData(emphasisMarkRun);
}

int Font::emphasisMarkAscent(const AtomicString& mark) const {
  FontCachePurgePreventer purgePreventer;

  const auto markGlyphData = getEmphasisMarkGlyphData(mark);
  const SimpleFontData* markFontData = markGlyphData.fontData;
  if (!markFontData)
    return 0;

  return markFontData->getFontMetrics().ascent();
}

int Font::emphasisMarkDescent(const AtomicString& mark) const {
  FontCachePurgePreventer purgePreventer;

  const auto markGlyphData = getEmphasisMarkGlyphData(mark);
  const SimpleFontData* markFontData = markGlyphData.fontData;
  if (!markFontData)
    return 0;

  return markFontData->getFontMetrics().descent();
}

int Font::emphasisMarkHeight(const AtomicString& mark) const {
  FontCachePurgePreventer purgePreventer;

  const auto markGlyphData = getEmphasisMarkGlyphData(mark);
  const SimpleFontData* markFontData = markGlyphData.fontData;
  if (!markFontData)
    return 0;

  return markFontData->getFontMetrics().height();
}

CharacterRange Font::getCharacterRange(const TextRun& run,
                                       unsigned from,
                                       unsigned to) const {
  FontCachePurgePreventer purgePreventer;
  CachingWordShaper shaper(*this);
  return shaper.getCharacterRange(run, from, to);
}

Vector<CharacterRange> Font::individualCharacterRanges(
    const TextRun& run) const {
  FontCachePurgePreventer purgePreventer;
  CachingWordShaper shaper(*this);
  auto ranges = shaper.individualCharacterRanges(run);
  // The shaper should return ranges.size == run.length but on some platforms
  // (OSX10.9.5) we are seeing cases in the upper end of the unicode range
  // where this is not true (see: crbug.com/620952). To catch these cases on
  // more popular platforms, and to protect users, we are using a CHECK here.
  CHECK_EQ(ranges.size(), run.length());
  return ranges;
}

bool Font::loadingCustomFonts() const {
  return m_fontFallbackList && m_fontFallbackList->loadingCustomFonts();
}

bool Font::isFallbackValid() const {
  return !m_fontFallbackList || m_fontFallbackList->isValid();
}

}  // namespace blink
