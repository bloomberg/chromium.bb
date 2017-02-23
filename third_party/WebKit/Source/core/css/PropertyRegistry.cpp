// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/PropertyRegistry.h"

namespace blink {

void PropertyRegistry::registerProperty(
    const AtomicString& name,
    const CSSSyntaxDescriptor& syntax,
    bool inherits,
    const CSSValue* initial,
    PassRefPtr<CSSVariableData> initialVariableData,
    CSSInterpolationTypes cssInterpolationTypes) {
  DCHECK(!registration(name));
  m_registrations.set(
      name, new PropertyRegistration(syntax, inherits, initial,
                                     std::move(initialVariableData),
                                     std::move(cssInterpolationTypes)));
}

const PropertyRegistration* PropertyRegistry::registration(
    const AtomicString& name) const {
  return m_registrations.at(name);
}

}  // namespace blink
