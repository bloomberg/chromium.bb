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
#include "platform/v8_inspector/V8FunctionCall.h"
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

static PassOwnPtr<protocol::Runtime::ExceptionDetails> toExceptionDetails(protocol::DictionaryValue* object)
{
    String16 text;
    if (!object->getString("text", &text))
        return nullptr;

    OwnPtr<protocol::Runtime::ExceptionDetails> exceptionDetails = protocol::Runtime::ExceptionDetails::create().setText(text).build();
    String16 url;
    if (object->getString("url", &url))
        exceptionDetails->setUrl(url);
    int line = 0;
    if (object->getNumber("line", &line))
        exceptionDetails->setLine(line);
    int column = 0;
    if (object->getNumber("column", &column))
        exceptionDetails->setColumn(column);
    int originScriptId = 0;
    object->getNumber("scriptId", &originScriptId);

    protocol::ListValue* stackTrace = object->getArray("stackTrace");
    if (stackTrace && stackTrace->size() > 0) {
        OwnPtr<protocol::Array<protocol::Runtime::CallFrame>> frames = protocol::Array<protocol::Runtime::CallFrame>::create();
        for (unsigned i = 0; i < stackTrace->size(); ++i) {
            protocol::DictionaryValue* stackFrame = protocol::DictionaryValue::cast(stackTrace->at(i));
            int lineNumber = 0;
            stackFrame->getNumber("lineNumber", &lineNumber);
            int column = 0;
            stackFrame->getNumber("column", &column);
            int scriptId = 0;
            stackFrame->getNumber("scriptId", &scriptId);
            if (i == 0 && scriptId == originScriptId)
                originScriptId = 0;

            String16 sourceURL;
            stackFrame->getString("scriptNameOrSourceURL", &sourceURL);
            String16 functionName;
            stackFrame->getString("functionName", &functionName);

            OwnPtr<protocol::Runtime::CallFrame> callFrame = protocol::Runtime::CallFrame::create()
                .setFunctionName(functionName)
                .setScriptId(String16::number(scriptId))
                .setUrl(sourceURL)
                .setLineNumber(lineNumber)
                .setColumnNumber(column).build();

            frames->addItem(callFrame.release());
        }
        OwnPtr<protocol::Runtime::StackTrace> stack = protocol::Runtime::StackTrace::create()
            .setCallFrames(frames.release()).build();
        exceptionDetails->setStack(stack.release());
    }
    if (originScriptId)
        exceptionDetails->setScriptId(String16::number(originScriptId));
    return exceptionDetails.release();
}

static void weakCallback(const v8::WeakCallbackInfo<InjectedScript>& data)
{
    data.GetParameter()->dispose();
}

InjectedScript::InjectedScript(InjectedScriptManager* manager, v8::Local<v8::Context> context, v8::Local<v8::Object> object, V8DebuggerClient* client, PassOwnPtr<InjectedScriptNative> injectedScriptNative, int contextId)
    : m_manager(manager)
    , m_isolate(context->GetIsolate())
    , m_context(m_isolate, context)
    , m_value(m_isolate, object)
    , m_client(client)
    , m_native(injectedScriptNative)
    , m_contextId(contextId)
{
    m_context.SetWeak(this, &weakCallback, v8::WeakCallbackType::kParameter);
}

InjectedScript::~InjectedScript()
{
}

void InjectedScript::evaluate(ErrorString* errorString, const String16& expression, const String16& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, OwnPtr<protocol::Runtime::RemoteObject>* result, Maybe<bool>* wasThrown, Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "evaluate");
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    *result = makeEvalCall(errorString, function, wasThrown, exceptionDetails);
}

void InjectedScript::callFunctionOn(ErrorString* errorString, const String16& objectId, const String16& expression, const String16& arguments, bool returnByValue, bool generatePreview, OwnPtr<protocol::Runtime::RemoteObject>* result, Maybe<bool>* wasThrown)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "callFunctionOn");
    function.appendArgument(objectId);
    function.appendArgument(expression);
    function.appendArgument(arguments);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    *result = makeEvalCall(errorString, function, wasThrown);
}

void InjectedScript::evaluateOnCallFrame(ErrorString* errorString, v8::Local<v8::Object> callFrames, const String16& callFrameId, const String16& expression, const String16& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, OwnPtr<RemoteObject>* result, Maybe<bool>* wasThrown, Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "evaluateOnCallFrame");
    function.appendArgument(callFrames);
    function.appendArgument(callFrameId);
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    *result = makeEvalCall(errorString, function, wasThrown, exceptionDetails);
}

