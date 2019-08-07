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

#include <memory>

#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"

namespace blink {

void StrokeData::SetLineDash(const DashArray& dashes, float dash_offset) {
  // FIXME: This is lifted directly off SkiaSupport, lines 49-74
  // so it is not guaranteed to work correctly.
  size_t dash_length = dashes.size();
  if (!dash_length) {
    // If no dash is set, revert to solid stroke
    // FIXME: do we need to set NoStroke in some cases?
    style_ = kSolidStroke;
    dash_.reset();
    return;
  }

  size_t count = !(dash_length % 2) ? dash_length : dash_length * 2;
  auto intervals = std::make_unique<SkScalar[]>(count);

  for (unsigned i = 0; i < count; i++)
    intervals[i] = dashes[i % dash_length];

  dash_ = SkDashPathEffect::Make(intervals.get(), count, dash_offset);
}

void StrokeData::SetupPaint(PaintFlags* flags,
                            const int length,
                            const int dash_thickness) const {
  flags->setStyle(PaintFlags::kStroke_Style);
  flags->setStrokeWidth(SkFloatToScalar(thickness_));
  flags->setStrokeCap(line_cap_);
  flags->setStrokeJoin(line_join_);
  flags->setStrokeMiter(SkFloatToScalar(miter_limit_));

  SetupPaintDashPathEffect(flags, length, dash_thickness);
}

void StrokeData::SetupPaintDashPathEffect(PaintFlags* flags,
                                          const int length,
                                          const int dash_thickness) const {
  int dash_width = dash_thickness ? dash_thickness : thickness_;
  if (dash_) {
    flags->setPathEffect(dash_);
  } else if (StrokeIsDashed(dash_width, style_)) {
    float dash_length = dash_width;
    float gap_length = dash_length;
    if (style_ == kDashedStroke) {
      dash_length *= StrokeData::DashLengthRatio(dash_width);
      gap_length *= StrokeData::DashGapRatio(dash_width);
    }
    if (length <= dash_length * 2) {
      // No space for dashes
      flags->setPathEffect(nullptr);
    } else if (length <= 2 * dash_length + gap_length) {
      // Exactly 2 dashes proportionally sized
      float multiplier = length / (2 * dash_length + gap_length);
      SkScalar intervals[2] = {dash_length * multiplier,
                               gap_length * multiplier};
      flags->setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));
    } else {
      float gap = gap_length;
      if (style_ == kDashedStroke)
        gap = SelectBestDashGap(length, dash_length, gap_length);
      SkScalar intervals[2] = {dash_length, gap};
      flags->setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));
    }
  } else if (style_ == kDottedStroke) {
    flags->setStrokeCap((PaintFlags::Cap)kRoundCap);
    // Adjust the width to get equal dot spacing as much as possible.
    float per_dot_length = dash_width * 2;
    if (length < per_dot_length) {
      // Not enoguh space for 2 dots. Just draw 1 by giving a gap that is
      // bigger than the length.
      SkScalar intervals[2] = {0, per_dot_length};
      flags->setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));
      return;
    }

    // Epsilon ensures that we get a whole dot at the end of the line,
    // even if that dot is a little inside the true endpoint. Without it
    // we can drop the end dot due to rounding along the line.
    static const float kEpsilon = 1.0e-2f;
    float gap = SelectBestDashGap(length, dash_width, dash_width);
    SkScalar intervals[2] = {0, gap + dash_width - kEpsilon};
    flags->setPathEffect(SkDashPathEffect::Make(intervals, 2, 0));
  } else {
    flags->setPathEffect(nullptr);
  }
}

bool StrokeData::StrokeIsDashed(float width, StrokeStyle style) {
  return style == kDashedStroke || (style == kDottedStroke && width <= 3);
}

float StrokeData::SelectBestDashGap(float stroke_length,
                                    float dash_length,
                                    float gap_length) {
  // Determine what number of dashes gives the minimum deviation from
  // gapLength between dashes. Set the gap to that width.
  float min_num_dashes =
      floorf((stroke_length + gap_length) / (dash_length + gap_length));
  float max_num_dashes = min_num_dashes + 1;
  float min_gap =
      (stroke_length - min_num_dashes * dash_length) / (min_num_dashes - 1);
  float max_gap =
      (stroke_length - max_num_dashes * dash_length) / (max_num_dashes - 1);
  return (max_gap <= 0) ||
                 (fabs(min_gap - gap_length) < fabs(max_gap - gap_length))
             ? min_gap
             : max_gap;
}

}  // namespace blink
