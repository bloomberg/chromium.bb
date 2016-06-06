// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptCustomElementDefinitionBuilder_h
#define ScriptCustomElementDefinitionBuilder_h

#include "core/CoreExport.h"
#include "core/dom/custom/CustomElementDefinitionBuilder.h"
#include "platform/heap/Handle.h"
#include "v8.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"

namespace blink {

class CustomElementsRegistry;
class ExceptionState;
class ScriptState;
class ScriptValue;

class CORE_EXPORT ScriptCustomElementDefinitionBuilder
    : public CustomElementDefinitionBuilder {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(ScriptCustomElementDefinitionBuilder);
public:
    ScriptCustomElementDefinitionBuilder(
        ScriptState*,
        CustomElementsRegistry*,
        const ScriptValue& constructorScriptValue,
        ExceptionState&);
    ~ScriptCustomElementDefinitionBuilder();

    bool checkConstructorIntrinsics() override;
    bool checkConstructorNotRegistered() override;
    bool checkPrototype() override;
    CustomElementDefinition* build(const CustomElementDescriptor&) override;

private:
    static ScriptCustomElementDefinitionBuilder* s_stack;

    ScriptCustomElementDefinitionBuilder* m_prev;
    RefPtr<ScriptState> m_scriptState;
    Member<CustomElementsRegistry> m_registry;
    v8::Local<v8::Value> m_constructorValue;
    v8::Local<v8::Object> m_constructor;
    v8::Local<v8::Object> m_prototype;
    ExceptionState& m_exceptionState;
};

} // namespace blink

#endif // ScriptCustomElementDefinitionBuilder_h
