// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"

#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

CustomProperty::CustomProperty(const AtomicString& name,
                               const Document& document)
    : name_(name), registration_(PropertyRegistration::From(&document, name)) {}

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

}  // namespace blink
