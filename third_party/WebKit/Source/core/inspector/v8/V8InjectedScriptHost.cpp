// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/v8/V8InjectedScriptHost.h"

#include "core/inspector/v8/EventListenerInfo.h"
#include "core/inspector/v8/InjectedScript.h"
#include "core/inspector/v8/InjectedScriptHost.h"
#include "core/inspector/v8/InspectorWrapper.h"
#include "core/inspector/v8/JavaScriptCallFrame.h"
#include "core/inspector/v8/V8DebuggerClient.h"
#include "core/inspector/v8/V8DebuggerImpl.h"
#include "core/inspector/v8/V8StringUtil.h"
#include "platform/JSONValues.h"
#include "platform/JSONValuesForV8.h"
#include "wtf/NonCopyingSort.h"
#include "wtf/RefPtr.h"
#include "wtf/StdLibExtras.h"
#include <algorithm>

namespace blink {

namespace {

template<typename CallbackInfo, typename S>
inline void v8SetReturnValue(const CallbackInfo& info, const v8::Local<S> handle)
{
    info.GetReturnValue().Set(handle);
}

template<typename CallbackInfo, typename S>
inline void v8SetReturnValue(const CallbackInfo& info, v8::MaybeLocal<S> maybe)
{
    if (LIKELY(!maybe.IsEmpty()))
        info.GetReturnValue().Set(maybe.ToLocalChecked());
}

template<typename CallbackInfo>
inline void v8SetReturnValue(const CallbackInfo& info, bool value)
{
    info.GetReturnValue().Set(value);
}

}

void V8InjectedScriptHost::clearConsoleMessagesCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    InjectedScriptHost* impl = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    impl->clearConsoleMessages();
}

void V8InjectedScriptHost::inspectedObjectCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;

    v8::Isolate* isolate = info.GetIsolate();
    if (!info[0]->IsInt32() && !isolate->IsExecutionTerminating()) {
        isolate->ThrowException(v8::Exception::TypeError(toV8String(isolate, "argument has to be an integer")));
        return;
    }

    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(context, info.Holder());
    InjectedScriptHost::InspectableObject* object = host->inspectedObject(info[0].As<v8::Int32>()->Value());
    v8SetReturnValue(info, object->get(context));
}

static v8::Local<v8::String> functionDisplayName(v8::Local<v8::Function> function)
{
    v8::Local<v8::Value> value = function->GetDebugName();
    if (value->IsString() && v8::Local<v8::String>::Cast(value)->Length())
        return v8::Local<v8::String>::Cast(value);
    return v8::Local<v8::String>();
}

void V8InjectedScriptHost::internalConstructorNameCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1 || !info[0]->IsObject())
        return;

    v8::Local<v8::Object> object = info[0].As<v8::Object>();
    v8::Local<v8::String> result = object->GetConstructorName();

    v8SetReturnValue(info, result);
}

void V8InjectedScriptHost::formatAccessorsAsProperties(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    v8SetReturnValue(info, host->debugger().client()->formatAccessorsAsProperties(info[0]));
}

void V8InjectedScriptHost::isTypedArrayCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;
    v8SetReturnValue(info, info[0]->IsTypedArray());
}

void V8InjectedScriptHost::subtypeCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;
    v8::Isolate* isolate = info.GetIsolate();

    v8::Local<v8::Value> value = info[0];
    if (value->IsArray() || value->IsTypedArray() || value->IsArgumentsObject()) {
        v8SetReturnValue(info, toV8StringInternalized(isolate, "array"));
        return;
    }
    if (value->IsDate()) {
        v8SetReturnValue(info, toV8StringInternalized(isolate, "date"));
        return;
    }
    if (value->IsRegExp()) {
        v8SetReturnValue(info, toV8StringInternalized(isolate, "regexp"));
        return;
    }
    if (value->IsMap() || value->IsWeakMap()) {
        v8SetReturnValue(info, toV8StringInternalized(isolate, "map"));
        return;
    }
    if (value->IsSet() || value->IsWeakSet()) {
        v8SetReturnValue(info, toV8StringInternalized(isolate, "set"));
        return;
    }
    if (value->IsMapIterator() || value->IsSetIterator()) {
        v8SetReturnValue(info, toV8StringInternalized(isolate, "iterator"));
        return;
    }
    if (value->IsGeneratorObject()) {
        v8SetReturnValue(info, toV8StringInternalized(isolate, "generator"));
        return;
    }

    if (value->IsNativeError()) {
        v8SetReturnValue(info, toV8StringInternalized(isolate, "error"));
        return;
    }

    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    String subtype = host->debugger().client()->valueSubtype(value);
    if (!subtype.isEmpty()) {
        v8SetReturnValue(info, toV8String(isolate, subtype));
        return;
    }
}

