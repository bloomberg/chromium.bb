// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSTransformValue.h"

#include "core/css/CSSValueList.h"
#include "core/css/cssom/CSSTransformComponent.h"

namespace blink {

CSSTransformValue* CSSTransformValue::FromCSSValue(const CSSValue& css_value) {
  if (!css_value.IsValueList()) {
    // TODO(meade): Also need to check the separator here if we care.
    return nullptr;
  }
  HeapVector<Member<CSSTransformComponent>> components;
  for (const CSSValue* value : ToCSSValueList(css_value)) {
    CSSTransformComponent* component =
        CSSTransformComponent::FromCSSValue(*value);
    if (!component)
      return nullptr;
    components.push_back(component);
  }
  return CSSTransformValue::Create(components);
}

bool CSSTransformValue::is2D() const {
  for (size_t i = 0; i < transform_components_.size(); i++) {
    if (!transform_components_[i]->is2D()) {
      return false;
    }
  }
  return true;
}

const CSSValue* CSSTransformValue::ToCSSValue() const {
  CSSValueList* transform_css_value = CSSValueList::CreateSpaceSeparated();
  for (size_t i = 0; i < transform_components_.size(); i++) {
    transform_css_value->Append(*transform_components_[i]->ToCSSValue());
  }
  return transform_css_value;
}

}  // namespace blink
