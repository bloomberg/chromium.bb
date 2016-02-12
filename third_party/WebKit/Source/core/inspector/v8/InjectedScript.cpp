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

#include "core/inspector/v8/InjectedScript.h"

#include "core/inspector/v8/InjectedScriptHost.h"
#include "core/inspector/v8/InjectedScriptManager.h"
#include "core/inspector/v8/RemoteObjectId.h"
#include "core/inspector/v8/V8FunctionCall.h"
#include "core/inspector/v8/V8StringUtil.h"
#include "core/inspector/v8/public/V8Debugger.h"
#include "core/inspector/v8/public/V8DebuggerClient.h"
#include "platform/JSONParser.h"
#include "platform/JSONValues.h"
#include "platform/JSONValuesForV8.h"
#include "wtf/text/WTFString.h"

using blink::protocol::TypeBuilder::Array;
using blink::protocol::TypeBuilder::Debugger::CallFrame;
using blink::protocol::TypeBuilder::Debugger::CollectionEntry;
using blink::protocol::TypeBuilder::Debugger::FunctionDetails;
using blink::protocol::TypeBuilder::Debugger::GeneratorObjectDetails;
using blink::protocol::TypeBuilder::Runtime::PropertyDescriptor;
using blink::protocol::TypeBuilder::Runtime::InternalPropertyDescriptor;
using blink::protocol::TypeBuilder::Runtime::RemoteObject;

namespace blink {

static PassRefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails> toExceptionDetails(PassRefPtr<JSONObject> object)
{
    String text;
    if (!object->getString("text", &text))
        return nullptr;

    RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails> exceptionDetails = protocol::TypeBuilder::Runtime::ExceptionDetails::create().setText(text);
    String url;
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

    RefPtr<JSONArray> stackTrace = object->getArray("stackTrace");
    if (stackTrace && stackTrace->length() > 0) {
        RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Runtime::CallFrame>> frames = protocol::TypeBuilder::Array<protocol::TypeBuilder::Runtime::CallFrame>::create();
        for (unsigned i = 0; i < stackTrace->length(); ++i) {
            RefPtr<JSONObject> stackFrame = stackTrace->get(i)->asObject();
            int lineNumber = 0;
            stackFrame->getNumber("lineNumber", &lineNumber);
            int column = 0;
            stackFrame->getNumber("column", &column);
            int scriptId = 0;
            stackFrame->getNumber("scriptId", &scriptId);
            if (i == 0 && scriptId == originScriptId)
                originScriptId = 0;

            String sourceURL;
            stackFrame->getString("scriptNameOrSourceURL", &sourceURL);
            String functionName;
            stackFrame->getString("functionName", &functionName);

            RefPtr<protocol::TypeBuilder::Runtime::CallFrame> callFrame = protocol::TypeBuilder::Runtime::CallFrame::create()
                .setFunctionName(functionName)
                .setScriptId(String::number(scriptId))
                .setUrl(sourceURL)
                .setLineNumber(lineNumber)
                .setColumnNumber(column);

            frames->addItem(callFrame.release());
        }
        RefPtr<protocol::TypeBuilder::Runtime::StackTrace> stack = protocol::TypeBuilder::Runtime::StackTrace::create()
            .setCallFrames(frames.release());
        exceptionDetails->setStack(stack.release());
    }
    if (originScriptId)
        exceptionDetails->setScriptId(String::number(originScriptId));
    return exceptionDetails.release();
}

static void weakCallback(const v8::WeakCallbackInfo<InjectedScript>& data)
{
    data.GetParameter()->dispose();
}

InjectedScript::InjectedScript(InjectedScriptManager* manager, v8::Local<v8::Context> context, v8::Local<v8::Object> object, V8DebuggerClient* client, PassRefPtr<InjectedScriptNative> injectedScriptNative, int contextId)
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

void InjectedScript::evaluate(ErrorString* errorString, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>* result, protocol::TypeBuilder::OptOutput<bool>* wasThrown, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>* exceptionDetails)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "evaluate");
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown, exceptionDetails);
}