void V8InjectedScriptHost::functionDetailsCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1 || !info[0]->IsFunction())
        return;

    v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(info[0]);
    int lineNumber = function->GetScriptLineNumber();
    int columnNumber = function->GetScriptColumnNumber();

    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Object> location = v8::Object::New(isolate);
    location->Set(toV8StringInternalized(isolate, "lineNumber"), v8::Integer::New(isolate, lineNumber));
    location->Set(toV8StringInternalized(isolate, "columnNumber"), v8::Integer::New(isolate, columnNumber));
    location->Set(toV8StringInternalized(isolate, "scriptId"), v8::Integer::New(isolate, function->ScriptId())->ToString(isolate));

    v8::Local<v8::Object> result = v8::Object::New(isolate);
    result->Set(toV8StringInternalized(isolate, "location"), location);

    v8::Local<v8::String> name = functionDisplayName(function);
    result->Set(toV8StringInternalized(isolate, "functionName"), name.IsEmpty() ? toV8StringInternalized(isolate, "") : name);
    result->Set(toV8StringInternalized(isolate, "isGenerator"), v8::Boolean::New(isolate, function->IsGeneratorFunction()));

    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    V8DebuggerImpl& debugger = host->debugger();
    v8::MaybeLocal<v8::Value> scopes = debugger.functionScopes(function);
    if (!scopes.IsEmpty() && scopes.ToLocalChecked()->IsArray())
        result->Set(toV8StringInternalized(isolate, "rawScopes"), scopes.ToLocalChecked());

    v8SetReturnValue(info, result);
}

void V8InjectedScriptHost::generatorObjectDetailsCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1 || !info[0]->IsObject())
        return;

    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(info[0]);

    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    V8DebuggerImpl& debugger = host->debugger();
    v8SetReturnValue(info, debugger.generatorObjectDetails(object));
}

void V8InjectedScriptHost::collectionEntriesCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1 || !info[0]->IsObject())
        return;

    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(info[0]);

    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    V8DebuggerImpl& debugger = host->debugger();
    v8SetReturnValue(info, debugger.collectionEntries(object));
}

void V8InjectedScriptHost::getInternalPropertiesCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1 || !info[0]->IsObject())
        return;

    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(info[0]);
    v8::MaybeLocal<v8::Array> properties = v8::Debug::GetInternalProperties(info.GetIsolate(), object);
    v8SetReturnValue(info, properties);
}

static v8::Local<v8::Array> wrapListenerFunctions(v8::Isolate* isolate, const Vector<EventListenerInfo>& listeners)
{
    v8::Local<v8::Array> result = v8::Array::New(isolate);
    size_t handlersCount = listeners.size();
    for (size_t i = 0, outputIndex = 0; i < handlersCount; ++i) {
        v8::Local<v8::Object> function = listeners[i].handler;
        v8::Local<v8::Object> listenerEntry = v8::Object::New(isolate);
        listenerEntry->Set(toV8StringInternalized(isolate, "listener"), function);
        listenerEntry->Set(toV8StringInternalized(isolate, "useCapture"), v8::Boolean::New(isolate, listeners[i].useCapture));
        result->Set(v8::Number::New(isolate, outputIndex++), listenerEntry);
    }
    return result;
}

void V8InjectedScriptHost::getEventListenersCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    V8DebuggerClient* client = host->debugger().client();
    EventListenerInfoMap listenerInfo;
    client->eventListeners(info[0], listenerInfo);

    v8::Local<v8::Object> result = v8::Object::New(info.GetIsolate());
    Vector<String> types;
    for (auto& it : listenerInfo)
        types.append(it.key);
    nonCopyingSort(types.begin(), types.end(), WTF::codePointCompareLessThan);
    for (const String& type : types) {
        v8::Local<v8::Array> listeners = wrapListenerFunctions(info.GetIsolate(), *listenerInfo.get(type));
        if (!listeners->Length())
            continue;
        result->Set(toV8String(info.GetIsolate(), type), listeners);
    }

    v8SetReturnValue(info, result);
}

void V8InjectedScriptHost::inspectCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 2)
        return;

    v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(context, info.Holder());
    host->inspectImpl(toJSONValue(context, info[0]), toJSONValue(context, info[1]));
}

