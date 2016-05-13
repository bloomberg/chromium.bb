// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementDefinition.h"

namespace blink {

CustomElementDefinition::CustomElementDefinition(
    CustomElementsRegistry* registry,
    CustomElementsRegistry::Id id,
    const AtomicString& localName)
    : m_registry(registry)
    , m_id(id)
    , m_localName(localName)
{
}

v8::Local<v8::Object> CustomElementDefinition::prototype(
    ScriptState* scriptState) const
{
    return m_registry->prototype(scriptState, *this);
}

} // namespace blink