void InjectedScript::callFunctionOn(ErrorString* errorString, const String& objectId, const String& expression, const String& arguments, bool returnByValue, bool generatePreview, RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>* result, protocol::TypeBuilder::OptOutput<bool>* wasThrown)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "callFunctionOn");
    function.appendArgument(objectId);
    function.appendArgument(expression);
    function.appendArgument(arguments);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::evaluateOnCallFrame(ErrorString* errorString, v8::Local<v8::Object> callFrames, bool isAsyncCallStack, const String& callFrameId, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, RefPtr<RemoteObject>* result, protocol::TypeBuilder::OptOutput<bool>* wasThrown, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>* exceptionDetails)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "evaluateOnCallFrame");
    function.appendArgument(callFrames);
    function.appendArgument(isAsyncCallStack);
    function.appendArgument(callFrameId);
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown, exceptionDetails);
}

void InjectedScript::restartFrame(ErrorString* errorString, v8::Local<v8::Object> callFrames, const String& callFrameId)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "restartFrame");
    function.appendArgument(callFrames);
    function.appendArgument(callFrameId);
    RefPtr<JSONValue> resultValue;
    makeCall(function, &resultValue);
    if (resultValue) {
        if (resultValue->type() == JSONValue::TypeString) {
            resultValue->asString(errorString);
        } else {
            bool value;
            ASSERT_UNUSED(value, resultValue->asBoolean(&value) && value);
        }
        return;
    }
    *errorString = "Internal error";
}

void InjectedScript::getStepInPositions(ErrorString* errorString, v8::Local<v8::Object> callFrames, const String& callFrameId, RefPtr<Array<protocol::TypeBuilder::Debugger::Location>>& positions)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "getStepInPositions");
    function.appendArgument(callFrames);
    function.appendArgument(callFrameId);
    RefPtr<JSONValue> resultValue;
    makeCall(function, &resultValue);
    if (resultValue) {
        if (resultValue->type() == JSONValue::TypeString) {
            resultValue->asString(errorString);
            return;
        }
        if (resultValue->type() == JSONValue::TypeArray) {
            positions = Array<protocol::TypeBuilder::Debugger::Location>::runtimeCast(resultValue);
            return;
        }
    }
    *errorString = "Internal error";
}

void InjectedScript::setVariableValue(ErrorString* errorString, v8::Local<v8::Object> callFrames, const String* callFrameIdOpt, const String* functionObjectIdOpt, int scopeNumber, const String& variableName, const String& newValueStr)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "setVariableValue");
    if (callFrameIdOpt) {
        function.appendArgument(callFrames);
        function.appendArgument(*callFrameIdOpt);
    } else {
        function.appendArgument(false);
        function.appendArgument(false);
    }
    if (functionObjectIdOpt)
        function.appendArgument(*functionObjectIdOpt);
    else
        function.appendArgument(false);
    function.appendArgument(scopeNumber);
    function.appendArgument(variableName);
    function.appendArgument(newValueStr);
    RefPtr<JSONValue> resultValue;
    makeCall(function, &resultValue);
    if (!resultValue) {
        *errorString = "Internal error";
        return;
    }
    if (resultValue->type() == JSONValue::TypeString) {
        resultValue->asString(errorString);
        return;
    }
    // Normal return.
}

void InjectedScript::getFunctionDetails(ErrorString* errorString, const String& functionId, RefPtr<FunctionDetails>* result)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "getFunctionDetails");
    function.appendArgument(functionId);
    RefPtr<JSONValue> resultValue;
    makeCall(function, &resultValue);
    if (!resultValue || resultValue->type() != JSONValue::TypeObject) {
        if (!resultValue->asString(errorString))
            *errorString = "Internal error";
        return;
    }
    *result = FunctionDetails::runtimeCast(resultValue);
}

