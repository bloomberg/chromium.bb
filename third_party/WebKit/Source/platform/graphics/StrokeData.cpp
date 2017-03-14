// Copyright (C) 2013 Google Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "platform/graphics/StrokeData.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

void StrokeData::setLineDash(const DashArray& dashes, float dashOffset) {
  // FIXME: This is lifted directly off SkiaSupport, lines 49-74
  // so it is not guaranteed to work correctly.
  size_t dashLength = dashes.size();
  if (!dashLength) {
    // If no dash is set, revert to solid stroke
    // FIXME: do we need to set NoStroke in some cases?
    m_style = SolidStroke;
    m_dash.reset();
    return;
  }

  size_t count = !(dashLength % 2) ? dashLength : dashLength * 2;
  std::unique_ptr<SkScalar[]> intervals = wrapArrayUnique(new SkScalar[count]);

  for (unsigned i = 0; i < count; i++)
    intervals[i] = dashes[i % dashLength];

  m_dash = SkDashPathEffect::Make(intervals.get(), count, dashOffset);
}

void StrokeData::setupPaint(PaintFlags* flags, int length) const {
  flags->setStyle(PaintFlags::kStroke_Style);
  flags->setStrokeWidth(SkFloatToScalar(m_thickness));
  flags->setStrokeCap(m_lineCap);
  flags->setStrokeJoin(m_lineJoin);
  flags->setStrokeMiter(SkFloatToScalar(m_miterLimit));

  setupPaintDashPathEffect(flags, length);
}

void StrokeData::setupPaintDashPathEffect(PaintFlags* flags, int length) const {
  if (m_dash) {
    flags->setPathEffect(m_dash);
  } else if (strokeIsDashed(m_thickness, m_style)) {
    float dashLength = m_thickness;
    float gapLength = dashLength;
    if (m_style == DashedStroke) {
      dashLength *= StrokeData::dashLengthRatio(m_thickness);
      gapLength *= StrokeData::dashGapRatio(m_thickness);
    }
    // Account for modification to effective length in
    // GraphicsContext::adjustLineToPixelBoundaries
    length -= 2 * m_thickness;
    if (length <= dashLength) {
      // No space for dashes
      flags->setPathEffect(0);
    } else if (length <= 2 * dashLength + gapLength) {
      // Exactly 2 dashes proportionally sized
      float multiplier = length / (2 * dashLength + gapLength);
      SkScalar intervals[2] = {dashLength * multiplier, gapLength * multiplier};
      flags->setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));
    } else {
      float gap = gapLength;
      if (m_style == DashedStroke)
        gap = selectBestDashGap(length, dashLength, gapLength);
      SkScalar intervals[2] = {dashLength, gap};
      flags->setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));
    }
  } else if (m_style == DottedStroke) {
    flags->setStrokeCap((PaintFlags::Cap)RoundCap);
    // Adjust the width to get equal dot spacing as much as possible.
    float perDotLength = m_thickness * 2;
    if (length < perDotLength) {
      // Not enoguh space for 2 dots. Just draw 1 by giving a gap that is
      // bigger than the length.
      SkScalar intervals[2] = {0, perDotLength};
      flags->setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));
      return;
    }

    static const float epsilon = 1.0e-2f;
    float gap = selectBestDashGap(length, m_thickness, m_thickness);
    SkScalar intervals[2] = {0, gap + m_thickness - epsilon};
    flags->setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));
  } else {
    // TODO(schenney): WavyStroke https://crbug.com/229574
    flags->setPathEffect(0);
  }
}

bool StrokeData::strokeIsDashed(float width, StrokeStyle style) {
  return style == DashedStroke || (style == DottedStroke && width <= 3);
}

float StrokeData::selectBestDashGap(float strokeLength,
                                    float dashLength,
                                    float gapLength) {
  // Determine what number of dashes gives the minimum deviation from
  // gapLength between dashes. Set the gap to that width.
  float minNumDashes =
      floorf((strokeLength + gapLength) / (dashLength + gapLength));
  float maxNumDashes = minNumDashes + 1;
  float minGap =
      (strokeLength - minNumDashes * dashLength) / (minNumDashes - 1);
  float maxGap =
      (strokeLength - maxNumDashes * dashLength) / (maxNumDashes - 1);
  return fabs(minGap - gapLength) < fabs(maxGap - gapLength) ? minGap : maxGap;
}

}  // namespace blink
