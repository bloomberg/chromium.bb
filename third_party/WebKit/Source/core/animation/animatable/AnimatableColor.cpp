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

#include "core/animation/animatable/AnimatableColor.h"


namespace blink {

AnimatableColorImpl::AnimatableColorImpl(float red,
                                         float green,
                                         float blue,
                                         float alpha)
    : alpha_(clampTo(alpha, 0.0f, 1.0f)),
      red_(clampTo(red, 0.0f, 1.0f)),
      green_(clampTo(green, 0.0f, 1.0f)),
      blue_(clampTo(blue, 0.0f, 1.0f)) {}

AnimatableColorImpl::AnimatableColorImpl(Color color)
    : alpha_(color.Alpha() / 255.0f),
      red_(color.Red() / 255.0f * alpha_),
      green_(color.Green() / 255.0f * alpha_),
      blue_(color.Blue() / 255.0f * alpha_) {}

bool AnimatableColorImpl::operator==(const AnimatableColorImpl& other) const {
  return red_ == other.red_ && green_ == other.green_ && blue_ == other.blue_ &&
         alpha_ == other.alpha_;
}

PassRefPtr<AnimatableColor> AnimatableColor::Create(
    const AnimatableColorImpl& color,
    const AnimatableColorImpl& visited_link_color) {
  return AdoptRef(new AnimatableColor(color, visited_link_color));
}

bool AnimatableColor::EqualTo(const AnimatableValue* value) const {
  const AnimatableColor* color = ToAnimatableColor(value);
  return color_ == color->color_ &&
         visited_link_color_ == color->visited_link_color_;
}

}  // namespace blink
