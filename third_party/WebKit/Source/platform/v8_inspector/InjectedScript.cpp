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

#include "platform/v8_inspector/InjectedScript.h"

#include "platform/inspector_protocol/Parser.h"
#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/InjectedScriptHost.h"
#include "platform/v8_inspector/InjectedScriptManager.h"
#include "platform/v8_inspector/RemoteObjectId.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8FunctionCall.h"
#include "platform/v8_inspector/V8StackTraceImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"
#include "platform/v8_inspector/public/V8ToProtocolValue.h"

using blink::protocol::Array;
using blink::protocol::Debugger::CallFrame;
using blink::protocol::Debugger::CollectionEntry;
using blink::protocol::Debugger::FunctionDetails;
using blink::protocol::Debugger::GeneratorObjectDetails;
using blink::protocol::Runtime::PropertyDescriptor;
using blink::protocol::Runtime::InternalPropertyDescriptor;
using blink::protocol::Runtime::RemoteObject;
using blink::protocol::Maybe;

namespace blink {

static void weakCallback(const v8::WeakCallbackInfo<InjectedScript>& data)
{
    data.GetParameter()->dispose();
}

InjectedScript::InjectedScript(InjectedScriptManager* manager, v8::Local<v8::Context> context, v8::Local<v8::Object> object, PassOwnPtr<InjectedScriptNative> injectedScriptNative, int contextId)
    : m_manager(manager)
    , m_isolate(context->GetIsolate())
    , m_context(m_isolate, context)
    , m_value(m_isolate, object)
    , m_native(injectedScriptNative)
    , m_contextId(contextId)
{
    m_context.SetWeak(this, &weakCallback, v8::WeakCallbackType::kParameter);
}

InjectedScript::~InjectedScript()
{
}

void InjectedScript::getFunctionDetails(ErrorString* errorString, const String16& functionId, OwnPtr<FunctionDetails>* result)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_manager->debugger(), context(), v8Value(), "getFunctionDetails");
    function.appendArgument(functionId);
    OwnPtr<protocol::Value> resultValue = makeCall(function);
    protocol::ErrorSupport errors(errorString);
    *result = FunctionDetails::parse(resultValue.get(), &errors);
}

void InjectedScript::getCollectionEntries(ErrorString* errorString, const String16& objectId, OwnPtr<Array<CollectionEntry>>* result)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_manager->debugger(), context(), v8Value(), "getCollectionEntries");
    function.appendArgument(objectId);
    OwnPtr<protocol::Value> resultValue = makeCall(function);
    protocol::ErrorSupport errors(errorString);
    *result = Array<CollectionEntry>::parse(resultValue.get(), &errors);
}

void InjectedScript::getProperties(ErrorString* errorString, const String16& objectId, bool ownProperties, bool accessorPropertiesOnly, bool generatePreview, OwnPtr<Array<PropertyDescriptor>>* properties, Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_manager->debugger(), context(), v8Value(), "getProperties");
    function.appendArgument(objectId);
    function.appendArgument(ownProperties);
    function.appendArgument(accessorPropertiesOnly);
    function.appendArgument(generatePreview);

    OwnPtr<protocol::Value> result = makeCallWithExceptionDetails(function, exceptionDetails);
    if (exceptionDetails->isJust()) {
        // FIXME: make properties optional
        *properties = Array<PropertyDescriptor>::create();
        return;
    }
    protocol::ErrorSupport errors(errorString);
    *properties = Array<PropertyDescriptor>::parse(result.get(), &errors);
}

void InjectedScript::releaseObject(const String16& objectId)
{
    OwnPtr<protocol::Value> parsedObjectId = protocol::parseJSON(objectId);
    if (!parsedObjectId)
        return;
    protocol::DictionaryValue* object = protocol::DictionaryValue::cast(parsedObjectId.get());
    if (!object)
        return;
    int boundId = 0;
    if (!object->getNumber("id", &boundId))
        return;
    m_native->unbind(boundId);
}

PassOwnPtr<Array<CallFrame>> InjectedScript::wrapCallFrames(v8::Local<v8::Object> callFrames)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_manager->debugger(), context(), v8Value(), "wrapCallFrames");
    function.appendArgument(callFrames);
    bool hadException = false;
    v8::Local<v8::Value> callFramesValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);
    OwnPtr<protocol::Value> result = toProtocolValue(context(), callFramesValue);
    protocol::ErrorSupport errors;
    if (result && result->type() == protocol::Value::TypeArray)
        return Array<CallFrame>::parse(result.get(), &errors);
    return Array<CallFrame>::create();
}

