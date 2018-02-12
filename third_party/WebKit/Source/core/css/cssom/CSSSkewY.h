// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSkewY_h
#define CSSSkewY_h

#include "base/macros.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class DOMMatrix;
class ExceptionState;

// Represents a skewY value in a CSSTransformValue used for properties like
// "transform".
// See CSSSkewY.idl for more information about this class.
class CORE_EXPORT CSSSkewY final : public CSSTransformComponent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructor defined in the IDL.
  static CSSSkewY* Create(CSSNumericValue*, ExceptionState&);
  static CSSSkewY* Create(CSSNumericValue* ay) { return new CSSSkewY(ay); }

  // Internal ways of creating CSSSkewY.
  static CSSSkewY* FromCSSValue(const CSSFunctionValue&);

  // Getters and setters for the ay attributes defined in the IDL.
  CSSNumericValue* ay() { return ay_.Get(); }
  void setAy(CSSNumericValue*, ExceptionState&);

  DOMMatrix* toMatrix(ExceptionState&) const final;

  // From CSSTransformComponent
  // Setting is2D for CSSSkewY does nothing.
  // https://drafts.css-houdini.org/css-typed-om/#dom-cssskew-is2d
  void setIs2D(bool is2D) final {}

  // Internal methods - from CSSTransformComponent.
  TransformComponentType GetType() const override { return kSkewYType; }
  const CSSFunctionValue* ToCSSValue() const override;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(ay_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSSkewY(CSSNumericValue* ay);

  Member<CSSNumericValue> ay_;
  DISALLOW_COPY_AND_ASSIGN(CSSSkewY);
};

}  // namespace blink

#endif
