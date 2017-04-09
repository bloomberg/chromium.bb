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

#ifndef AnimatableColor_h
#define AnimatableColor_h

#include "core/CoreExport.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "platform/graphics/Color.h"
#include "wtf/Allocator.h"

namespace blink {

class AnimatableColorImpl {
  DISALLOW_NEW();

 public:
  AnimatableColorImpl(float red, float green, float blue, float alpha);
  AnimatableColorImpl(Color);
  bool operator==(const AnimatableColorImpl&) const;

 private:
  float alpha_;
  float red_;
  float green_;
  float blue_;
};

// This class handles both the regular and 'visted link' colors for a given
// property. Currently it is used for all properties, even those which do not
// support a separate 'visited link' color (eg SVG properties). This is correct
// but inefficient.
class AnimatableColor final : public AnimatableValue {
 public:
  static PassRefPtr<AnimatableColor> Create(
      const AnimatableColorImpl&,
      const AnimatableColorImpl& visited_link_color);

 private:
  AnimatableColor(const AnimatableColorImpl& color,
                  const AnimatableColorImpl& visited_link_color)
      : color_(color), visited_link_color_(visited_link_color) {}
  AnimatableType GetType() const override { return kTypeColor; }
  bool EqualTo(const AnimatableValue*) const override;
  const AnimatableColorImpl color_;
  const AnimatableColorImpl visited_link_color_;
};

DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(AnimatableColor, IsColor());

}  // namespace blink

#endif  // AnimatableColor_h
