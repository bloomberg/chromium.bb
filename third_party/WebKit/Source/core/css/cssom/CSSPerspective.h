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

class CORE_EXPORT CSSPerspective : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSPerspective);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSPerspective* Create(const CSSNumericValue*, ExceptionState&);
  static CSSPerspective* FromCSSValue(const CSSFunctionValue&);

  // Bindings require a non const return value.
  CSSNumericValue* length() const {
    return const_cast<CSSNumericValue*>(length_.Get());
  }

  TransformComponentType GetType() const override { return kPerspectiveType; }

  // TODO: Implement AsMatrix for CSSPerspective.
  DOMMatrix* AsMatrix() const override { return nullptr; }

  CSSFunctionValue* ToCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(length_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSPerspective(const CSSNumericValue* length) : length_(length) {}

  Member<const CSSNumericValue> length_;
};

}  // namespace blink

#endif
