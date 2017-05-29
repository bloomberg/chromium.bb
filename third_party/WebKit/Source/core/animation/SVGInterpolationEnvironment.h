// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGInterpolationEnvironment_h
#define SVGInterpolationEnvironment_h

#include "core/animation/InterpolationEnvironment.h"
#include "platform/wtf/Assertions.h"

namespace blink {

class SVGPropertyBase;
class SVGElement;

class SVGInterpolationEnvironment : public InterpolationEnvironment {
 public:
  explicit SVGInterpolationEnvironment(const InterpolationTypesMap& map,
                                       SVGElement& svg_element,
                                       const SVGPropertyBase& svg_base_value)
      : InterpolationEnvironment(map),
        svg_element_(&svg_element),
        svg_base_value_(&svg_base_value) {}

  bool IsSVG() const final { return true; }

  SVGElement& SvgElement() {
    DCHECK(svg_element_);
    return *svg_element_;
  }
  const SVGElement& SvgElement() const {
    DCHECK(svg_element_);
    return *svg_element_;
  }

  const SVGPropertyBase& SvgBaseValue() const {
    DCHECK(svg_base_value_);
    return *svg_base_value_;
  }

 private:
  Member<SVGElement> svg_element_ = nullptr;
  Member<const SVGPropertyBase> svg_base_value_ = nullptr;
};

DEFINE_TYPE_CASTS(SVGInterpolationEnvironment,
                  InterpolationEnvironment,
                  value,
                  value->IsSVG(),
                  value.IsSVG());

}  // namespace blink

#endif  // SVGInterpolationEnvironment_h
