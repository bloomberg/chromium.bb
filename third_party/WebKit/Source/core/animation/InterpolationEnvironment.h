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
      : m_interpolationTypesMap(map), m_state(&state), m_style(state.style()) {}

  explicit InterpolationEnvironment(const InterpolationTypesMap& map,
                                    const ComputedStyle& style)
      : m_interpolationTypesMap(map), m_style(&style) {}

  explicit InterpolationEnvironment(const InterpolationTypesMap& map,
                                    SVGElement& svgElement,
                                    const SVGPropertyBase& svgBaseValue)
      : m_interpolationTypesMap(map),
        m_svgElement(&svgElement),
        m_svgBaseValue(&svgBaseValue) {}

  const InterpolationTypesMap& interpolationTypesMap() const {
    return m_interpolationTypesMap;
  }

  StyleResolverState& state() {
    DCHECK(m_state);
    return *m_state;
  }
  const StyleResolverState& state() const {
    DCHECK(m_state);
    return *m_state;
  }

  const ComputedStyle& style() const {
    DCHECK(m_style);
    return *m_style;
  }

  SVGElement& svgElement() {
    DCHECK(m_svgElement);
    return *m_svgElement;
  }
  const SVGElement& svgElement() const {
    DCHECK(m_svgElement);
    return *m_svgElement;
  }

  const SVGPropertyBase& svgBaseValue() const {
    DCHECK(m_svgBaseValue);
    return *m_svgBaseValue;
  }

 private:
  const InterpolationTypesMap& m_interpolationTypesMap;

  // CSSInterpolationType environment
  StyleResolverState* m_state = nullptr;
  const ComputedStyle* m_style = nullptr;

  // SVGInterpolationType environment
  Member<SVGElement> m_svgElement = nullptr;
  Member<const SVGPropertyBase> m_svgBaseValue = nullptr;
};

}  // namespace blink

#endif  // InterpolationEnvironment_h
