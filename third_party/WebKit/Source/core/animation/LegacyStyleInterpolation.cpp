// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/LegacyStyleInterpolation.h"

#include <memory>

namespace blink {

namespace {

bool typesMatch(const InterpolableValue* start, const InterpolableValue* end) {
  if (start == end)
    return true;
  if (start->isNumber())
    return end->isNumber();
  if (start->isBool())
    return end->isBool();
  if (start->isAnimatableValue())
    return end->isAnimatableValue();
  if (!(start->isList() && end->isList()))
    return false;
  const InterpolableList* startList = toInterpolableList(start);
  const InterpolableList* endList = toInterpolableList(end);
  if (startList->length() != endList->length())
    return false;
  for (size_t i = 0; i < startList->length(); ++i) {
    if (!typesMatch(startList->get(i), endList->get(i)))
      return false;
  }
  return true;
}

}  // namespace

LegacyStyleInterpolation::LegacyStyleInterpolation(
    std::unique_ptr<InterpolableValue> start,
    std::unique_ptr<InterpolableValue> end,
    CSSPropertyID id)
    : Interpolation(),
      m_start(std::move(start)),
      m_end(std::move(end)),
      m_property(id),
      m_cachedFraction(0),
      m_cachedIteration(0),
      m_cachedValue(m_start ? m_start->clone() : nullptr) {
  RELEASE_ASSERT(typesMatch(m_start.get(), m_end.get()));
}

void LegacyStyleInterpolation::interpolate(int iteration, double fraction) {
  if (m_cachedFraction != fraction || m_cachedIteration != iteration) {
    m_start->interpolate(*m_end, fraction, *m_cachedValue);
    m_cachedIteration = iteration;
    m_cachedFraction = fraction;
  }
}

}  // namespace blink
