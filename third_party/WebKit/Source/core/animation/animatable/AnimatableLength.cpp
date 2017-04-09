/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/animation/animatable/AnimatableLength.h"

#include "platform/CalculationValue.h"
#include "platform/animation/AnimationUtilities.h"

namespace blink {

namespace {

double ClampNumber(double value, ValueRange range) {
  if (range == kValueRangeNonNegative)
    return std::max(value, 0.0);
  DCHECK_EQ(range, kValueRangeAll);
  return value;
}

}  // namespace

AnimatableLength::AnimatableLength(const Length& length, float zoom) {
  DCHECK(zoom);
  PixelsAndPercent pixels_and_percent = length.GetPixelsAndPercent();
  pixels_ = pixels_and_percent.pixels / zoom;
  percent_ = pixels_and_percent.percent;
  has_pixels_ = length.GetType() != kPercent;
  has_percent_ = !length.IsFixed();
}

Length AnimatableLength::GetLength(float zoom, ValueRange range) const {
  if (!has_percent_)
    return Length(ClampNumber(pixels_, range) * zoom, kFixed);
  if (!has_pixels_)
    return Length(ClampNumber(percent_, range), kPercent);
  return Length(CalculationValue::Create(
      PixelsAndPercent(pixels_ * zoom, percent_), range));
}

bool AnimatableLength::EqualTo(const AnimatableValue* value) const {
  const AnimatableLength* length = ToAnimatableLength(value);
  return pixels_ == length->pixels_ && percent_ == length->percent_ &&
         has_pixels_ == length->has_pixels_ &&
         has_percent_ == length->has_percent_;
}

}  // namespace blink
