// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_non_inherited_variables.h"

#include "third_party/blink/renderer/core/style/data_equivalency.h"

namespace blink {

bool StyleNonInheritedVariables::operator==(
    const StyleNonInheritedVariables& other) const {
  return variables_ == other.variables_;
}

CSSVariableData* StyleNonInheritedVariables::GetVariable(
    const AtomicString& name) const {
  return variables_.GetData(name).value_or(nullptr);
}

StyleVariables::OptionalData StyleNonInheritedVariables::GetData(
    const AtomicString& name) const {
  return variables_.GetData(name);
}

void StyleNonInheritedVariables::SetRegisteredVariable(
    const AtomicString& name,
    const CSSValue* parsed_value) {
  needs_resolution_ = true;
  variables_.SetValue(name, parsed_value);
}

StyleVariables::OptionalValue StyleNonInheritedVariables::GetValue(
    const AtomicString& name) const {
  return variables_.GetValue(name);
}

HashSet<AtomicString> StyleNonInheritedVariables::GetCustomPropertyNames()
    const {
  return variables_.GetNames();
}

}  // namespace blink