PassOwnPtr<protocol::Runtime::RemoteObject> InjectedScript::wrapObject(ErrorString* errorString, v8::Local<v8::Value> value, const String16& groupName, bool forceValueType, bool generatePreview) const
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_manager->debugger(), context(), v8Value(), "wrapObject");
    v8::Local<v8::Value> wrappedObject;
    if (!wrapValue(errorString, value, groupName, forceValueType, generatePreview).ToLocal(&wrappedObject))
        return nullptr;
    protocol::ErrorSupport errors;
    OwnPtr<protocol::Runtime::RemoteObject> remoteObject = protocol::Runtime::RemoteObject::parse(toProtocolValue(context(), wrappedObject).get(), &errors);
    if (!remoteObject)
        *errorString = "Object has too long reference chain";
    return remoteObject.release();
}

bool InjectedScript::wrapObjectProperty(ErrorString* error, v8::Local<v8::Object> object, v8::Local<v8::Value> key, const String16& groupName, bool forceValueType, bool generatePreview) const
{
    v8::Local<v8::Value> property;
    if (!object->Get(context(), key).ToLocal(&property)) {
        *error = "Internal error.";
        return false;
    }
    v8::Local<v8::Value> wrappedProperty;
    if (!wrapValue(error, property, groupName, forceValueType, generatePreview).ToLocal(&wrappedProperty))
        return false;
    v8::Maybe<bool> success = object->Set(context(), key, wrappedProperty);
    if (success.IsNothing() || !success.FromJust()) {
        *error = "Internal error.";
        return false;
    }
    return true;
}

v8::MaybeLocal<v8::Value> InjectedScript::wrapValue(ErrorString* error, v8::Local<v8::Value> value, const String16& groupName, bool forceValueType, bool generatePreview) const
{
    V8FunctionCall function(m_manager->debugger(), context(), v8Value(), "wrapObject");
    function.appendArgument(value);
    function.appendArgument(groupName);
    function.appendArgument(canAccessInspectedWindow());
    function.appendArgument(forceValueType);
    function.appendArgument(generatePreview);
    bool hadException = false;
    v8::Local<v8::Value> r = callFunctionWithEvalEnabled(function, hadException);
    if (hadException) {
        *error = "Internal error.";
        return v8::MaybeLocal<v8::Value>();
    }
    return r;
}

PassOwnPtr<protocol::Runtime::RemoteObject> InjectedScript::wrapTable(v8::Local<v8::Value> table, v8::Local<v8::Value> columns) const
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_manager->debugger(), context(), v8Value(), "wrapTable");
    function.appendArgument(canAccessInspectedWindow());
    function.appendArgument(table);
    if (columns.IsEmpty())
        function.appendArgument(false);
    else
        function.appendArgument(columns);
    bool hadException = false;
    v8::Local<v8::Value>  r = callFunctionWithEvalEnabled(function, hadException);
    if (hadException)
        return nullptr;
    protocol::ErrorSupport errors;
    return protocol::Runtime::RemoteObject::parse(toProtocolValue(context(), r).get(), &errors);
}

bool InjectedScript::findObject(ErrorString* errorString, const RemoteObjectId& objectId, v8::Local<v8::Value>* outObject) const
{
    *outObject = m_native->objectForId(objectId.id());
    if (outObject->IsEmpty())
        *errorString = "Could not find object with given id";
    return !outObject->IsEmpty();
}

String16 InjectedScript::objectGroupName(const RemoteObjectId& objectId) const
{
    return m_native->groupName(objectId.id());
}

void InjectedScript::releaseObjectGroup(const String16& objectGroup)
{
    v8::HandleScope handles(m_isolate);
    m_native->releaseObjectGroup(objectGroup);
    if (objectGroup == "console") {
        V8FunctionCall function(m_manager->debugger(), context(), v8Value(), "clearLastEvaluationResult");
        bool hadException = false;
        callFunctionWithEvalEnabled(function, hadException);
        ASSERT(!hadException);
    }
}

