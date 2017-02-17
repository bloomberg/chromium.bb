// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TransitionInterpolation_h
#define TransitionInterpolation_h

#include "core/animation/CompositorAnimations.h"
#include "core/animation/Interpolation.h"
#include "core/animation/InterpolationType.h"

namespace blink {

class StyleResolverState;
class InterpolationType;

class TransitionInterpolation : public Interpolation {
 public:
  static PassRefPtr<TransitionInterpolation> create(
      const PropertyHandle& property,
      const InterpolationType& type,
      InterpolationValue&& start,
      InterpolationValue&& end,
      const RefPtr<AnimatableValue> compositorStart,
      const RefPtr<AnimatableValue> compositorEnd) {
    return adoptRef(new TransitionInterpolation(
        property, type, std::move(start), std::move(end),
        std::move(compositorStart), std::move(compositorEnd)));
  }

  void apply(StyleResolverState&) const;

  bool isTransitionInterpolation() const final { return true; }

  const PropertyHandle& getProperty() const final { return m_property; }

  std::unique_ptr<TypedInterpolationValue> getInterpolatedValue() const;

  RefPtr<AnimatableValue> getInterpolatedCompositorValue() const;

  void interpolate(int iteration, double fraction) final;

 protected:
  TransitionInterpolation(const PropertyHandle& property,
                          const InterpolationType& type,
                          InterpolationValue&& start,
                          InterpolationValue&& end,
                          const RefPtr<AnimatableValue> compositorStart,
                          const RefPtr<AnimatableValue> compositorEnd)
      : m_property(property),
        m_type(type),
        m_start(std::move(start)),
        m_end(std::move(end)),
        m_merge(type.maybeMergeSingles(m_start.clone(), m_end.clone())),
        m_compositorStart(std::move(compositorStart)),
        m_compositorEnd(std::move(compositorEnd)),
        m_cachedInterpolableValue(m_merge.startInterpolableValue->clone()) {
    DCHECK(m_merge);
    DCHECK_EQ(
        m_compositorStart && m_compositorEnd,
        CompositorAnimations::isCompositableProperty(m_property.cssProperty()));
  }

 private:
  const InterpolableValue& currentInterpolableValue() const;
  NonInterpolableValue* currentNonInterpolableValue() const;

  const PropertyHandle m_property;
  const InterpolationType& m_type;
  const InterpolationValue m_start;
  const InterpolationValue m_end;
  const PairwiseInterpolationValue m_merge;
  const RefPtr<AnimatableValue> m_compositorStart;
  const RefPtr<AnimatableValue> m_compositorEnd;

  mutable double m_cachedFraction = 0;
  mutable int m_cachedIteration = 0;
  mutable std::unique_ptr<InterpolableValue> m_cachedInterpolableValue;
};

DEFINE_TYPE_CASTS(TransitionInterpolation,
                  Interpolation,
                  value,
                  value->isTransitionInterpolation(),
                  value.isTransitionInterpolation());

}  // namespace blink

#endif
