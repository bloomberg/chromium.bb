// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDefinition_h
#define CustomElementDefinition_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "core/dom/custom/CustomElementDescriptor.h"
#include "core/dom/custom/CustomElementsRegistry.h"
#include "platform/heap/Handle.h"
#include "v8.h"

namespace blink {

class CustomElementDefinition final
    : public GarbageCollectedFinalized<CustomElementDefinition> {
public:
    CustomElementDefinition(
        CustomElementsRegistry*,
        CustomElementsRegistry::Id,
        const CustomElementDescriptor&);

    CustomElementsRegistry::Id id() const { return m_id; }
    const CustomElementDescriptor& descriptor() { return m_descriptor; }
    v8::Local<v8::Object> prototype(ScriptState*) const;

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_registry);
    }

private:
    Member<CustomElementsRegistry> m_registry;
    CustomElementsRegistry::Id m_id;
    CustomElementDescriptor m_descriptor;
};

} // namespace blink

#endif // CustomElementDefinition_h
