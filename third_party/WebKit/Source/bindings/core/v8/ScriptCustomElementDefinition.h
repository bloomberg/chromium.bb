// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptCustomElementDefinition_h
#define ScriptCustomElementDefinition_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/CoreExport.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "v8.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class CustomElementDescriptor;
class CustomElementsRegistry;

class CORE_EXPORT ScriptCustomElementDefinition final :
    public CustomElementDefinition {
    WTF_MAKE_NONCOPYABLE(ScriptCustomElementDefinition);
public:
    static ScriptCustomElementDefinition* forConstructor(
        ScriptState*,
        CustomElementsRegistry*,
        const v8::Local<v8::Value>& constructor);

    static ScriptCustomElementDefinition* create(
        ScriptState*,
        CustomElementsRegistry*,
        const CustomElementDescriptor&,
        const v8::Local<v8::Object>& constructor,
        const v8::Local<v8::Object>& prototype,
        const v8::Local<v8::Object>& connectedCallback,
        const v8::Local<v8::Object>& disconnectedCallback,
        const v8::Local<v8::Object>& attributeChangedCallback,
        const HashSet<AtomicString>& observedAttributes);

    virtual ~ScriptCustomElementDefinition() = default;

    v8::Local<v8::Object> constructor() const;
    v8::Local<v8::Object> prototype() const;

private:
    ScriptCustomElementDefinition(
        ScriptState*,
        const CustomElementDescriptor&,
        const v8::Local<v8::Object>& constructor,
        const v8::Local<v8::Object>& prototype,
        const v8::Local<v8::Object>& connectedCallback,
        const v8::Local<v8::Object>& disconnectedCallback,
        const v8::Local<v8::Object>& attributeChangedCallback,
        const HashSet<AtomicString>& observedAttributes);

    // Implementations of |CustomElementDefinition|
    ScriptValue getConstructorForScript() final;
    bool runConstructor(Element*) override;

    RefPtr<ScriptState> m_scriptState;
    ScopedPersistent<v8::Object> m_constructor;
    ScopedPersistent<v8::Object> m_prototype;
    ScopedPersistent<v8::Object> m_connectedCallback;
    ScopedPersistent<v8::Object> m_disconnectedCallback;
    ScopedPersistent<v8::Object> m_attributeChangedCallback;
    HashSet<AtomicString> m_observedAttributes;
};

} // namespace blink

#endif // ScriptCustomElementDefinition_h
