// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8InjectedScriptHost.h"

#include "platform/inspector_protocol/String16.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/InjectedScript.h"
#include "platform/v8_inspector/InjectedScriptHost.h"
#include "platform/v8_inspector/InspectorWrapper.h"
#include "platform/v8_inspector/JavaScriptCallFrame.h"
#include "platform/v8_inspector/V8Compat.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"
#include "platform/v8_inspector/public/V8EventListenerInfo.h"
#include "platform/v8_inspector/public/V8ToProtocolValue.h"
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
    if (!host->debugger())
        return;
    v8SetReturnValue(info, host->debugger()->client()->formatAccessorsAsProperties(info[0]));
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
    if (!host->debugger())
        return;
    String16 subtype = host->debugger()->client()->valueSubtype(value);
    if (!subtype.isEmpty()) {
        v8SetReturnValue(info, toV8String(isolate, subtype));
        return;
    }
}

void V8InjectedScriptHost::collectionEntriesCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1 || !info[0]->IsObject())
        return;

    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(info[0]);

    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    if (!host->debugger())
        return;
    v8SetReturnValue(info, host->debugger()->collectionEntries(object));
}

void V8InjectedScriptHost::getInternalPropertiesCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1 || !info[0]->IsObject())
        return;

    v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(info[0]);
    v8::MaybeLocal<v8::Array> properties = v8::Debug::GetInternalProperties(info.GetIsolate(), object);
    v8SetReturnValue(info, properties);
}

static v8::Local<v8::Array> wrapListenerFunctions(v8::Isolate* isolate, const V8EventListenerInfoList& listeners, const String16& type)
{
    v8::Local<v8::Array> result = v8::Array::New(isolate);
    size_t handlersCount = listeners.size();
    for (size_t i = 0, outputIndex = 0; i < handlersCount; ++i) {
        if (listeners[i].eventType != type)
            continue;
        v8::Local<v8::Object> function = listeners[i].handler;
        v8::Local<v8::Object> listenerEntry = v8::Object::New(isolate);
        listenerEntry->Set(toV8StringInternalized(isolate, "listener"), function);
        listenerEntry->Set(toV8StringInternalized(isolate, "useCapture"), v8::Boolean::New(isolate, listeners[i].useCapture));
        listenerEntry->Set(toV8StringInternalized(isolate, "passive"), v8::Boolean::New(isolate, listeners[i].passive));
        result->Set(v8::Number::New(isolate, outputIndex++), listenerEntry);
    }
    return result;
}

void V8InjectedScriptHost::getEventListenersCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(info.GetIsolate()->GetCurrentContext(), info.Holder());
    if (!host->debugger())
        return;
    V8DebuggerClient* client = host->debugger()->client();
    V8EventListenerInfoList listenerInfo;
    client->eventListeners(info[0], listenerInfo);

    v8::Local<v8::Object> result = v8::Object::New(info.GetIsolate());
    protocol::HashSet<String16> types;
    for (auto& info : listenerInfo)
        types.add(info.eventType);
    for (const auto& it : types) {
        v8::Local<v8::Array> listeners = wrapListenerFunctions(info.GetIsolate(), listenerInfo, it.first);
        if (!listeners->Length())
            continue;
        result->Set(toV8String(info.GetIsolate(), it.first), listeners);
    }

    v8SetReturnValue(info, result);
}

void V8InjectedScriptHost::inspectCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 2)
        return;

    v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
    InjectedScriptHost* host = V8InjectedScriptHost::unwrap(context, info.Holder());
    host->inspectImpl(toProtocolValue(context, info[0]), toProtocolValue(context, info[1]));
}

void V8InjectedScriptHost::callFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 2 || info.Length() > 3 || !info[0]->IsFunction()) {
        ASSERT_NOT_REACHED();
        return;
    }

    v8::MicrotasksScope microtasks(info.GetIsolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
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
    if (!host->debugger())
        return;
    host->debugger()->client()->muteWarningsAndDeprecations();
    callFunctionCallback(info);
    host->debugger()->client()->unmuteWarningsAndDeprecations();
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
    String16 groupName = toProtocolStringWithTypeCheck(v8groupName);
    int id = injectedScriptNative->bind(info[0], groupName);
    info.GetReturnValue().Set(id);
}

v8::Local<v8::Private> V8Debugger::scopeExtensionPrivate(v8::Isolate* isolate)
{
    return v8::Private::ForApi(isolate, toV8StringInternalized(isolate, "V8Debugger#scopeExtension"));
}

bool V8Debugger::isRemoteObjectAPIMethod(const String16& name)
{
    return name == "bindRemoteObject";
}

namespace {

char hiddenPropertyName[] = "v8inspector::InjectedScriptHost";
char className[] = "V8InjectedScriptHost";
using InjectedScriptHostWrapper = InspectorWrapper<InjectedScriptHost, hiddenPropertyName, className>;

const InjectedScriptHostWrapper::V8MethodConfiguration V8InjectedScriptHostMethods[] = {
    {"inspect", V8InjectedScriptHost::inspectCallback},
    {"internalConstructorName", V8InjectedScriptHost::internalConstructorNameCallback},
    {"formatAccessorsAsProperties", V8InjectedScriptHost::formatAccessorsAsProperties},
    {"isTypedArray", V8InjectedScriptHost::isTypedArrayCallback},
    {"subtype", V8InjectedScriptHost::subtypeCallback},
    {"collectionEntries", V8InjectedScriptHost::collectionEntriesCallback},
    {"getInternalProperties", V8InjectedScriptHost::getInternalPropertiesCallback},
    {"getEventListeners", V8InjectedScriptHost::getEventListenersCallback},
    {"callFunction", V8InjectedScriptHost::callFunctionCallback},
    {"suppressWarningsAndCallFunction", V8InjectedScriptHost::suppressWarningsAndCallFunctionCallback},
    {"setNonEnumProperty", V8InjectedScriptHost::setNonEnumPropertyCallback},
    {"bind", V8InjectedScriptHost::bindCallback}
};

} // namespace

v8::Local<v8::FunctionTemplate> V8InjectedScriptHost::createWrapperTemplate(v8::Isolate* isolate)
{
    protocol::Vector<InspectorWrapperBase::V8MethodConfiguration> methods(WTF_ARRAY_LENGTH(V8InjectedScriptHostMethods));
    std::copy(V8InjectedScriptHostMethods, V8InjectedScriptHostMethods + WTF_ARRAY_LENGTH(V8InjectedScriptHostMethods), methods.begin());
    protocol::Vector<InspectorWrapperBase::V8AttributeConfiguration> attributes;
    return InjectedScriptHostWrapper::createWrapperTemplate(isolate, methods, attributes);
}

v8::Local<v8::Object> V8InjectedScriptHost::wrap(v8::Local<v8::FunctionTemplate> constructorTemplate, v8::Local<v8::Context> context, InjectedScriptHost* host)
{
    return InjectedScriptHostWrapper::wrap(constructorTemplate, context, host);
}

InjectedScriptHost* V8InjectedScriptHost::unwrap(v8::Local<v8::Context> context, v8::Local<v8::Object> object)
{
    return InjectedScriptHostWrapper::unwrap(context, object);
}

} // namespace blink