void InjectedScript::restartFrame(ErrorString* errorString, v8::Local<v8::Object> callFrames, const String16& callFrameId)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "restartFrame");
    function.appendArgument(callFrames);
    function.appendArgument(callFrameId);
    OwnPtr<protocol::Value> resultValue = makeCall(function);
    if (resultValue) {
        if (resultValue->type() == protocol::Value::TypeString) {
            resultValue->asString(errorString);
        } else {
            bool value;
            ASSERT_UNUSED(value, resultValue->asBoolean(&value) && value);
        }
        return;
    }
    *errorString = "Internal error";
}

void InjectedScript::setVariableValue(ErrorString* errorString,
    v8::Local<v8::Object> callFrames,
    const protocol::Maybe<String16>& callFrameIdOpt,
    const protocol::Maybe<String16>&  functionObjectIdOpt,
    int scopeNumber,
    const String16& variableName,
    const String16& newValueStr)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "setVariableValue");
    if (callFrameIdOpt.isJust()) {
        function.appendArgument(callFrames);
        function.appendArgument(callFrameIdOpt.fromJust());
    } else {
        function.appendArgument(false);
        function.appendArgument(false);
    }
    if (functionObjectIdOpt.isJust())
        function.appendArgument(functionObjectIdOpt.fromJust());
    else
        function.appendArgument(false);
    function.appendArgument(scopeNumber);
    function.appendArgument(variableName);
    function.appendArgument(newValueStr);
    OwnPtr<protocol::Value> resultValue = makeCall(function);
    if (!resultValue) {
        *errorString = "Internal error";
        return;
    }
    if (resultValue->type() == protocol::Value::TypeString) {
        resultValue->asString(errorString);
        return;
    }
    // Normal return.
}

void InjectedScript::getFunctionDetails(ErrorString* errorString, const String16& functionId, OwnPtr<FunctionDetails>* result)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "getFunctionDetails");
    function.appendArgument(functionId);
    OwnPtr<protocol::Value> resultValue = makeCall(function);
    protocol::ErrorSupport errors(errorString);
    *result = FunctionDetails::parse(resultValue.get(), &errors);
}

void InjectedScript::getCollectionEntries(ErrorString* errorString, const String16& objectId, OwnPtr<Array<CollectionEntry>>* result)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "getCollectionEntries");
    function.appendArgument(objectId);
    OwnPtr<protocol::Value> resultValue = makeCall(function);
    protocol::ErrorSupport errors(errorString);
    *result = Array<CollectionEntry>::parse(resultValue.get(), &errors);
}

void InjectedScript::getProperties(ErrorString* errorString, const String16& objectId, bool ownProperties, bool accessorPropertiesOnly, bool generatePreview, OwnPtr<Array<PropertyDescriptor>>* properties, Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "getProperties");
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

void InjectedScript::getInternalProperties(ErrorString* errorString, const String16& objectId, Maybe<Array<InternalPropertyDescriptor>>* properties, Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "getInternalProperties");
    function.appendArgument(objectId);

    OwnPtr<protocol::Value> result = makeCallWithExceptionDetails(function, exceptionDetails);
    if (exceptionDetails->isJust())
        return;
    protocol::ErrorSupport errors(errorString);
    OwnPtr<Array<InternalPropertyDescriptor>> array = Array<InternalPropertyDescriptor>::parse(result.get(), &errors);
    if (!errors.hasErrors() && array->length() > 0)
        *properties = array.release();
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

v8::MaybeLocal<v8::Value> InjectedScript::runCompiledScript(v8::Local<v8::Script> script, bool includeCommandLineAPI)
{
    v8::Local<v8::Symbol> commandLineAPISymbolValue = V8Debugger::commandLineAPISymbol(m_isolate);
    v8::Local<v8::Object> global = context()->Global();
    if (includeCommandLineAPI) {
        V8FunctionCall function(m_client, context(), v8Value(), "commandLineAPI");
        bool hadException = false;
        v8::Local<v8::Value> commandLineAPI = function.call(hadException, false);
        if (!hadException)
            global->Set(commandLineAPISymbolValue, commandLineAPI);
    }

    v8::MaybeLocal<v8::Value> maybeValue = m_client->runCompiledScript(context(), script);
    if (includeCommandLineAPI)
        global->Delete(context(), commandLineAPISymbolValue);

    return maybeValue;
}