void InjectedScript::setCustomObjectFormatterEnabled(bool enabled)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_manager->debugger(), context(), v8Value(), "setCustomObjectFormatterEnabled");
    function.appendArgument(enabled);
    makeCall(function);
}

bool InjectedScript::canAccessInspectedWindow() const
{
    v8::Local<v8::Context> callingContext = m_isolate->GetCallingContext();
    if (callingContext.IsEmpty())
        return true;
    return m_manager->debugger()->client()->callingContextCanAccessContext(callingContext, context());
}

v8::Local<v8::Context> InjectedScript::context() const
{
    return m_context.Get(m_isolate);
}

v8::Local<v8::Value> InjectedScript::v8Value() const
{
    return m_value.Get(m_isolate);
}

v8::Local<v8::Value> InjectedScript::callFunctionWithEvalEnabled(V8FunctionCall& function, bool& hadException) const
{
    v8::Local<v8::Context> localContext = context();
    v8::Context::Scope scope(localContext);
    bool evalIsDisabled = !localContext->IsCodeGenerationFromStringsAllowed();
    // Temporarily enable allow evals for inspector.
    if (evalIsDisabled)
        localContext->AllowCodeGenerationFromStrings(true);
    v8::Local<v8::Value> resultValue = function.call(hadException);
    if (evalIsDisabled)
        localContext->AllowCodeGenerationFromStrings(false);
    return resultValue;
}

PassOwnPtr<protocol::Value> InjectedScript::makeCall(V8FunctionCall& function)
{
    OwnPtr<protocol::Value> result;
    if (!canAccessInspectedWindow()) {
        result = protocol::StringValue::create("Can not access given context.");
        return nullptr;
    }

    bool hadException = false;
    v8::Local<v8::Value> resultValue = callFunctionWithEvalEnabled(function, hadException);

    ASSERT(!hadException);
    if (!hadException) {
        result = toProtocolValue(function.context(), resultValue);
        if (!result)
            result = protocol::StringValue::create("Object has too long reference chain");
    } else {
        result = protocol::StringValue::create("Exception while making a call.");
    }
    return result.release();
}

PassOwnPtr<protocol::Value> InjectedScript::makeCallWithExceptionDetails(V8FunctionCall& function, Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    OwnPtr<protocol::Value> result;
    v8::HandleScope handles(m_isolate);
    v8::Context::Scope scope(context());
    v8::TryCatch tryCatch(m_isolate);
    v8::Local<v8::Value> resultValue = function.callWithoutExceptionHandling();
    if (tryCatch.HasCaught()) {
        v8::Local<v8::Message> message = tryCatch.Message();
        String16 text = !message.IsEmpty() ? toProtocolStringWithTypeCheck(message->Get()) : "Internal error";
        *exceptionDetails = protocol::Runtime::ExceptionDetails::create().setText(text).build();
    } else {
        result = toProtocolValue(function.context(), resultValue);
        if (!result)
            result = protocol::StringValue::create("Object has too long reference chain");
    }
    return result.release();
}

void InjectedScript::dispose()
{
    m_manager->discardInjectedScript(m_contextId);
}

bool InjectedScript::setLastEvaluationResult(ErrorString* errorString, v8::Local<v8::Value> value)
{
    V8FunctionCall function(m_manager->debugger(), context(), v8Value(), "setLastEvaluationResult");
    function.appendArgument(value);
    bool hadException = false;
    function.call(hadException, false);
    if (hadException)
        *errorString = "Internal error";
    return !hadException;
}

v8::MaybeLocal<v8::Value> InjectedScript::resolveCallArgument(ErrorString* errorString, protocol::Runtime::CallArgument* callArgument)
{
    if (callArgument->hasObjectId()) {
        OwnPtr<RemoteObjectId> remoteObjectId = RemoteObjectId::parse(errorString, callArgument->getObjectId(""));
        if (!remoteObjectId)
            return v8::MaybeLocal<v8::Value>();
        if (remoteObjectId->contextId() != m_contextId) {
            *errorString = "Argument should belong to the same JavaScript world as target object";
            return v8::MaybeLocal<v8::Value>();
        }
        v8::Local<v8::Value> object;
        if (!findObject(errorString, *remoteObjectId, &object))
            return v8::MaybeLocal<v8::Value>();
        return object;
    }
    if (callArgument->hasValue()) {
        String16 value = callArgument->getValue(nullptr)->toJSONString();
        if (callArgument->getType(String16()) == "number")
            value = "Number(" + value + ")";
        v8::Local<v8::Value> object;
        if (!m_manager->debugger()->compileAndRunInternalScript(context(), toV8String(m_isolate, value)).ToLocal(&object)) {
            *errorString = "Couldn't parse value object in call argument";
            return v8::MaybeLocal<v8::Value>();
        }
        return object;
    }
    return v8::Undefined(m_isolate);
}

