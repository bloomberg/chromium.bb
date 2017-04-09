// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationEnvironment_h
#define InterpolationEnvironment_h

#include "core/animation/InterpolationTypesMap.h"
#include "core/css/resolver/StyleResolverState.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"

namespace blink {

class ComputedStyle;
class SVGPropertyBase;
class SVGElement;

class InterpolationEnvironment {
  STACK_ALLOCATED();

 public:
  explicit InterpolationEnvironment(const InterpolationTypesMap& map,
                                    StyleResolverState& state)
      : interpolation_types_map_(map), state_(&state), style_(state.Style()) {}

  explicit InterpolationEnvironment(const InterpolationTypesMap& map,
                                    const ComputedStyle& style)
      : interpolation_types_map_(map), style_(&style) {}

  explicit InterpolationEnvironment(const InterpolationTypesMap& map,
                                    SVGElement& svg_element,
                                    const SVGPropertyBase& svg_base_value)
      : interpolation_types_map_(map),
        svg_element_(&svg_element),
        svg_base_value_(&svg_base_value) {}

  const InterpolationTypesMap& GetInterpolationTypesMap() const {
    return interpolation_types_map_;
  }

  StyleResolverState& GetState() {
    DCHECK(state_);
    return *state_;
  }
  const StyleResolverState& GetState() const {
    DCHECK(state_);
    return *state_;
  }

  const ComputedStyle& Style() const {
    DCHECK(style_);
    return *style_;
  }

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
  const InterpolationTypesMap& interpolation_types_map_;

  // CSSInterpolationType environment
  StyleResolverState* state_ = nullptr;
  const ComputedStyle* style_ = nullptr;

  // SVGInterpolationType environment
  Member<SVGElement> svg_element_ = nullptr;
  Member<const SVGPropertyBase> svg_base_value_ = nullptr;
};

}  // namespace blink

#endif  // InterpolationEnvironment_h
