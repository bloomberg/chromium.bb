// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSInterpolationType_h
#define CSSInterpolationType_h

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/InterpolationType.h"

namespace blink {

class CSSCustomPropertyDeclaration;
class PropertyRegistration;

class CSSInterpolationType : public InterpolationType {
 public:
  void setCustomPropertyRegistration(const PropertyRegistration&);

 protected:
  CSSInterpolationType(PropertyHandle);

  CSSPropertyID cssProperty() const { return getProperty().cssProperty(); }

  InterpolationValue maybeConvertSingle(const PropertySpecificKeyframe&,
                                        const InterpolationEnvironment&,
                                        const InterpolationValue& underlying,
                                        ConversionCheckers&) const final;
  virtual InterpolationValue maybeConvertNeutral(
      const InterpolationValue& underlying,
      ConversionCheckers&) const = 0;
  virtual InterpolationValue maybeConvertInitial(const StyleResolverState&,
                                                 ConversionCheckers&) const = 0;
  virtual InterpolationValue maybeConvertInherit(const StyleResolverState&,
                                                 ConversionCheckers&) const = 0;
  virtual InterpolationValue maybeConvertValue(const CSSValue&,
                                               const StyleResolverState*,
                                               ConversionCheckers&) const = 0;
  virtual void additiveKeyframeHook(InterpolationValue&) const {}

  InterpolationValue maybeConvertUnderlyingValue(
      const InterpolationEnvironment&) const final;
  virtual InterpolationValue maybeConvertStandardPropertyUnderlyingValue(
      const ComputedStyle&) const = 0;

  void apply(const InterpolableValue&,
             const NonInterpolableValue*,
             InterpolationEnvironment&) const final;
  virtual void applyStandardPropertyValue(const InterpolableValue&,
                                          const NonInterpolableValue*,
                                          StyleResolverState&) const = 0;

 private:
  InterpolationValue maybeConvertSingleInternal(
      const PropertySpecificKeyframe&,
      const InterpolationEnvironment&,
      const InterpolationValue& underlying,
      ConversionCheckers&) const;

  InterpolationValue maybeConvertCustomPropertyDeclaration(
      const CSSCustomPropertyDeclaration&,
      const StyleResolverState&,
      ConversionCheckers&) const;
  InterpolationValue maybeConvertCustomPropertyDeclarationInternal(
      const CSSCustomPropertyDeclaration&,
      const StyleResolverState&,
      ConversionCheckers&) const;

  virtual const CSSValue* createCSSValue(const InterpolableValue&,
                                         const NonInterpolableValue*,
                                         const StyleResolverState&) const {
    // TODO(alancutter): Implement this for all subclasses and make this an
    // abstract declaration so the return type can be changed to
    // const CSSValue&.
    NOTREACHED();
    return nullptr;
  }

  void applyCustomPropertyValue(const InterpolableValue&,
                                const NonInterpolableValue*,
                                StyleResolverState&) const;

  WeakPersistent<const PropertyRegistration> m_registration;
};

}  // namespace blink

#endif  // CSSInterpolationType_h