void V8InjectedScriptHost::evalCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    if (info.Length() < 1) {
        isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, "One argument expected.")));
        return;
    }

    v8::Local<v8::String> expression = info[0]->ToString(isolate);
    if (expression.IsEmpty()) {
        isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, "The argument must be a string.")));
        return;
    }

    ASSERT(isolate->InContext());
    v8::TryCatch tryCatch(isolate);
    v8::Local<v8::Value> result;
    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(isolate->GetCurrentContext(), info.Holder());
    if (!host->debugger().client()->compileAndRunInternalScript(expression).ToLocal(&result)) {
        v8SetReturnValue(info, tryCatch.ReThrow());
        return;
    }
    v8SetReturnValue(info, result);
}

static void setExceptionAsReturnValue(const v8::FunctionCallbackInfo<v8::Value>& info, v8::Local<v8::Object> returnValue, v8::TryCatch& tryCatch)
{
    v8::Isolate* isolate = info.GetIsolate();
    returnValue->Set(v8::String::NewFromUtf8(isolate, "result"), tryCatch.Exception());
    returnValue->Set(v8::String::NewFromUtf8(isolate, "exceptionDetails"), JavaScriptCallFrame::createExceptionDetails(isolate, tryCatch.Message()));
    v8SetReturnValue(info, returnValue);
}

void V8InjectedScriptHost::evaluateWithExceptionDetailsCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    if (info.Length() < 1) {
        isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, "One argument expected.")));
        return;
    }

    v8::Local<v8::String> expression = info[0]->ToString(isolate);
    if (expression.IsEmpty()) {
        isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(isolate, "The argument must be a string.")));
        return;
    }

    ASSERT(isolate->InContext());
    v8::Local<v8::Object> wrappedResult = v8::Object::New(isolate);
    if (wrappedResult.IsEmpty())
        return;

    v8::TryCatch tryCatch(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(context, info.Holder());

    v8::Local<v8::Script> script = host->debugger().compileInternalScript(context, expression, String());
    if (script.IsEmpty()) {
        setExceptionAsReturnValue(info, wrappedResult, tryCatch);
        return;
    }

    v8::Local<v8::Symbol> commandLineAPISymbolValue = commandLineAPISymbol(isolate);
    v8::Local<v8::Object> global = context->Global();
    if (info.Length() >= 2 && info[1]->IsObject()) {
        v8::Local<v8::Object> commandLineAPI = info[1]->ToObject(isolate);
        global->Set(commandLineAPISymbolValue, commandLineAPI);
    }

    v8::MaybeLocal<v8::Value> result = host->debugger().client()->runCompiledScript(context, script);
    if (result.IsEmpty()) {
        global->Delete(context, commandLineAPISymbolValue);
        setExceptionAsReturnValue(info, wrappedResult, tryCatch);
        return;
    }

    global->Delete(context, commandLineAPISymbolValue);
    wrappedResult->Set(v8::String::NewFromUtf8(isolate, "result"), result.ToLocalChecked());
    wrappedResult->Set(v8::String::NewFromUtf8(isolate, "exceptionDetails"), v8::Undefined(isolate));
    v8SetReturnValue(info, wrappedResult);
}

void V8InjectedScriptHost::setFunctionVariableValueCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 4 || !info[0]->IsFunction() || !info[1]->IsInt32() || !info[2]->IsString())
        return;

    v8::Local<v8::Value> functionValue = info[0];
    int scopeIndex = info[1].As<v8::Int32>()->Value();
    String variableName = toWTFStringWithTypeCheck(info[2]);
    v8::Local<v8::Value> newValue = info[3];

    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    v8SetReturnValue(info, host->debugger().setFunctionVariableValue(functionValue, scopeIndex, variableName, newValue));
}

static bool getFunctionLocation(const v8::FunctionCallbackInfo<v8::Value>& info, String* scriptId, int* lineNumber, int* columnNumber)
{
    if (info.Length() < 1 || !info[0]->IsFunction())
        return false;
    v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(info[0]);
    *lineNumber = function->GetScriptLineNumber();
    *columnNumber = function->GetScriptColumnNumber();
    if (*lineNumber == v8::Function::kLineOffsetNotFound || *columnNumber == v8::Function::kLineOffsetNotFound)
        return false;
    *scriptId = String::number(function->ScriptId());
    return true;
}

void V8InjectedScriptHost::debugFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    String scriptId;
    int lineNumber;
    int columnNumber;
    if (!getFunctionLocation(info, &scriptId, &lineNumber, &columnNumber))
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    host->debugFunction(scriptId, lineNumber, columnNumber);
}

void V8InjectedScriptHost::undebugFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    String scriptId;
    int lineNumber;
    int columnNumber;
    if (!getFunctionLocation(info, &scriptId, &lineNumber, &columnNumber))
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    host->undebugFunction(scriptId, lineNumber, columnNumber);
}