PassOwnPtr<Array<CallFrame>> InjectedScript::wrapCallFrames(v8::Local<v8::Object> callFrames)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "wrapCallFrames");
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

PassOwnPtr<protocol::Runtime::RemoteObject> InjectedScript::wrapObject(v8::Local<v8::Value> value, const String16& groupName, bool generatePreview) const
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "wrapObject");
    v8::Local<v8::Value> wrappedObject;
    ErrorString errorString;
    if (!wrapValue(&errorString, value, groupName, generatePreview).ToLocal(&wrappedObject))
        return nullptr;
    protocol::ErrorSupport errors;
    return protocol::Runtime::RemoteObject::parse(toProtocolValue(context(), wrappedObject).get(), &errors);
}

bool InjectedScript::wrapObjectProperty(ErrorString* error, v8::Local<v8::Object> object, v8::Local<v8::Value> key, const String16& groupName, bool generatePreview) const
{
    v8::Local<v8::Value> property;
    if (!object->Get(context(), key).ToLocal(&property)) {
        *error = "Internal error.";
        return false;
    }
    v8::Local<v8::Value> wrappedProperty;
    if (!wrapValue(error, property, groupName, generatePreview).ToLocal(&wrappedProperty))
        return false;
    v8::Maybe<bool> success = object->Set(context(), key, wrappedProperty);
    if (success.IsNothing() || !success.FromJust()) {
        *error = "Internal error.";
        return false;
    }
    return true;
}

v8::MaybeLocal<v8::Value> InjectedScript::wrapValue(ErrorString* error, v8::Local<v8::Value> value, const String16& groupName, bool generatePreview) const
{
    V8FunctionCall function(m_client, context(), v8Value(), "wrapObject");
    function.appendArgument(value);
    function.appendArgument(groupName);
    function.appendArgument(canAccessInspectedWindow());
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
    V8FunctionCall function(m_client, context(), v8Value(), "wrapTable");
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

v8::Local<v8::Value> InjectedScript::findObject(const RemoteObjectId& objectId) const
{
    return m_native->objectForId(objectId.id());
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
        V8FunctionCall function(m_client, context(), v8Value(), "clearLastEvaluationResult");
        bool hadException = false;
        callFunctionWithEvalEnabled(function, hadException);
        ASSERT(!hadException);
    }
}

void InjectedScript::setCustomObjectFormatterEnabled(bool enabled)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "setCustomObjectFormatterEnabled");
    function.appendArgument(enabled);
    makeCall(function);
}

bool InjectedScript::canAccessInspectedWindow() const
{
    v8::Local<v8::Context> callingContext = m_isolate->GetCallingContext();
    if (callingContext.IsEmpty())
        return true;
    return m_client->callingContextCanAccessContext(callingContext, context());
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

PassOwnPtr<protocol::Runtime::RemoteObject> InjectedScript::makeEvalCall(ErrorString* errorString, V8FunctionCall& function, Maybe<bool>* wasThrown, Maybe<protocol::Runtime::ExceptionDetails>* exceptionDetails)
{
    OwnPtr<protocol::Value> result = makeCall(function);
    if (!result) {
        *errorString = "Internal error: result value is empty";
        return nullptr;
    }
    if (result->type() == protocol::Value::TypeString) {
        result->asString(errorString);
        ASSERT(errorString->length());
        return nullptr;
    }
    protocol::DictionaryValue* resultPair = protocol::DictionaryValue::cast(result.get());
    if (!resultPair) {
        *errorString = "Internal error: result is not an Object";
        return nullptr;
    }
    protocol::DictionaryValue* resultObj = resultPair->getObject("result");
    bool wasThrownVal = false;
    if (!resultObj || !resultPair->getBoolean("wasThrown", &wasThrownVal)) {
        *errorString = "Internal error: result is not a pair of value and wasThrown flag";
        return nullptr;
    }
    if (wasThrownVal) {
        protocol::DictionaryValue* objectExceptionDetails = resultPair->getObject("exceptionDetails");
        if (objectExceptionDetails)
            *exceptionDetails = toExceptionDetails(objectExceptionDetails);
    }
    protocol::ErrorSupport errors(errorString);
    *wasThrown = wasThrownVal;
    return protocol::Runtime::RemoteObject::parse(resultObj, &errors);
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

} // namespace blink