void InjectedScript::getGeneratorObjectDetails(ErrorString* errorString, const String& objectId, RefPtr<GeneratorObjectDetails>* result)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "getGeneratorObjectDetails");
    function.appendArgument(objectId);
    RefPtr<JSONValue> resultValue;
    makeCall(function, &resultValue);
    if (!resultValue || resultValue->type() != JSONValue::TypeObject) {
        if (!resultValue->asString(errorString))
            *errorString = "Internal error";
        return;
    }
    *result = GeneratorObjectDetails::runtimeCast(resultValue);
}

void InjectedScript::getCollectionEntries(ErrorString* errorString, const String& objectId, RefPtr<Array<CollectionEntry>>* result)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "getCollectionEntries");
    function.appendArgument(objectId);
    RefPtr<JSONValue> resultValue;
    makeCall(function, &resultValue);
    if (!resultValue || resultValue->type() != JSONValue::TypeArray) {
        if (!resultValue->asString(errorString))
            *errorString = "Internal error";
        return;
    }
    *result = Array<CollectionEntry>::runtimeCast(resultValue);
}

void InjectedScript::getProperties(ErrorString* errorString, const String& objectId, bool ownProperties, bool accessorPropertiesOnly, bool generatePreview, RefPtr<Array<PropertyDescriptor>>* properties, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>* exceptionDetails)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "getProperties");
    function.appendArgument(objectId);
    function.appendArgument(ownProperties);
    function.appendArgument(accessorPropertiesOnly);
    function.appendArgument(generatePreview);

    RefPtr<JSONValue> result;
    makeCallWithExceptionDetails(function, &result, exceptionDetails);
    if (*exceptionDetails) {
        // FIXME: make properties optional
        *properties = Array<PropertyDescriptor>::create();
        return;
    }
    if (!result || result->type() != JSONValue::TypeArray) {
        *errorString = "Internal error";
        return;
    }
    *properties = Array<PropertyDescriptor>::runtimeCast(result);
}

void InjectedScript::getInternalProperties(ErrorString* errorString, const String& objectId, RefPtr<Array<InternalPropertyDescriptor>>* properties, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>* exceptionDetails)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "getInternalProperties");
    function.appendArgument(objectId);

    RefPtr<JSONValue> result;
    makeCallWithExceptionDetails(function, &result, exceptionDetails);
    if (*exceptionDetails)
        return;
    if (!result || result->type() != JSONValue::TypeArray) {
        *errorString = "Internal error";
        return;
    }
    RefPtr<Array<InternalPropertyDescriptor>> array = Array<InternalPropertyDescriptor>::runtimeCast(result);
    if (array->length() > 0)
        *properties = array;
}

void InjectedScript::releaseObject(const String& objectId)
{
    RefPtr<JSONValue> parsedObjectId = parseJSON(objectId);
    if (!parsedObjectId)
        return;
    RefPtr<JSONObject> object;
    if (!parsedObjectId->asObject(&object))
        return;
    int boundId = 0;
    if (!object->getNumber("id", &boundId))
        return;
    m_native->unbind(boundId);
}

PassRefPtr<Array<CallFrame>> InjectedScript::wrapCallFrames(v8::Local<v8::Object> callFrames, int asyncOrdinal)
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "wrapCallFrames");
    function.appendArgument(callFrames);
    function.appendArgument(asyncOrdinal);
    bool hadException = false;
    v8::Local<v8::Value> callFramesValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);
    RefPtr<JSONValue> result = toJSONValue(context(), callFramesValue);
    if (result && result->type() == JSONValue::TypeArray)
        return Array<CallFrame>::runtimeCast(result);
    return Array<CallFrame>::create();
}

PassRefPtr<protocol::TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapObject(v8::Local<v8::Value> value, const String& groupName, bool generatePreview) const
{
    v8::HandleScope handles(m_isolate);
    V8FunctionCall function(m_client, context(), v8Value(), "wrapObject");
    function.appendArgument(value);
    function.appendArgument(groupName);
    function.appendArgument(canAccessInspectedWindow());
    function.appendArgument(generatePreview);
    bool hadException = false;
    v8::Local<v8::Value> r = callFunctionWithEvalEnabled(function, hadException);
    if (hadException)
        return nullptr;
    RefPtr<JSONObject> rawResult = toJSONValue(context(), r)->asObject();
    return protocol::TypeBuilder::Runtime::RemoteObject::runtimeCast(rawResult);
}