void V8InjectedScriptHost::monitorFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    String scriptId;
    int lineNumber;
    int columnNumber;
    if (!getFunctionLocation(info, &scriptId, &lineNumber, &columnNumber))
        return;

    v8::Local<v8::Value> name;
    if (info.Length() > 0 && info[0]->IsFunction()) {
        v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(info[0]);
        name = function->GetName();
        if (!name->IsString() || !v8::Local<v8::String>::Cast(name)->Length())
            name = function->GetInferredName();
    }

    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    host->monitorFunction(scriptId, lineNumber, columnNumber, toWTFStringWithTypeCheck(name));
}

void V8InjectedScriptHost::unmonitorFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    String scriptId;
    int lineNumber;
    int columnNumber;
    if (!getFunctionLocation(info, &scriptId, &lineNumber, &columnNumber))
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    host->unmonitorFunction(scriptId, lineNumber, columnNumber);
}

void V8InjectedScriptHost::callFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 2 || info.Length() > 3 || !info[0]->IsFunction()) {
        ASSERT_NOT_REACHED();
        return;
    }

    v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(info[0]);
    v8::Local<v8::Value> receiver = info[1];

    if (info.Length() < 3 || info[2]->IsUndefined()) {
        v8::Local<v8::Value> result = function->Call(receiver, 0, 0);
        v8SetReturnValue(info, result);
        return;
    }

    if (!info[2]->IsArray()) {
        ASSERT_NOT_REACHED();
        return;
    }

    v8::Local<v8::Array> arguments = v8::Local<v8::Array>::Cast(info[2]);
    size_t argc = arguments->Length();
    OwnPtr<v8::Local<v8::Value>[]> argv = adoptArrayPtr(new v8::Local<v8::Value>[argc]);
    for (size_t i = 0; i < argc; ++i) {
        if (!arguments->Get(info.GetIsolate()->GetCurrentContext(), v8::Integer::New(info.GetIsolate(), i)).ToLocal(&argv[i]))
            return;
    }

    v8::Local<v8::Value> result = function->Call(receiver, argc, argv.get());
    v8SetReturnValue(info, result);
}

void V8InjectedScriptHost::suppressWarningsAndCallFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    host->client()->muteWarningsAndDeprecations();

    callFunctionCallback(info);

    host->client()->unmuteWarningsAndDeprecations();
}

void V8InjectedScriptHost::setNonEnumPropertyCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 3 || !info[0]->IsObject() || !info[1]->IsString())
        return;

    v8::Local<v8::Object> object = info[0].As<v8::Object>();
    v8::Maybe<bool> success = object->DefineOwnProperty(info.GetIsolate()->GetCurrentContext(), info[1].As<v8::String>(), info[2], v8::DontEnum);
    ASSERT_UNUSED(!success.IsNothing(), !success.IsNothing());
}

void V8InjectedScriptHost::bindCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 2 || !info[1]->IsString())
        return;
    InjectedScriptNative* injectedScriptNative = InjectedScriptNative::fromInjectedScriptHost(info.Holder());
    if (!injectedScriptNative)
        return;

    v8::Local<v8::String> v8groupName = info[1]->ToString(info.GetIsolate());
    String groupName = toWTFStringWithTypeCheck(v8groupName);
    int id = injectedScriptNative->bind(info[0], groupName);
    info.GetReturnValue().Set(id);
}

void V8InjectedScriptHost::objectForIdCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1 || !info[0]->IsInt32())
        return;
    InjectedScriptNative* injectedScriptNative = InjectedScriptNative::fromInjectedScriptHost(info.Holder());
    if (!injectedScriptNative)
        return;
    int id = info[0].As<v8::Int32>()->Value();
    v8::Local<v8::Value> value = injectedScriptNative->objectForId(id);
    if (!value.IsEmpty())
        info.GetReturnValue().Set(value);
}

void V8InjectedScriptHost::idToObjectGroupNameCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1 || !info[0]->IsInt32())
        return;
    InjectedScriptNative* injectedScriptNative = InjectedScriptNative::fromInjectedScriptHost(info.Holder());
    if (!injectedScriptNative)
        return;
    int id = info[0].As<v8::Int32>()->Value();
    String groupName = injectedScriptNative->groupName(id);
    if (!groupName.isEmpty())
        info.GetReturnValue().Set(toV8String(info.GetIsolate(), groupName));
}

v8::Local<v8::Symbol> V8InjectedScriptHost::commandLineAPISymbol(v8::Isolate* isolate)
{
    return v8::Symbol::ForApi(isolate, toV8StringInternalized(isolate, "commandLineAPI"));
}

