// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"

#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/core/css/property_registry.h"

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

}  // namespace blink