PassRefPtr<protocol::TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapTable(v8::Local<v8::Value> table, v8::Local<v8::Value> columns) const
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
    RefPtr<JSONObject> rawResult = toJSONValue(context(), r)->asObject();
    return protocol::TypeBuilder::Runtime::RemoteObject::runtimeCast(rawResult);
}

v8::Local<v8::Value> InjectedScript::findObject(const RemoteObjectId& objectId) const
{
    return m_native->objectForId(objectId.id());
}

String InjectedScript::objectGroupName(const RemoteObjectId& objectId) const
{
    return m_native->groupName(objectId.id());
}

void InjectedScript::releaseObjectGroup(const String& objectGroup)
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
    RefPtr<JSONValue> result;
    makeCall(function, &result);
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

void InjectedScript::makeCall(V8FunctionCall& function, RefPtr<JSONValue>* result)
{
    if (!canAccessInspectedWindow()) {
        *result = JSONString::create("Can not access given context.");
        return;
    }

    bool hadException = false;
    v8::Local<v8::Value> resultValue = callFunctionWithEvalEnabled(function, hadException);

    ASSERT(!hadException);
    if (!hadException) {
        *result = toJSONValue(function.context(), resultValue);
        if (!*result)
            *result = JSONString::create(String::format("Object has too long reference chain(must not be longer than %d)", JSONValue::maxDepth));
    } else {
        *result = JSONString::create("Exception while making a call.");
    }
}

void InjectedScript::makeEvalCall(ErrorString* errorString, V8FunctionCall& function, RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>* objectResult, protocol::TypeBuilder::OptOutput<bool>* wasThrown, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>* exceptionDetails)
{
    RefPtr<JSONValue> result;
    makeCall(function, &result);
    if (!result) {
        *errorString = "Internal error: result value is empty";
        return;
    }
    if (result->type() == JSONValue::TypeString) {
        result->asString(errorString);
        ASSERT(errorString->length());
        return;
    }
    RefPtr<JSONObject> resultPair = result->asObject();
    if (!resultPair) {
        *errorString = "Internal error: result is not an Object";
        return;
    }
    RefPtr<JSONObject> resultObj = resultPair->getObject("result");
    bool wasThrownVal = false;
    if (!resultObj || !resultPair->getBoolean("wasThrown", &wasThrownVal)) {
        *errorString = "Internal error: result is not a pair of value and wasThrown flag";
        return;
    }
    if (wasThrownVal) {
        RefPtr<JSONObject> objectExceptionDetails = resultPair->getObject("exceptionDetails");
        if (objectExceptionDetails)
            *exceptionDetails = toExceptionDetails(objectExceptionDetails.release());
    }
    *objectResult = protocol::TypeBuilder::Runtime::RemoteObject::runtimeCast(resultObj);
    *wasThrown = wasThrownVal;
}

void InjectedScript::makeCallWithExceptionDetails(V8FunctionCall& function, RefPtr<JSONValue>* result, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>* exceptionDetails)
{
    v8::TryCatch tryCatch(m_isolate);
    v8::Local<v8::Value> resultValue = function.callWithoutExceptionHandling();
    if (tryCatch.HasCaught()) {
        v8::Local<v8::Message> message = tryCatch.Message();
        String text = !message.IsEmpty() ? toWTFStringWithTypeCheck(message->Get()) : "Internal error";
        *exceptionDetails = protocol::TypeBuilder::Runtime::ExceptionDetails::create().setText(text);
    } else {
        *result = toJSONValue(function.context(), resultValue);
        if (!*result)
            *result = JSONString::create(String::format("Object has too long reference chain(must not be longer than %d)", JSONValue::maxDepth));
    }
}

void InjectedScript::dispose()
{
    m_manager->discardInjectedScript(m_contextId);
}

} // namespace blink
