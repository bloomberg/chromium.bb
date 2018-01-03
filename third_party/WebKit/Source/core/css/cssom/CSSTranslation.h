// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTranslation_h
#define CSSTranslation_h

#include "base/macros.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSTransformComponent.h"
#include "core/css/cssom/CSSUnitValue.h"

namespace blink {

class CSSNumericValue;
class DOMMatrix;
class ExceptionState;

// Represents a translation value in a CSSTransformValue used for properties
// like "transform".
// See CSSTranslation.idl for more information about this class.
class CORE_EXPORT CSSTranslation final : public CSSTransformComponent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructors defined in the IDL.
  static CSSTranslation* Create(CSSNumericValue* x,
                                CSSNumericValue* y,
                                CSSNumericValue* z,
                                ExceptionState&);
  static CSSTranslation* Create(CSSNumericValue* x,
                                CSSNumericValue* y,
                                ExceptionState&);

  // Blink-internal ways of creating CSSTranslations.
  static CSSTranslation* Create(CSSNumericValue* x, CSSNumericValue* y);
  static CSSTranslation* Create(CSSNumericValue* x,
                                CSSNumericValue* y,
                                CSSNumericValue* z);
  static CSSTranslation* FromCSSValue(const CSSFunctionValue&);

  // Getters and setters for attributes defined in the IDL.
  CSSNumericValue* x() { return x_; }
  CSSNumericValue* y() { return y_; }
  CSSNumericValue* z() { return z_; }
  void setX(CSSNumericValue* x, ExceptionState&);
  void setY(CSSNumericValue* y, ExceptionState&);
  void setZ(CSSNumericValue* z, ExceptionState&);

  // Internal methods - from CSSTransformComponent.
  TransformComponentType GetType() const final { return kTranslationType; }
  const DOMMatrix* AsMatrix(ExceptionState&) const final;
  const CSSFunctionValue* ToCSSValue() const final;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(x_);
    visitor->Trace(y_);
    visitor->Trace(z_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSTranslation(CSSNumericValue* x,
                 CSSNumericValue* y,
                 CSSNumericValue* z,
                 bool is2D);

  Member<CSSNumericValue> x_;
  Member<CSSNumericValue> y_;
  Member<CSSNumericValue> z_;
  DISALLOW_COPY_AND_ASSIGN(CSSTranslation);
};

}  // namespace blink

#endif
