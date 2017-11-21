// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSRotation_h
#define CSSRotation_h

#include "base/macros.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class DOMMatrix;
class ExceptionState;

// Represents a rotation value in a CSSTransformValue used for properties like
// "transform".
// See CSSRotation.idl for more information about this class.
class CORE_EXPORT CSSRotation final : public CSSTransformComponent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructors defined in the IDL.
  static CSSRotation* Create(CSSNumericValue* angle, ExceptionState&);
  static CSSRotation* Create(double x,
                             double y,
                             double z,
                             CSSNumericValue* angle,
                             ExceptionState&);

  // Blink-internal ways of creating CSSRotations.
  static CSSRotation* Create(CSSNumericValue* angle);
  static CSSRotation* Create(double x,
                             double y,
                             double z,
                             CSSNumericValue* angle);
  static CSSRotation* FromCSSValue(const CSSFunctionValue&);

  // Getters and setters for attributes defined in the IDL.
  CSSNumericValue* angle() { return angle_.Get(); }
  double x() const { return x_; }
  double y() const { return y_; }
  double z() const { return z_; }
  void setAngle(CSSNumericValue* angle, ExceptionState&);
  void setX(double x) { x_ = x; }
  void setY(double y) { y_ = y; }
  void setZ(double z) { z_ = z; }

  // Internal methods - from CSSTransformComponent.
  TransformComponentType GetType() const final { return kRotationType; }
  const DOMMatrix* AsMatrix(ExceptionState&) const final;
  const CSSFunctionValue* ToCSSValue(SecureContextMode) const final;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(angle_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSRotation(double x, double y, double z, CSSNumericValue* angle, bool is2D)
      : CSSTransformComponent(is2D), angle_(angle), x_(x), y_(y), z_(z) {}

  Member<CSSNumericValue> angle_;
  double x_;
  double y_;
  double z_;
  DISALLOW_COPY_AND_ASSIGN(CSSRotation);
};

}  // namespace blink

#endif
