// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"

namespace blink {

void PropertyRegistry::RegisterProperty(const AtomicString& name,
                                        PropertyRegistration& registration) {
  DCHECK(!Registration(name));
  registrations_.Set(name, &registration);
}

const PropertyRegistration* PropertyRegistry::Registration(
    const AtomicString& name) const {
  return registrations_.at(name);
}

const CSSValue* PropertyRegistry::ParseIfRegistered(
    const Document& document,
    const AtomicString& property_name,
    const CSSValue* value) {
  if (!value)
    return nullptr;

  const PropertyRegistry* registry = document.GetPropertyRegistry();

  if (!registry)
    return value;

  const PropertyRegistration* registration =
      registry->Registration(property_name);

  if (!registration)
    return value;
  if (!value->IsCustomPropertyDeclaration())
    return value;

  CSSVariableData* tokens = ToCSSCustomPropertyDeclaration(value)->Value();

  if (!tokens || tokens->NeedsVariableResolution())
    return value;

  const CSSValue* parsed_value = tokens->ParseForSyntax(
      registration->Syntax(), document.GetSecureContextMode());

  return parsed_value ? parsed_value : value;
}

}  // namespace blink
