// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTransformValue_h
#define CSSTransformValue_h

#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSTransformComponent.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {

class DOMMatrix;

class CORE_EXPORT CSSTransformValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSTransformValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSTransformValue* Create() { return new CSSTransformValue(); }

  static CSSTransformValue* Create(
      const HeapVector<Member<CSSTransformComponent>>& transform_components) {
    return new CSSTransformValue(transform_components);
  }

  static CSSTransformValue* FromCSSValue(const CSSValue&);

  bool is2D() const;

  DOMMatrix* toMatrix(ExceptionState&) const;

  const CSSValue* ToCSSValue() const override;

  StyleValueType GetType() const override { return kTransformType; }

  CSSTransformComponent* componentAtIndex(uint32_t index) {
    return transform_components_.at(index);
  }

  size_t length() const { return transform_components_.size(); }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(transform_components_);
    CSSStyleValue::Trace(visitor);
  }

 private:
  CSSTransformValue() {}
  CSSTransformValue(
      const HeapVector<Member<CSSTransformComponent>>& transform_components)
      : CSSStyleValue(), transform_components_(transform_components) {}

  HeapVector<Member<CSSTransformComponent>> transform_components_;
};

}  // namespace blink

#endif  // CSSTransformValue_h
