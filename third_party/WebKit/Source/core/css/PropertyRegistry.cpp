// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/PropertyRegistry.h"

namespace blink {

void PropertyRegistry::RegisterProperty(
    const AtomicString& name,
    const CSSSyntaxDescriptor& syntax,
    bool inherits,
    const CSSValue* initial,
    PassRefPtr<CSSVariableData> initial_variable_data,
    CSSInterpolationTypes css_interpolation_types) {
  DCHECK(!Registration(name));
  registrations_.Set(
      name, new PropertyRegistration(syntax, inherits, initial,
                                     std::move(initial_variable_data),
                                     std::move(css_interpolation_types)));
}

const PropertyRegistration* PropertyRegistry::Registration(
    const AtomicString& name) const {
  return registrations_.at(name);
}

}  // namespace blink
