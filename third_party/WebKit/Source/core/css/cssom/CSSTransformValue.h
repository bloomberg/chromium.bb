// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTransformValue_h
#define CSSTransformValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSTransformComponent.h"
#include "platform/heap/HeapAllocator.h"

namespace blink {

class CORE_EXPORT CSSTransformValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSTransformValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSTransformValue* create() { return new CSSTransformValue(); }

  static CSSTransformValue* create(
      const HeapVector<Member<CSSTransformComponent>>& transformComponents) {
    return new CSSTransformValue(transformComponents);
  }

  static CSSTransformValue* fromCSSValue(const CSSValue&);

  bool is2D() const;

  const CSSValue* toCSSValue() const override;

  StyleValueType type() const override { return TransformType; }

  CSSTransformComponent* componentAtIndex(uint32_t index) {
    return m_transformComponents.at(index);
  }

  size_t length() const { return m_transformComponents.size(); }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_transformComponents);
    CSSStyleValue::trace(visitor);
  }

 private:
  CSSTransformValue() {}
  CSSTransformValue(
      const HeapVector<Member<CSSTransformComponent>>& transformComponents)
      : CSSStyleValue(), m_transformComponents(transformComponents) {}

  HeapVector<Member<CSSTransformComponent>> m_transformComponents;
};

}  // namespace blink

#endif  // CSSTransformValue_h
