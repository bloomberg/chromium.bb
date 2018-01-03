// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTransformValue_h
#define CSSTransformValue_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSTransformComponent.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {

class DOMMatrix;

class CORE_EXPORT CSSTransformValue final : public CSSStyleValue {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSTransformValue* Create(
      const HeapVector<Member<CSSTransformComponent>>& transform_components,
      ExceptionState&);

  // Blink-internal constructor
  static CSSTransformValue* Create(
      const HeapVector<Member<CSSTransformComponent>>& transform_components);

  static CSSTransformValue* FromCSSValue(const CSSValue&);

  bool is2D() const;

  DOMMatrix* toMatrix(ExceptionState&) const;

  const CSSValue* ToCSSValue() const override;

  StyleValueType GetType() const override { return kTransformType; }

  CSSTransformComponent* componentAtIndex(uint32_t index) {
    return transform_components_.at(index);
  }

  size_t length() const { return transform_components_.size(); }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(transform_components_);
    CSSStyleValue::Trace(visitor);
  }

 private:
  CSSTransformValue(
      const HeapVector<Member<CSSTransformComponent>>& transform_components)
      : CSSStyleValue(), transform_components_(transform_components) {}

  HeapVector<Member<CSSTransformComponent>> transform_components_;
  DISALLOW_COPY_AND_ASSIGN(CSSTransformValue);
};

}  // namespace blink

#endif  // CSSTransformValue_h
