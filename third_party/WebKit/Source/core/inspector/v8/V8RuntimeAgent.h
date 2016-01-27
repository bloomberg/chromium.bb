// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8RuntimeAgent_h
#define V8RuntimeAgent_h

#include "core/CoreExport.h"
#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

#include <v8.h>

namespace blink {

class InjectedScript;
class InjectedScriptManager;
class JSONArray;
class JSONObject;
class V8Debugger;

typedef String ErrorString;

class CORE_EXPORT V8RuntimeAgent {
public:
    static PassOwnPtr<V8RuntimeAgent> create(InjectedScriptManager*, V8Debugger*);
    virtual ~V8RuntimeAgent() { }

    // State management methods.
    virtual void setInspectorState(PassRefPtr<JSONObject>) = 0;
    virtual void setFrontend(InspectorFrontend::Runtime*) = 0;
    virtual void clearFrontend() = 0;
    virtual void restore() = 0;

    // Part of the protocol.
    virtual void enable(ErrorString*) = 0;
    virtual void disable(ErrorString*) = 0;

    virtual void evaluate(ErrorString*,
        const String& expression,
        const String* objectGroup,
        const bool* includeCommandLineAPI,
        const bool* doNotPauseOnExceptionsAndMuteConsole,
        const int* executionContextId,
        const bool* returnByValue,
        const bool* generatePreview,
        RefPtr<TypeBuilder::Runtime::RemoteObject>& result,
        TypeBuilder::OptOutput<bool>* wasThrown,
        RefPtr<TypeBuilder::Debugger::ExceptionDetails>&) = 0;
    virtual void callFunctionOn(ErrorString*,
        const String& objectId,
        const String& expression,
        const RefPtr<JSONArray>* optionalArguments,
        const bool* doNotPauseOnExceptionsAndMuteConsole,
        const bool* returnByValue,
        const bool* generatePreview,
        RefPtr<TypeBuilder::Runtime::RemoteObject>& result,
        TypeBuilder::OptOutput<bool>* wasThrown) = 0;
    virtual void releaseObject(ErrorString*, const String& objectId) = 0;
    virtual void getProperties(ErrorString*, const String& objectId, const bool* ownProperties, const bool* accessorPropertiesOnly, const bool* generatePreview, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::PropertyDescriptor>>& result, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::InternalPropertyDescriptor>>& internalProperties, RefPtr<TypeBuilder::Debugger::ExceptionDetails>&) = 0;
    virtual void releaseObjectGroup(ErrorString*, const String& objectGroup) = 0;
    virtual void run(ErrorString*) = 0;
    virtual void isRunRequired(ErrorString*, bool* out_result) = 0;
    virtual void setCustomObjectFormatterEnabled(ErrorString*, bool) = 0;

    // Embedder callbacks.
    virtual void reportExecutionContextCreated(v8::Local<v8::Context>, const String& type, const String& origin, const String& humanReadableName, const String& frameId) = 0;
    virtual void reportExecutionContextDestroyed(v8::Local<v8::Context>) = 0;
};

} // namespace blink

#endif // V8RuntimeAgent_h
