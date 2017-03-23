// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/shaping/ShapeResultBloberizer.h"

#include "platform/fonts/Font.h"
#include "platform/fonts/shaping/CachingWordShaper.h"
#include "platform/text/TextRun.h"

namespace blink {

ShapeResultBloberizer::ShapeResultBloberizer(const Font& font,
                                             float deviceScaleFactor,
                                             Type type)
    : m_font(font), m_deviceScaleFactor(deviceScaleFactor), m_type(type) {}

bool ShapeResultBloberizer::hasPendingVerticalOffsets() const {
  // We exclusively store either horizontal/x-only ofssets -- in which case
  // m_offsets.size == size, or vertical/xy offsets -- in which case
  // m_offsets.size == size * 2.
  DCHECK(m_pendingGlyphs.size() == m_pendingOffsets.size() ||
         m_pendingGlyphs.size() * 2 == m_pendingOffsets.size());
  return m_pendingGlyphs.size() != m_pendingOffsets.size();
}

void ShapeResultBloberizer::commitPendingRun() {
  if (m_pendingGlyphs.isEmpty())
    return;

  const auto pendingRotation = blobRotation(m_pendingFontData);
  if (pendingRotation != m_builderRotation) {
    // The pending run rotation doesn't match the current blob; start a new
    // blob.
    commitPendingBlob();
    m_builderRotation = pendingRotation;
  }

  SkPaint runPaint;
  runPaint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
  m_pendingFontData->platformData().setupPaint(&runPaint, m_deviceScaleFactor,
                                               &m_font);

  const auto runSize = m_pendingGlyphs.size();
  const auto& buffer = hasPendingVerticalOffsets()
                           ? m_builder.allocRunPos(runPaint, runSize)
                           : m_builder.allocRunPosH(runPaint, runSize, 0);

  std::copy(m_pendingGlyphs.begin(), m_pendingGlyphs.end(), buffer.glyphs);
  std::copy(m_pendingOffsets.begin(), m_pendingOffsets.end(), buffer.pos);

  m_builderRunCount += 1;
  m_pendingGlyphs.shrink(0);
  m_pendingOffsets.shrink(0);
}

void ShapeResultBloberizer::commitPendingBlob() {
  if (!m_builderRunCount)
    return;

  m_blobs.emplace_back(m_builder.make(), m_builderRotation);
  m_builderRunCount = 0;
}

const ShapeResultBloberizer::BlobBuffer& ShapeResultBloberizer::blobs() {
  commitPendingRun();
  commitPendingBlob();
  DCHECK(m_pendingGlyphs.isEmpty());
  DCHECK_EQ(m_builderRunCount, 0u);

  return m_blobs;
}

ShapeResultBloberizer::BlobRotation ShapeResultBloberizer::blobRotation(
    const SimpleFontData* fontData) {
  // For vertical upright text we need to compensate the inherited 90deg CW
  // rotation (using a 90deg CCW rotation).
  return (fontData->platformData().isVerticalAnyUpright() &&
          fontData->verticalData())
             ? BlobRotation::CCWRotation
             : BlobRotation::NoRotation;
}

}  // namespace blink
