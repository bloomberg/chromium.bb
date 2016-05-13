// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDefinition_h
#define CustomElementDefinition_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "core/dom/custom/CustomElementsRegistry.h"
#include "platform/heap/Handle.h"
#include "v8.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class CustomElementDefinition final
    : public GarbageCollectedFinalized<CustomElementDefinition> {
public:
    CustomElementDefinition(
        CustomElementsRegistry*,
        CustomElementsRegistry::Id,
        const AtomicString& localName);

    CustomElementsRegistry::Id id() const { return m_id; }
    const AtomicString& localName() const { return m_localName; }
    v8::Local<v8::Object> prototype(ScriptState*) const;

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_registry);
    }

private:
    Member<CustomElementsRegistry> m_registry;
    CustomElementsRegistry::Id m_id;
    AtomicString m_localName;
};

} // namespace blink

#endif // CustomElementDefinition_h
