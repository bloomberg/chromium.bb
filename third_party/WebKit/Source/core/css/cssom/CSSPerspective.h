// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPerspective_h
#define CSSPerspective_h

#include "core/CoreExport.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class DOMMatrix;
class ExceptionState;

// Represents a perspective value in a CSSTransformValue used for properties
// like "transform".
// See CSSPerspective.idl for more information about this class.
class CORE_EXPORT CSSPerspective final : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSPerspective);
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructor defined in the IDL.
  static CSSPerspective* Create(CSSNumericValue*, ExceptionState&);

  // Blink-internal ways of creating CSSPerspectives.
  static CSSPerspective* FromCSSValue(const CSSFunctionValue&);

  // Getters and setters for attributes defined in the IDL.
  CSSNumericValue* length() { return length_.Get(); }
  void setLength(CSSNumericValue*, ExceptionState&);

  // From CSSTransformComponent
  // Setting is2D for CSSPerspective does nothing.
  // https://drafts.css-houdini.org/css-typed-om/#dom-cssskew-is2d
  void setIs2D(bool is2D) final {}

  // Internal methods - from CSSTransformComponent.
  TransformComponentType GetType() const final { return kPerspectiveType; }
  const DOMMatrix* AsMatrix(ExceptionState&) const final;
  const CSSFunctionValue* ToCSSValue() const final;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(length_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSPerspective(CSSNumericValue* length)
      : CSSTransformComponent(false /* is2D */), length_(length) {}

  Member<CSSNumericValue> length_;
};

}  // namespace blink

#endif
