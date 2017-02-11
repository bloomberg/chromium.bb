// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSTransformValue.h"

#include "core/css/CSSValueList.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

CSSTransformValue* CSSTransformValue::fromCSSValue(const CSSValue& cssValue) {
  if (!cssValue.isValueList()) {
    // TODO(meade): Also need to check the separator here if we care.
    return nullptr;
  }
  HeapVector<Member<CSSTransformComponent>> components;
  for (const CSSValue* value : toCSSValueList(cssValue)) {
    CSSTransformComponent* component =
        CSSTransformComponent::fromCSSValue(*value);
    if (!component)
      return nullptr;
    components.push_back(component);
  }
  return CSSTransformValue::create(components);
}

bool CSSTransformValue::is2D() const {
  for (size_t i = 0; i < m_transformComponents.size(); i++) {
    if (!m_transformComponents[i]->is2D()) {
      return false;
    }
  }
  return true;
}

const CSSValue* CSSTransformValue::toCSSValue() const {
  CSSValueList* transformCSSValue = CSSValueList::createSpaceSeparated();
  for (size_t i = 0; i < m_transformComponents.size(); i++) {
    transformCSSValue->append(*m_transformComponents[i]->toCSSValue());
  }
  return transformCSSValue;
}

}  // namespace blink
