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
class CSSNumericValue;

// Represents a rotation value in a CSSTransformValue used for properties like
// "transform".
// See CSSRotation.idl for more information about this class.
class CORE_EXPORT CSSRotation final : public CSSTransformComponent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructors defined in the IDL.
  static CSSRotation* Create(CSSNumericValue* angle, ExceptionState&);
  static CSSRotation* Create(const CSSNumberish& x,
                             const CSSNumberish& y,
                             const CSSNumberish& z,
                             CSSNumericValue* angle,
                             ExceptionState&);

  // Blink-internal ways of creating CSSRotations.
  static CSSRotation* Create(CSSNumericValue* angle);
  static CSSRotation* Create(CSSNumericValue* x,
                             CSSNumericValue* y,
                             CSSNumericValue* z,
                             CSSNumericValue* angle);
  static CSSRotation* FromCSSValue(const CSSFunctionValue&);

  // Getters and setters for attributes defined in the IDL.
  CSSNumericValue* angle() { return angle_.Get(); }
  void setAngle(CSSNumericValue* angle, ExceptionState&);
  void x(CSSNumberish& x) { x.SetCSSNumericValue(x_); }
  void y(CSSNumberish& y) { y.SetCSSNumericValue(y_); }
  void z(CSSNumberish& z) { z.SetCSSNumericValue(z_); }
  void setX(const CSSNumberish&, ExceptionState&);
  void setY(const CSSNumberish&, ExceptionState&);
  void setZ(const CSSNumberish&, ExceptionState&);

  // Internal methods - from CSSTransformComponent.
  TransformComponentType GetType() const final { return kRotationType; }
  const DOMMatrix* AsMatrix(ExceptionState&) const final;
  const CSSFunctionValue* ToCSSValue() const final;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(angle_);
    visitor->Trace(x_);
    visitor->Trace(y_);
    visitor->Trace(z_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSRotation(CSSNumericValue* x,
              CSSNumericValue* y,
              CSSNumericValue* z,
              CSSNumericValue* angle,
              bool is2D);

  Member<CSSNumericValue> angle_;
  Member<CSSNumericValue> x_;
  Member<CSSNumericValue> y_;
  Member<CSSNumericValue> z_;
  DISALLOW_COPY_AND_ASSIGN(CSSRotation);
};

}  // namespace blink

#endif
