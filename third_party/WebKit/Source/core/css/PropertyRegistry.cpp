// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/PropertyRegistry.h"

namespace blink {

void PropertyRegistry::registerProperty(const AtomicString& name, const CSSSyntaxDescriptor& syntax, bool inherits, const CSSValue* initial)
{
    DCHECK(!registration(name));
    // TODO(timloh): We only support inherited properties for now.
    inherits = true;
    m_registrations.set(name, new Registration(syntax, inherits, initial));
}

void PropertyRegistry::unregisterProperty(const AtomicString& name)
{
    DCHECK(registration(name));
    m_registrations.remove(name);
}

const PropertyRegistry::Registration* PropertyRegistry::registration(const AtomicString& name) const
{
    return m_registrations.get(name);
}

} // namespace blink
