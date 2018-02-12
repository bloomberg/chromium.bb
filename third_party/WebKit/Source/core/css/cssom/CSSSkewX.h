// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSkewX_h
#define CSSSkewX_h

#include "base/macros.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class DOMMatrix;
class ExceptionState;

// Represents a skewX value in a CSSTransformValue used for properties like
// "transform".
// See CSSSkewX.idl for more information about this class.
class CORE_EXPORT CSSSkewX final : public CSSTransformComponent {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructor defined in the IDL.
  static CSSSkewX* Create(CSSNumericValue*, ExceptionState&);
  static CSSSkewX* Create(CSSNumericValue* ax) { return new CSSSkewX(ax); }

  // Internal ways of creating CSSSkewX.
  static CSSSkewX* FromCSSValue(const CSSFunctionValue&);

  // Getters and setters for the ax attributes defined in the IDL.
  CSSNumericValue* ax() { return ax_.Get(); }
  void setAx(CSSNumericValue*, ExceptionState&);

  DOMMatrix* toMatrix(ExceptionState&) const final;

  // From CSSTransformComponent
  // Setting is2D for CSSSkewX does nothing.
  // https://drafts.css-houdini.org/css-typed-om/#dom-cssskew-is2d
  void setIs2D(bool is2D) final {}

  // Internal methods - from CSSTransformComponent.
  TransformComponentType GetType() const override { return kSkewXType; }
  const CSSFunctionValue* ToCSSValue() const override;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(ax_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSSkewX(CSSNumericValue* ax);

  Member<CSSNumericValue> ax_;
  DISALLOW_COPY_AND_ASSIGN(CSSSkewX);
};

}  // namespace blink

#endif
