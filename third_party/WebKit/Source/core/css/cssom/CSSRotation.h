// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSRotation_h
#define CSSRotation_h

#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class DOMMatrix;

// Represents a rotation value in a CSSTransformValue used for properties like
// "transform".
// See CSSRotation.idl for more information about this class.
class CORE_EXPORT CSSRotation final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSRotation);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructors defined in the IDL.
  static CSSRotation* Create(CSSNumericValue* angle,
                             ExceptionState& exception_state) {
    return Create(0, 0, 1, angle, exception_state);
  }
  static CSSRotation* Create(double x,
                             double y,
                             double z,
                             CSSNumericValue* angle,
                             ExceptionState&);

  // Blink-internal ways of creating CSSRotations.
  static CSSRotation* Create(CSSNumericValue* angle) {
    return Create(0, 0, 1, angle);
  }
  static CSSRotation* Create(double x,
                             double y,
                             double z,
                             CSSNumericValue* angle);
  static CSSRotation* FromCSSValue(const CSSFunctionValue&);

  // Getters and setters for attributes defined in the IDL.
  // Bindings requires returning non-const pointers. This is safe because
  // CSSNumericValues are immutable.
  CSSNumericValue* angle() const {
    return const_cast<CSSNumericValue*>(angle_.Get());
  }
  double x() const { return x_; }
  double y() const { return y_; }
  double z() const { return z_; }

  // Internal methods - from CSSTransformComponent.
  TransformComponentType GetType() const override {
    return is2d_ ? kRotationType : kRotation3DType;
  }
  DOMMatrix* AsMatrix() const override {
    return nullptr;
    // TODO(meade): Implement.
    // return is2d_
    //           ? CSSMatrixComponent::Rotate(
    //              angle_->to(CSSPrimitiveValue::UnitType::kDegrees)->value())
    //           : CSSMatrixComponent::Rotate3d(
    //              angle_->to(CSSPrimitiveValue::UnitType::kDegrees)->value(),
    //                 x_, y_, z_);
  }
  CSSFunctionValue* ToCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(angle_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSRotation(const CSSNumericValue* angle)
      : angle_(angle), x_(0), y_(0), z_(1), is2d_(true) {}

  CSSRotation(double x, double y, double z, const CSSNumericValue* angle)
      : angle_(angle), x_(x), y_(y), z_(z), is2d_(false) {}

  Member<const CSSNumericValue> angle_;
  double x_;
  double y_;
  double z_;
  bool is2d_;
};

}  // namespace blink

#endif
