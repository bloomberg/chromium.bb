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

#include "core/inspector/InjectedScript.h"

#include "bindings/core/v8/ScriptFunctionCall.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/JSONParser.h"
#include "core/inspector/RemoteObjectId.h"
#include "core/inspector/v8/V8DebuggerClient.h"
#include "platform/JSONValues.h"
#include "platform/JSONValuesForV8.h"
#include "wtf/text/WTFString.h"

using blink::TypeBuilder::Array;
using blink::TypeBuilder::Debugger::CallFrame;
using blink::TypeBuilder::Debugger::CollectionEntry;
using blink::TypeBuilder::Debugger::FunctionDetails;
using blink::TypeBuilder::Debugger::GeneratorObjectDetails;
using blink::TypeBuilder::Runtime::PropertyDescriptor;
using blink::TypeBuilder::Runtime::InternalPropertyDescriptor;
using blink::TypeBuilder::Runtime::RemoteObject;

namespace blink {

static PassRefPtr<TypeBuilder::Debugger::ExceptionDetails> toExceptionDetails(PassRefPtr<JSONObject> object)
{
    String text;
    if (!object->getString("text", &text))
        return nullptr;

    RefPtr<TypeBuilder::Debugger::ExceptionDetails> exceptionDetails = TypeBuilder::Debugger::ExceptionDetails::create().setText(text);
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
        RefPtr<TypeBuilder::Array<TypeBuilder::Console::CallFrame>> frames = TypeBuilder::Array<TypeBuilder::Console::CallFrame>::create();
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

            RefPtr<TypeBuilder::Console::CallFrame> callFrame = TypeBuilder::Console::CallFrame::create()
                .setFunctionName(functionName)
                .setScriptId(String::number(scriptId))
                .setUrl(sourceURL)
                .setLineNumber(lineNumber)
                .setColumnNumber(column);

            frames->addItem(callFrame.release());
        }
        exceptionDetails->setStackTrace(frames.release());
    }
    if (originScriptId)
        exceptionDetails->setScriptId(String::number(originScriptId));
    return exceptionDetails.release();
}

InjectedScript::InjectedScript(ScriptValue injectedScriptObject, V8DebuggerClient* client, PassRefPtr<InjectedScriptNative> injectedScriptNative, int contextId)
    : m_isolate(injectedScriptObject.isolate())
    , m_injectedScriptObject(injectedScriptObject)
    , m_client(client)
    , m_native(injectedScriptNative)
    , m_contextId(contextId)
{
}

InjectedScript::~InjectedScript()
{
}

void InjectedScript::evaluate(ErrorString* errorString, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown, RefPtr<TypeBuilder::Debugger::ExceptionDetails>* exceptionDetails)
{
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    ScriptFunctionCall function(m_client, v8Context(), v8Value(), "evaluate");
    function.appendArgument(expression);
    function.appendArgument(objectGroup);
    function.appendArgument(includeCommandLineAPI);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown, exceptionDetails);
}

void InjectedScript::callFunctionOn(ErrorString* errorString, const String& objectId, const String& expression, const String& arguments, bool returnByValue, bool generatePreview, RefPtr<TypeBuilder::Runtime::RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown)
{
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    ScriptFunctionCall function(m_client, v8Context(), v8Value(), "callFunctionOn");
    function.appendArgument(objectId);
    function.appendArgument(expression);
    function.appendArgument(arguments);
    function.appendArgument(returnByValue);
    function.appendArgument(generatePreview);
    makeEvalCall(errorString, function, result, wasThrown);
}

void InjectedScript::evaluateOnCallFrame(ErrorString* errorString, v8::Local<v8::Object> callFrames, bool isAsyncCallStack, const String& callFrameId, const String& expression, const String& objectGroup, bool includeCommandLineAPI, bool returnByValue, bool generatePreview, RefPtr<RemoteObject>* result, TypeBuilder::OptOutput<bool>* wasThrown, RefPtr<TypeBuilder::Debugger::ExceptionDetails>* exceptionDetails)
{
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    ScriptFunctionCall function(m_client, v8Context(), v8Value(), "evaluateOnCallFrame");
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
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    ScriptFunctionCall function(m_client, v8Context(), v8Value(), "restartFrame");
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

void InjectedScript::getStepInPositions(ErrorString* errorString, v8::Local<v8::Object> callFrames, const String& callFrameId, RefPtr<Array<TypeBuilder::Debugger::Location>>& positions)
{
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    ScriptFunctionCall function(m_client, v8Context(), v8Value(), "getStepInPositions");
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
            positions = Array<TypeBuilder::Debugger::Location>::runtimeCast(resultValue);
            return;
        }
    }
    *errorString = "Internal error";
}

