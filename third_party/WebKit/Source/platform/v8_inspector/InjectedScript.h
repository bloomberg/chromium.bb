/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InjectedScript_h
#define InjectedScript_h

#include "platform/inspector_protocol/Allocator.h"
#include "platform/inspector_protocol/TypeBuilder.h"
#include "platform/v8_inspector/InjectedScriptNative.h"
#include "platform/v8_inspector/InspectedContext.h"
#include "wtf/PassOwnPtr.h"

#include <v8.h>

namespace blink {

class InjectedScriptHost;
class RemoteObjectId;
class V8FunctionCall;

namespace protocol {
class DictionaryValue;
}

using protocol::Maybe;

class InjectedScript final {
    PROTOCOL_DISALLOW_COPY(InjectedScript);
public:
    static PassOwnPtr<InjectedScript> create(InspectedContext*, InjectedScriptHost*);
    ~InjectedScript();

    void getProperties(ErrorString*, v8::Local<v8::Object>, const String16& groupName, bool ownProperties, bool accessorPropertiesOnly, bool generatePreview, OwnPtr<protocol::Array<protocol::Runtime::PropertyDescriptor>>* result, Maybe<protocol::Runtime::ExceptionDetails>*);
    void releaseObject(const String16& objectId);

    PassOwnPtr<protocol::Runtime::RemoteObject> wrapObject(ErrorString*, v8::Local<v8::Value>, const String16& groupName, bool forceValueType = false, bool generatePreview = false) const;
    bool wrapObjectProperty(ErrorString*, v8::Local<v8::Object>, v8::Local<v8::Value> key, const String16& groupName, bool forceValueType = false, bool generatePreview = false) const;
    bool wrapPropertyInArray(ErrorString*, v8::Local<v8::Array>, v8::Local<v8::String> property, const String16& groupName, bool forceValueType = false, bool generatePreview = false) const;
    bool wrapObjectsInArray(ErrorString*, v8::Local<v8::Array>, const String16& groupName, bool forceValueType = false, bool generatePreview = false) const;

    PassOwnPtr<protocol::Runtime::RemoteObject> wrapTable(v8::Local<v8::Value> table, v8::Local<v8::Value> columns) const;
    bool findObject(ErrorString*, const RemoteObjectId&, v8::Local<v8::Value>*) const;
    String16 objectGroupName(const RemoteObjectId&) const;
    void releaseObjectGroup(const String16&);

    void setCustomObjectFormatterEnabled(bool);

    InspectedContext* context() const { return m_context; }
    v8::Isolate* isolate() const;
    bool canAccessInspectedWindow() const;

    bool setLastEvaluationResult(ErrorString*, v8::Local<v8::Value>);
    v8::MaybeLocal<v8::Value> resolveCallArgument(ErrorString*, protocol::Runtime::CallArgument*);

    v8::MaybeLocal<v8::Object> commandLineAPI(ErrorString*) const;
    v8::MaybeLocal<v8::Object> remoteObjectAPI(ErrorString*, const String16& groupName) const;

    PassOwnPtr<protocol::Runtime::ExceptionDetails> createExceptionDetails(v8::Local<v8::Message>);
    void wrapEvaluateResult(ErrorString*,
        v8::MaybeLocal<v8::Value> maybeResultValue,
        const v8::TryCatch&,
        const String16& objectGroup,
        bool returnByValue,
        bool generatePreview,
        OwnPtr<protocol::Runtime::RemoteObject>* result,
        Maybe<bool>* wasThrown,
        Maybe<protocol::Runtime::ExceptionDetails>*);

    class ScopedGlobalObjectExtension {
        PROTOCOL_DISALLOW_COPY(ScopedGlobalObjectExtension);
    public:
        ScopedGlobalObjectExtension(InjectedScript* current, v8::MaybeLocal<v8::Object> extension);
        ~ScopedGlobalObjectExtension();

    private:
        v8::Local<v8::Symbol> m_symbol;
        v8::Local<v8::Context> m_context;
        v8::MaybeLocal<v8::Object> m_global;
    };

private:
    InjectedScript(InspectedContext*, v8::Local<v8::Object>, PassOwnPtr<InjectedScriptNative>);

    v8::Local<v8::Value> v8Value() const;
    v8::Local<v8::Value> callFunctionWithEvalEnabled(V8FunctionCall&, bool& hadException) const;
    PassOwnPtr<protocol::Value> makeCall(V8FunctionCall&);
    PassOwnPtr<protocol::Value> makeCallWithExceptionDetails(V8FunctionCall&, Maybe<protocol::Runtime::ExceptionDetails>*);
    v8::MaybeLocal<v8::Value> wrapValue(ErrorString*, v8::Local<v8::Value>, const String16& groupName, bool forceValueType, bool generatePreview) const;
    v8::MaybeLocal<v8::Object> callFunctionReturnObject(ErrorString*, V8FunctionCall&) const;

    InspectedContext* m_context;
    v8::Global<v8::Value> m_value;
    OwnPtr<InjectedScriptNative> m_native;
};

} // namespace blink

#endif
