// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

CustomProperty::CustomProperty(const AtomicString& name,
                               const Document& document)
    : name_(name), registration_(PropertyRegistration::From(&document, name)) {}

CustomProperty::CustomProperty(const AtomicString& name,
                               const PropertyRegistry* registry)
    : name_(name),
      registration_(registry ? registry->Registration(name) : nullptr) {}

bool CustomProperty::IsInherited() const {
  return !registration_ || registration_->Inherits();
}

const AtomicString& CustomProperty::GetPropertyNameAtomicString() const {
  return name_;
}

void CustomProperty::ApplyInitial(StyleResolverState& state) const {
  state.Style()->RemoveVariable(name_, IsInherited());
}

void CustomProperty::ApplyInherit(StyleResolverState& state) const {
  bool is_inherited_property = IsInherited();
  state.Style()->RemoveVariable(name_, is_inherited_property);

  CSSVariableData* parent_value =
      state.ParentStyle()->GetVariable(name_, is_inherited_property);

  if (!parent_value)
    return;

  state.Style()->SetVariable(name_, parent_value, is_inherited_property);

  if (registration_) {
    const CSSValue* parent_css_value =
        parent_value ? state.ParentStyle()->GetNonInitialRegisteredVariable(
                           name_, is_inherited_property)
                     : nullptr;
    state.Style()->SetRegisteredVariable(name_, parent_css_value,
                                         is_inherited_property);
  }
}

void CustomProperty::ApplyValue(StyleResolverState& state,
                                const CSSValue& value) const {
  const CSSCustomPropertyDeclaration& declaration =
      ToCSSCustomPropertyDeclaration(value);

  bool is_inherited_property = IsInherited();
  bool initial = declaration.IsInitial(is_inherited_property);
  bool inherit = declaration.IsInherit(is_inherited_property);
  DCHECK(!(initial && inherit));

  // TODO(andruud): Use regular initial/inherit dispatch in StyleBuilder
  //                once custom properties are Ribbonized.
  if (initial) {
    ApplyInitial(state);
  } else if (inherit) {
    ApplyInherit(state);
  } else {
    state.Style()->SetVariable(name_, declaration.Value(),
                               is_inherited_property);
    if (registration_) {
      state.Style()->SetRegisteredVariable(name_, nullptr,
                                           is_inherited_property);
    }
  }
}

const CSSValue* CustomProperty::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  using VariableMode = CSSParserLocalContext::VariableMode;

  switch (local_context.GetVariableMode()) {
    case VariableMode::kTyped:
      return ParseTyped(range, context, local_context);
    case VariableMode::kUntyped:
      return ParseUntyped(range, context, local_context);
    case VariableMode::kValidatedUntyped:
      if (registration_ && !ParseTyped(range, context, local_context))
        return nullptr;
      return ParseUntyped(range, context, local_context);
  }
}

const CSSValue* CustomProperty::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (registration_) {
    const CSSValue* value = style.GetRegisteredVariable(name_, IsInherited());
    if (value)
      return value;
    // If we don't have CSSValue for this registered property, it means that
    // that the property was not registered at the time |style| was calculated,
    // hence we proceed with unregistered behavior.
  }

  CSSVariableData* data = style.GetVariable(name_, IsInherited());

  if (!data)
    return nullptr;

  return CSSCustomPropertyDeclaration::Create(name_, data);
}

const CSSValue* CustomProperty::ParseUntyped(
    CSSParserTokenRange range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  return CSSVariableParser::ParseDeclarationValue(
      name_, range, local_context.IsAnimationTainted(), context);
}

const CSSValue* CustomProperty::ParseTyped(
    CSSParserTokenRange range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  if (!registration_)
    return ParseUntyped(range, context, local_context);
  return registration_->Syntax().Parse(range, &context,
                                       local_context.IsAnimationTainted());
}

}  // namespace blink
