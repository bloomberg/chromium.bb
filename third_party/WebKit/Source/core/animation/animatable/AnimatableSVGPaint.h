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

#ifndef AnimatableSVGPaint_h
#define AnimatableSVGPaint_h

#include "core/animation/animatable/AnimatableColor.h"
#include "core/animation/animatable/AnimatableValue.h"
#include "core/style/SVGComputedStyleDefs.h"

namespace blink {

class AnimatableSVGPaint final : public AnimatableValue {
 public:
  ~AnimatableSVGPaint() override {}
  static PassRefPtr<AnimatableSVGPaint> Create(SVGPaintType type,
                                               SVGPaintType visited_link_type,
                                               const Color& color,
                                               const Color& visited_link_color,
                                               const String& uri,
                                               const String& visited_link_uri) {
    return AdoptRef(new AnimatableSVGPaint(
        type, visited_link_type,
        AnimatableColor::Create(color, visited_link_color), uri,
        visited_link_uri));
  }

 private:
  AnimatableSVGPaint(SVGPaintType type,
                     SVGPaintType visited_link_type,
                     PassRefPtr<AnimatableColor> color,
                     const String& uri,
                     const String& visited_link_uri)
      : type_(type),
        visited_link_type_(visited_link_type),
        color_(std::move(color)),
        uri_(uri),
        visited_link_uri_(visited_link_uri) {}
  AnimatableType GetType() const override { return kTypeSVGPaint; }
  bool EqualTo(const AnimatableValue*) const override;

  SVGPaintType type_;
  SVGPaintType visited_link_type_;
  // AnimatableColor includes a visited link color.
  RefPtr<AnimatableColor> color_;
  String uri_;
  String visited_link_uri_;
};

DEFINE_ANIMATABLE_VALUE_TYPE_CASTS(AnimatableSVGPaint, IsSVGPaint());

}  // namespace blink

#endif  // AnimatableSVGPaint_h
