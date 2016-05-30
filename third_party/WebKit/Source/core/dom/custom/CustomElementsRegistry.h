// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementsRegistry_h
#define CustomElementsRegistry_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class CustomElementDefinition;
class CustomElementDefinitionBuilder;
class ElementRegistrationOptions;
class ExceptionState;
class ScriptState;
class ScriptValue;
class V0CustomElementRegistrationContext;

class CORE_EXPORT CustomElementsRegistry final
    : public GarbageCollectedFinalized<CustomElementsRegistry>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(CustomElementsRegistry);
public:
    static CustomElementsRegistry* create(
        V0CustomElementRegistrationContext*);

    void define(
        ScriptState*,
        const AtomicString& name,
        const ScriptValue& constructor,
        const ElementRegistrationOptions&,
        ExceptionState&);

    void define(
        const AtomicString& name,
        CustomElementDefinitionBuilder&,
        const ElementRegistrationOptions&,
        ExceptionState&);

    bool nameIsDefined(const AtomicString& name) const
    {
        return m_definitions.contains(name);
    }

    CustomElementDefinition* definitionForName(const AtomicString& name) const;

    DECLARE_TRACE();

private:
    CustomElementsRegistry(const V0CustomElementRegistrationContext*);
    bool v0NameIsDefined(const AtomicString&) const;

    using DefinitionMap =
        HeapHashMap<AtomicString, Member<CustomElementDefinition>>;
    DefinitionMap m_definitions;

    Member<const V0CustomElementRegistrationContext> m_v0;
};

} // namespace blink

#endif // CustomElementsRegistry_h
