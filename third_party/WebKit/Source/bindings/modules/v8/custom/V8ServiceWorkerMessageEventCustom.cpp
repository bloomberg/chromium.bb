// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/V8ServiceWorkerMessageEvent.h"

#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/modules/v8/V8ServiceWorkerMessageEventInit.h"

namespace blink {

void V8ServiceWorkerMessageEvent::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "ServiceWorkerMessageEvent", info.Holder(), info.GetIsolate());
    if (UNLIKELY(info.Length() < 1)) {
        setMinimumArityTypeError(exceptionState, 1, info.Length());
        exceptionState.throwIfNeeded();
        return;
    }

    V8StringResource<> type = info[0];
    if (!type.prepare())
        return;

    ServiceWorkerMessageEventInit eventInitDict;
    if (!isUndefinedOrNull(info[1])) {
        if (!info[1]->IsObject()) {
            exceptionState.throwTypeError("parameter 2 ('eventInitDict') is not an object.");
            exceptionState.throwIfNeeded();
            return;
        }
        V8ServiceWorkerMessageEventInit::toImpl(info.GetIsolate(), info[1], eventInitDict, exceptionState);
        if (exceptionState.throwIfNeeded())
            return;
    }

    RefPtrWillBeRawPtr<ServiceWorkerMessageEvent> impl = ServiceWorkerMessageEvent::create(type, eventInitDict);
    v8::Local<v8::Object> wrapper = info.Holder();
    wrapper = impl->associateWithWrapper(info.GetIsolate(), &V8ServiceWorkerMessageEvent::wrapperTypeInfo, wrapper);

    // TODO(bashi): Workaround for http://crbug.com/529941. We need to store
    // |data| as a hidden value to avoid cycle references.
    if (eventInitDict.hasData()) {
        v8::Local<v8::Value> v8Data = eventInitDict.data().v8Value();
        V8HiddenValue::setHiddenValue(ScriptState::current(info.GetIsolate()), wrapper, V8HiddenValue::data(info.GetIsolate()), v8Data);
        if (DOMWrapperWorld::current(info.GetIsolate()).isIsolatedWorld())
            impl->setSerializedData(SerializedScriptValueFactory::instance().createAndSwallowExceptions(info.GetIsolate(), v8Data));
    }
    v8SetReturnValue(info, wrapper);
}

void V8ServiceWorkerMessageEvent::dataAttributeGetterCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ServiceWorkerMessageEvent* event = V8ServiceWorkerMessageEvent::toImpl(info.Holder());
    v8::Isolate* isolate = info.GetIsolate();
    ScriptState* scriptState = ScriptState::current(isolate);
    v8::Local<v8::Value> result = V8HiddenValue::getHiddenValue(scriptState, info.Holder(), V8HiddenValue::data(isolate));

    if (!result.IsEmpty()) {
        v8SetReturnValue(info, result);
        return;
    }

    v8::Local<v8::Value> data;
    if (SerializedScriptValue* serializedValue = event->serializedData()) {
        MessagePortArray ports = event->ports();
        data = serializedValue->deserialize(isolate, &ports);
    } else if (DOMWrapperWorld::current(isolate).isIsolatedWorld()) {
        v8::Local<v8::Value> mainWorldData = V8HiddenValue::getHiddenValueFromMainWorldWrapper(scriptState, event, V8HiddenValue::data(isolate));
        if (!mainWorldData.IsEmpty()) {
            // TODO(bashi): Enter the main world's ScriptState::Scope while
            // serializing the main world's value.
            event->setSerializedData(SerializedScriptValueFactory::instance().createAndSwallowExceptions(info.GetIsolate(), mainWorldData));
            data = event->serializedData()->deserialize();
        }
    }
    if (data.IsEmpty())
        data = v8::Null(isolate);
    V8HiddenValue::setHiddenValue(scriptState, info.Holder(), V8HiddenValue::data(isolate), data);
    v8SetReturnValue(info, data);
}

} // namespace blink
