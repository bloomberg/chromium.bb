// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/PropertyRegistry.h"

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

}  // namespace blink
