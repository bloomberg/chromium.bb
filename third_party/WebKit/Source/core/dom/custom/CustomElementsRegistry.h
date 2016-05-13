// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementsRegistry_h
#define CustomElementsRegistry_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "v8.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class CustomElementDefinition;
class ElementRegistrationOptions;
class ExceptionState;
class ScriptState;
class ScriptValue;
class V0CustomElementRegistrationContext;

class CORE_EXPORT CustomElementsRegistry final
    : public GarbageCollectedFinalized<CustomElementsRegistry>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    using Id = uint32_t;
    static CustomElementsRegistry* create(
        ScriptState*,
        V0CustomElementRegistrationContext*);

    void define(
        ScriptState*,
        const AtomicString& name,
        const ScriptValue& constructor,
        const ElementRegistrationOptions&,
        ExceptionState&);

    CustomElementDefinition* definitionForConstructor(
        ScriptState*,
        v8::Local<v8::Value> constructor);
    v8::Local<v8::Object> prototype(
        ScriptState*,
        const CustomElementDefinition&);

    // TODO(dominicc): Remove this when V0CustomElements are removed.
    bool nameIsDefined(const AtomicString& name) const;

    DECLARE_TRACE();

private:
    CustomElementsRegistry(const V0CustomElementRegistrationContext*);

    // Retrieves the Map which, given a constructor, contains the id
    // of the definition; or given an id, contains the prototype.
    // Returns true if the map was retrieved; false if the map was
    // not initialized yet.
    v8::Local<v8::Map> idMap(ScriptState*);

    bool idForConstructor(
        ScriptState*,
        v8::Local<v8::Value> constructor,
        Id&) WARN_UNUSED_RETURN;

    bool v0NameIsDefined(const AtomicString& name);

    Member<const V0CustomElementRegistrationContext> m_v0;
    HeapVector<Member<CustomElementDefinition>> m_definitions;
    HashSet<AtomicString> m_names;
};

} // namespace blink

#endif // CustomElementsRegistry_h
