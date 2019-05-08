// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_inherited_variables.h"

#include "third_party/blink/renderer/core/style/data_equivalency.h"

namespace blink {

bool StyleInheritedVariables::operator==(
    const StyleInheritedVariables& other) const {
  return DataEquivalent(root_, other.root_) && variables_ == other.variables_;
}

StyleInheritedVariables::StyleInheritedVariables()
    : root_(nullptr), needs_resolution_(false) {}

StyleInheritedVariables::StyleInheritedVariables(StyleInheritedVariables& other)
    : needs_resolution_(other.needs_resolution_) {
  if (!other.root_) {
    root_ = &other;
  } else {
    variables_ = other.variables_;
    root_ = other.root_;
  }
}

CSSVariableData* StyleInheritedVariables::GetVariable(
    const AtomicString& name) const {
  return GetData(name).value_or(nullptr);
}

StyleVariables::OptionalData StyleInheritedVariables::GetData(
    const AtomicString& name) const {
  if (auto data = variables_.GetData(name))
    return *data;
  if (root_)
    return root_->variables_.GetData(name);
  return base::nullopt;
}

void StyleInheritedVariables::SetRegisteredVariable(
    const AtomicString& name,
    const CSSValue* parsed_value) {
  needs_resolution_ = true;
  variables_.SetValue(name, parsed_value);
}

const CSSValue* StyleInheritedVariables::RegisteredVariable(
    const AtomicString& name) const {
  return GetValue(name).value_or(nullptr);
}

StyleVariables::OptionalValue StyleInheritedVariables::GetValue(
    const AtomicString& name) const {
  if (auto data = variables_.GetValue(name))
    return *data;
  if (root_)
    return root_->variables_.GetValue(name);
  return base::nullopt;
}

HashSet<AtomicString> StyleInheritedVariables::GetCustomPropertyNames() const {
  HashSet<AtomicString> names;
  if (root_) {
    for (const auto& pair : root_->Data())
      names.insert(pair.key);
  }
  for (const auto& pair : Data())
    names.insert(pair.key);
  return names;
}

}  // namespace blink
