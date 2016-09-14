// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is for functions that are used only by generated code.
// CAUTION:
// All functions defined in this file should be used by generated code only.
// If you want to use them from hand-written code, please find appropriate
// location and move them to that location.

#ifndef GeneratedCodeHelper_h
#define GeneratedCodeHelper_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/CoreExport.h"
#include "wtf/PassRefPtr.h"
#include <v8.h>

namespace blink {

class ScriptState;
class SerializedScriptValue;

CORE_EXPORT void v8ConstructorAttributeGetter(v8::Local<v8::Name> propertyName, const v8::PropertyCallbackInfo<v8::Value>&);

CORE_EXPORT v8::Local<v8::Value> v8Deserialize(v8::Isolate*, PassRefPtr<SerializedScriptValue>);

// ExceptionToRejectPromiseScope converts a possible exception to a reject
// promise and returns the promise instead of throwing the exception.
//
// Promise-returning DOM operations are required to always return a promise
// and to never throw an exception.
// See also http://heycam.github.io/webidl/#es-operations
class CORE_EXPORT ExceptionToRejectPromiseScope {
    STACK_ALLOCATED();
public:
    ExceptionToRejectPromiseScope(const v8::FunctionCallbackInfo<v8::Value>& info, ScriptState* scriptState, ExceptionState& exceptionState)
        : m_info(info)
        , m_scriptState(scriptState)
        , m_exceptionState(exceptionState) { }
    ~ExceptionToRejectPromiseScope()
    {
        if (!m_exceptionState.hadException())
            return;

        v8SetReturnValue(m_info, m_exceptionState.reject(m_scriptState).v8Value());
    }

private:
    const v8::FunctionCallbackInfo<v8::Value>& m_info;
    ScriptState* m_scriptState;
    ExceptionState& m_exceptionState;
};

} // namespace blink

#endif // GeneratedCodeHelper_h