v8::MaybeLocal<v8::Object> InjectedScript::commandLineAPI(ErrorString* errorString) const
{
    V8FunctionCall function(m_manager->debugger(), context(), v8Value(), "commandLineAPI");
    return callFunctionReturnObject(errorString, function);
}

v8::MaybeLocal<v8::Object> InjectedScript::remoteObjectAPI(ErrorString* errorString, const String16& groupName) const
{
    V8FunctionCall function(m_manager->debugger(), context(), v8Value(), "remoteObjectAPI");
    function.appendArgument(groupName);
    return callFunctionReturnObject(errorString, function);
}

v8::MaybeLocal<v8::Object> InjectedScript::callFunctionReturnObject(ErrorString* errorString, V8FunctionCall& function) const
{
    bool hadException = false;
    v8::Local<v8::Value> result = function.call(hadException, false);
    v8::Local<v8::Object> resultObject;
    if (hadException || result.IsEmpty() || !result->ToObject(context()).ToLocal(&resultObject)) {
        *errorString = "Internal error";
        return v8::MaybeLocal<v8::Object>();
    }
    return resultObject;
}

PassOwnPtr<protocol::Runtime::ExceptionDetails> InjectedScript::createExceptionDetails(v8::Local<v8::Message> message)
{
    OwnPtr<protocol::Runtime::ExceptionDetails> exceptionDetailsObject = protocol::Runtime::ExceptionDetails::create().setText(toProtocolString(message->Get())).build();
    exceptionDetailsObject->setUrl(toProtocolStringWithTypeCheck(message->GetScriptResourceName()));
    exceptionDetailsObject->setScriptId(String16::number(message->GetScriptOrigin().ScriptID()->Value()));

    v8::Maybe<int> lineNumber = message->GetLineNumber(context());
    if (lineNumber.IsJust())
        exceptionDetailsObject->setLine(lineNumber.FromJust());
    v8::Maybe<int> columnNumber = message->GetStartColumn(context());
    if (columnNumber.IsJust())
        exceptionDetailsObject->setColumn(columnNumber.FromJust());

    v8::Local<v8::StackTrace> stackTrace = message->GetStackTrace();
    if (!stackTrace.IsEmpty() && stackTrace->GetFrameCount() > 0)
        exceptionDetailsObject->setStack(m_manager->debugger()->createStackTrace(stackTrace, stackTrace->GetFrameCount())->buildInspectorObject());
    return exceptionDetailsObject.release();
}

void InjectedScript::wrapEvaluateResult(ErrorString* errorString, v8::MaybeLocal<v8::Value> maybeResultValue, const v8::TryCatch& tryCatch, const String16& objectGroup, bool returnByValue, bool generatePreview, OwnPtr<protocol::Runtime::RemoteObject>* result, Maybe<bool>* wasThrown, Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    v8::Local<v8::Value> resultValue;
    if (!tryCatch.HasCaught()) {
        if (!maybeResultValue.ToLocal(&resultValue)) {
            *errorString = "Internal error";
            return;
        }
        OwnPtr<RemoteObject> remoteObject = wrapObject(errorString, resultValue, objectGroup, returnByValue, generatePreview);
        if (!remoteObject)
            return;
        if (objectGroup == "console" && !setLastEvaluationResult(errorString, resultValue))
            return;
        *result = remoteObject.release();
        if (wasThrown)
            *wasThrown = false;
    } else {
        v8::Local<v8::Value> exception = tryCatch.Exception();
        OwnPtr<RemoteObject> remoteObject = wrapObject(errorString, exception, objectGroup, false, generatePreview && !exception->IsNativeError());
        if (!remoteObject)
            return;
        *result = remoteObject.release();
        if (exceptionDetails)
            *exceptionDetails = createExceptionDetails(tryCatch.Message());
        if (wasThrown)
            *wasThrown = true;
    }
}

} // namespace blink