bool V8InjectedScriptHost::isCommandLineAPIMethod(const AtomicString& name)
{
    DEFINE_STATIC_LOCAL(HashSet<String>, methods, ());
    if (methods.size() == 0) {
        const char* members[] = { "$", "$$", "$x", "dir", "dirxml", "keys", "values", "profile", "profileEnd",
            "monitorEvents", "unmonitorEvents", "inspect", "copy", "clear", "getEventListeners",
            "debug", "undebug", "monitor", "unmonitor", "table", "$_", "$0", "$1", "$2", "$3", "$4" };
        for (size_t i = 0; i < sizeof(members) / sizeof(const char*); ++i)
            methods.add(members[i]);
    }
    return methods.find(name) != methods.end();
}

namespace {

char hiddenPropertyName[] = "v8inspector::InjectedScriptHost";
char className[] = "V8InjectedScriptHost";
using InjectedScriptHostWrapper = InspectorWrapper<InjectedScriptHost, hiddenPropertyName, className>;

const InjectedScriptHostWrapper::V8MethodConfiguration V8InjectedScriptHostMethods[] = {
    {"clearConsoleMessages", V8InjectedScriptHost::clearConsoleMessagesCallback},
    {"inspect", V8InjectedScriptHost::inspectCallback},
    {"inspectedObject", V8InjectedScriptHost::inspectedObjectCallback},
    {"internalConstructorName", V8InjectedScriptHost::internalConstructorNameCallback},
    {"formatAccessorsAsProperties", V8InjectedScriptHost::formatAccessorsAsProperties},
    {"isTypedArray", V8InjectedScriptHost::isTypedArrayCallback},
    {"subtype", V8InjectedScriptHost::subtypeCallback},
    {"functionDetails", V8InjectedScriptHost::functionDetailsCallback},
    {"generatorObjectDetails", V8InjectedScriptHost::generatorObjectDetailsCallback},
    {"collectionEntries", V8InjectedScriptHost::collectionEntriesCallback},
    {"getInternalProperties", V8InjectedScriptHost::getInternalPropertiesCallback},
    {"getEventListeners", V8InjectedScriptHost::getEventListenersCallback},
    {"eval", V8InjectedScriptHost::evalCallback},
    {"evaluateWithExceptionDetails", V8InjectedScriptHost::evaluateWithExceptionDetailsCallback},
    {"debugFunction", V8InjectedScriptHost::debugFunctionCallback},
    {"undebugFunction", V8InjectedScriptHost::undebugFunctionCallback},
    {"monitorFunction", V8InjectedScriptHost::monitorFunctionCallback},
    {"unmonitorFunction", V8InjectedScriptHost::unmonitorFunctionCallback},
    {"callFunction", V8InjectedScriptHost::callFunctionCallback},
    {"suppressWarningsAndCallFunction", V8InjectedScriptHost::suppressWarningsAndCallFunctionCallback},
    {"setNonEnumProperty", V8InjectedScriptHost::setNonEnumPropertyCallback},
    {"setFunctionVariableValue", V8InjectedScriptHost::setFunctionVariableValueCallback},
    {"bind", V8InjectedScriptHost::bindCallback},
    {"objectForId", V8InjectedScriptHost::objectForIdCallback},
    {"idToObjectGroupName", V8InjectedScriptHost::idToObjectGroupNameCallback},
};

} // namespace

v8::Local<v8::FunctionTemplate> V8InjectedScriptHost::createWrapperTemplate(v8::Isolate* isolate)
{
    Vector<InspectorWrapperBase::V8MethodConfiguration> methods(WTF_ARRAY_LENGTH(V8InjectedScriptHostMethods));
    std::copy(V8InjectedScriptHostMethods, V8InjectedScriptHostMethods + WTF_ARRAY_LENGTH(V8InjectedScriptHostMethods), methods.begin());
    Vector<InspectorWrapperBase::V8AttributeConfiguration> attributes;
    return InjectedScriptHostWrapper::createWrapperTemplate(isolate, methods, attributes);
}

v8::Local<v8::Object> V8InjectedScriptHost::wrap(V8DebuggerClient* client, v8::Local<v8::FunctionTemplate> constructorTemplate, v8::Local<v8::Context> context, PassRefPtr<InjectedScriptHost> host)
{
    return InjectedScriptHostWrapper::wrap(client, constructorTemplate, context, host);
}

InjectedScriptHost* V8InjectedScriptHost::unwrap(v8::Local<v8::Context> context, v8::Local<v8::Object> object)
{
    return InjectedScriptHostWrapper::unwrap(context, object);
}

} // namespace blink
