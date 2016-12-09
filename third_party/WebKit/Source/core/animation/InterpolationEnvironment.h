// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InterpolationEnvironment_h
#define InterpolationEnvironment_h

#include "core/animation/InterpolationTypesMap.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"

namespace blink {

class StyleResolverState;
class SVGPropertyBase;
class SVGElement;

class InterpolationEnvironment {
  STACK_ALLOCATED();

 public:
  explicit InterpolationEnvironment(const InterpolationTypesMap& map,
                                    StyleResolverState& state)
      : m_interpolationTypesMap(map),
        m_state(&state),
        m_svgElement(nullptr),
        m_svgBaseValue(nullptr) {}

  explicit InterpolationEnvironment(const InterpolationTypesMap& map,
                                    SVGElement& svgElement,
                                    const SVGPropertyBase& svgBaseValue)
      : m_interpolationTypesMap(map),
        m_state(nullptr),
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
  StyleResolverState* m_state;
  Member<SVGElement> m_svgElement;
  Member<const SVGPropertyBase> m_svgBaseValue;
};

}  // namespace blink

#endif  // InterpolationEnvironment_h