void InjectedScript::setVariableValue(ErrorString* errorString, v8::Local<v8::Object> callFrames, const String* callFrameIdOpt, const String* functionObjectIdOpt, int scopeNumber, const String& variableName, const String& newValueStr)
{
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    ScriptFunctionCall function(m_client, v8Context(), v8Value(), "setVariableValue");
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
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    ScriptFunctionCall function(m_client, v8Context(), v8Value(), "getFunctionDetails");
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
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    ScriptFunctionCall function(m_client, v8Context(), v8Value(), "getGeneratorObjectDetails");
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

void InjectedScript::getCollectionEntries(ErrorString* errorString, const String& objectId, RefPtr<Array<CollectionEntry> >* result)
{
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    ScriptFunctionCall function(m_client, v8Context(), v8Value(), "getCollectionEntries");
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

void InjectedScript::getProperties(ErrorString* errorString, const String& objectId, bool ownProperties, bool accessorPropertiesOnly, bool generatePreview, RefPtr<Array<PropertyDescriptor>>* properties, RefPtr<TypeBuilder::Debugger::ExceptionDetails>* exceptionDetails)
{
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    ScriptFunctionCall function(m_client, v8Context(), v8Value(), "getProperties");
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

void InjectedScript::getInternalProperties(ErrorString* errorString, const String& objectId, RefPtr<Array<InternalPropertyDescriptor>>* properties, RefPtr<TypeBuilder::Debugger::ExceptionDetails>* exceptionDetails)
{
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    ScriptFunctionCall function(m_client, v8Context(), v8Value(), "getInternalProperties");
    function.appendArgument(objectId);

    RefPtr<JSONValue> result;
    makeCallWithExceptionDetails(function, &result, exceptionDetails);
    if (*exceptionDetails)
        return;
    if (!result || result->type() != JSONValue::TypeArray) {
        *errorString = "Internal error";
        return;
    }
    RefPtr<Array<InternalPropertyDescriptor> > array = Array<InternalPropertyDescriptor>::runtimeCast(result);
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
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    v8::Local<v8::Context> context = v8Context();
    ScriptFunctionCall function(m_client, context, v8Value(), "wrapCallFrames");
    function.appendArgument(callFrames);
    function.appendArgument(asyncOrdinal);
    bool hadException = false;
    v8::Local<v8::Value> callFramesValue = callFunctionWithEvalEnabled(function, hadException);
    ASSERT(!hadException);
    RefPtr<JSONValue> result = toJSONValue(context, callFramesValue);
    if (result && result->type() == JSONValue::TypeArray)
        return Array<CallFrame>::runtimeCast(result);
    return Array<CallFrame>::create();
}

PassRefPtr<TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapObject(const ScriptValue& value, const String& groupName, bool generatePreview) const
{
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    v8::Local<v8::Context> context = v8Context();
    ScriptFunctionCall function(m_client, context, v8Value(), "wrapObject");
    function.appendArgument(value.v8Value());
    function.appendArgument(groupName);
    function.appendArgument(canAccessInspectedWindow());
    function.appendArgument(generatePreview);
    bool hadException = false;
    v8::Local<v8::Value> r = callFunctionWithEvalEnabled(function, hadException);
    if (hadException)
        return nullptr;
    RefPtr<JSONObject> rawResult = toJSONValue(context, r)->asObject();
    return TypeBuilder::Runtime::RemoteObject::runtimeCast(rawResult);
}

PassRefPtr<TypeBuilder::Runtime::RemoteObject> InjectedScript::wrapTable(const ScriptValue& table, const ScriptValue& columns) const
{
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    v8::Local<v8::Context> context = v8Context();
    ScriptFunctionCall function(m_client, context, v8Value(), "wrapTable");
    function.appendArgument(canAccessInspectedWindow());
    function.appendArgument(table.v8Value());
    if (columns.isEmpty())
        function.appendArgument(false);
    else
        function.appendArgument(columns.v8Value());
    bool hadException = false;
    v8::Local<v8::Value>  r = callFunctionWithEvalEnabled(function, hadException);
    if (hadException)
        return nullptr;
    RefPtr<JSONObject> rawResult = toJSONValue(context, r)->asObject();
    return TypeBuilder::Runtime::RemoteObject::runtimeCast(rawResult);
}

v8::Local<v8::Value> InjectedScript::findObject(const RemoteObjectId& objectId) const
{
    return m_native->objectForId(objectId.id());
}

String InjectedScript::objectIdToObjectGroupName(const String& objectId) const
{
    RefPtr<JSONValue> parsedObjectId = parseJSON(objectId);
    if (!parsedObjectId)
        return String();
    RefPtr<JSONObject> object;
    if (!parsedObjectId->asObject(&object))
        return String();
    int boundId = 0;
    if (!object->getNumber("id", &boundId))
        return String();
    return m_native->groupName(boundId);
}

void InjectedScript::releaseObjectGroup(const String& objectGroup)
{
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    m_native->releaseObjectGroup(objectGroup);
    if (objectGroup == "console") {
        ScriptFunctionCall function(m_client, v8Context(), v8Value(), "clearLastEvaluationResult");
        bool hadException = false;
        callFunctionWithEvalEnabled(function, hadException);
        ASSERT(!hadException);
    }
}

void InjectedScript::setCustomObjectFormatterEnabled(bool enabled)
{
    ScriptState::Scope scope(m_injectedScriptObject.scriptState());
    ScriptFunctionCall function(m_client, v8Context(), v8Value(), "setCustomObjectFormatterEnabled");
    function.appendArgument(enabled);
    RefPtr<JSONValue> result;
    makeCall(function, &result);
}

bool InjectedScript::canAccessInspectedWindow() const
{
    ScriptState* scriptState = m_injectedScriptObject.scriptState();
    return scriptState && m_client->canAccessContext(scriptState->context());
}

v8::Local<v8::Context> InjectedScript::v8Context() const
{
    return m_injectedScriptObject.context();
}

v8::Local<v8::Value> InjectedScript::v8Value() const
{
    return m_injectedScriptObject.v8Value();
}

v8::Local<v8::Value> InjectedScript::callFunctionWithEvalEnabled(ScriptFunctionCall& function, bool& hadException) const
{
    v8::Local<v8::Context> context = v8Context();
    bool evalIsDisabled = !context->IsCodeGenerationFromStringsAllowed();
    // Temporarily enable allow evals for inspector.
    if (evalIsDisabled)
        context->AllowCodeGenerationFromStrings(true);
    v8::Local<v8::Value> resultValue = function.call(hadException);
    if (evalIsDisabled)
        context->AllowCodeGenerationFromStrings(false);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data", InspectorUpdateCountersEvent::data());
    return resultValue;
}

void InjectedScript::makeCall(ScriptFunctionCall& function, RefPtr<JSONValue>* result)
{
    if (!canAccessInspectedWindow()) {
        *result = JSONValue::null();
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

void InjectedScript::makeEvalCall(ErrorString* errorString, ScriptFunctionCall& function, RefPtr<TypeBuilder::Runtime::RemoteObject>* objectResult, TypeBuilder::OptOutput<bool>* wasThrown, RefPtr<TypeBuilder::Debugger::ExceptionDetails>* exceptionDetails)
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
    *objectResult = TypeBuilder::Runtime::RemoteObject::runtimeCast(resultObj);
    *wasThrown = wasThrownVal;
}

void InjectedScript::makeCallWithExceptionDetails(ScriptFunctionCall& function, RefPtr<JSONValue>* result, RefPtr<TypeBuilder::Debugger::ExceptionDetails>* exceptionDetails)
{
    v8::TryCatch tryCatch(m_isolate);
    v8::Local<v8::Value> resultValue = function.callWithoutExceptionHandling();
    if (tryCatch.HasCaught()) {
        v8::Local<v8::Message> message = tryCatch.Message();
        String text = !message.IsEmpty() ? toCoreStringWithUndefinedOrNullCheck(message->Get()) : "Internal error";
        *exceptionDetails = TypeBuilder::Debugger::ExceptionDetails::create().setText(text);
    } else {
        *result = toJSONValue(function.context(), resultValue);
        if (!*result)
            *result = JSONString::create(String::format("Object has too long reference chain(must not be longer than %d)", JSONValue::maxDepth));
    }
}

} // namespace blink
