// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPerspective_h
#define CSSPerspective_h

#include "core/CoreExport.h"
#include "core/css/cssom/CSSLengthValue.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT CSSPerspective : public CSSTransformComponent {
  WTF_MAKE_NONCOPYABLE(CSSPerspective);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSPerspective* Create(const CSSLengthValue*, ExceptionState&);
  static CSSPerspective* FromCSSValue(const CSSFunctionValue&);

  // Bindings require a non const return value.
  CSSLengthValue* length() const {
    return const_cast<CSSLengthValue*>(length_.Get());
  }

  TransformComponentType GetType() const override { return kPerspectiveType; }

  // TODO: Implement asMatrix for CSSPerspective.
  CSSMatrixComponent* asMatrix() const override { return nullptr; }

  CSSFunctionValue* ToCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(length_);
    CSSTransformComponent::Trace(visitor);
  }

 private:
  CSSPerspective(const CSSLengthValue* length) : length_(length) {}

  Member<const CSSLengthValue> length_;
};

}  // namespace blink

#endif
