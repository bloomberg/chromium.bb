/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "V8MessageEvent.h"

#include "V8Blob.h"
#include "V8MessagePort.h"
#include "V8Window.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "bindings/v8/custom/V8ArrayBufferCustom.h"
#include "core/dom/MessageEvent.h"

namespace WebCore {

void V8MessageEvent::dataAttributeGetterCustom(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    MessageEvent* event = V8MessageEvent::toNative(info.Holder());

    v8::Handle<v8::Value> result;
    switch (event->dataType()) {
    case MessageEvent::DataTypeScriptValue: {
        result = info.Holder()->GetHiddenValue(V8HiddenPropertyName::data());
        if (result.IsEmpty()) {
            if (!event->dataAsSerializedScriptValue()) {
                // If we're in an isolated world and the event was created in the main world,
                // we need to find the 'data' property on the main world wrapper and clone it.
                v8::Local<v8::Value> mainWorldData = getHiddenValueFromMainWorldWrapper(info.GetIsolate(), event, V8HiddenPropertyName::data());
                if (!mainWorldData.IsEmpty())
                    event->setSerializedData(SerializedScriptValue::createAndSwallowExceptions(mainWorldData, info.GetIsolate()));
            }
            if (event->dataAsSerializedScriptValue())
                result = event->dataAsSerializedScriptValue()->deserialize(info.GetIsolate());
            else
                result = v8::Null(info.GetIsolate());
        }
        break;
    }

    case MessageEvent::DataTypeSerializedScriptValue:
        if (SerializedScriptValue* serializedValue = event->dataAsSerializedScriptValue()) {
            MessagePortArray ports = event->ports();
            result = serializedValue->deserialize(info.GetIsolate(), &ports);
        } else {
            result = v8::Null(info.GetIsolate());
        }
        break;

    case MessageEvent::DataTypeString: {
        String stringValue = event->dataAsString();
        result = v8String(stringValue, info.GetIsolate());
        break;
    }

    case MessageEvent::DataTypeBlob:
        result = toV8(event->dataAsBlob(), info.Holder(), info.GetIsolate());
        break;

    case MessageEvent::DataTypeArrayBuffer:
        result = toV8(event->dataAsArrayBuffer(), info.Holder(), info.GetIsolate());
        break;
    }

    // Overwrite the data attribute so it returns the cached result in future invocations.
    // This custom handler (dataAccessGetter) will not be called again.
    v8::PropertyAttribute dataAttr = static_cast<v8::PropertyAttribute>(v8::DontDelete | v8::ReadOnly);
    info.Holder()->ForceSet(name, result, dataAttr);
    v8SetReturnValue(info, result);
}

void V8MessageEvent::initMessageEventMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    MessageEvent* event = V8MessageEvent::toNative(args.Holder());
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, typeArg, args[0]);
    V8TRYCATCH_VOID(bool, canBubbleArg, args[1]->BooleanValue());
    V8TRYCATCH_VOID(bool, cancelableArg, args[2]->BooleanValue());
    v8::Handle<v8::Value> dataArg = args[3];
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, originArg, args[4]);
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, lastEventIdArg, args[5]);

    DOMWindow* sourceArg = 0;
    if (args[6]->IsObject()) {
        v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(args[6]);
        v8::Handle<v8::Object> window = wrapper->FindInstanceInPrototypeChain(V8Window::GetTemplate(args.GetIsolate(), worldTypeInMainThread(args.GetIsolate())));
        if (!window.IsEmpty())
            sourceArg = V8Window::toNative(window);
    }
    OwnPtr<MessagePortArray> portArray;

    if (!isUndefinedOrNull(args[7])) {
        portArray = adoptPtr(new MessagePortArray);
        if (!getMessagePortArray(args[7], *portArray, args.GetIsolate()))
            return;
    }
    event->initMessageEvent(typeArg, canBubbleArg, cancelableArg, originArg, lastEventIdArg, sourceArg, portArray.release());

    if (!dataArg.IsEmpty()) {
        args.Holder()->SetHiddenValue(V8HiddenPropertyName::data(), dataArg);
        if (isolatedWorldForIsolate(args.GetIsolate()))
            event->setSerializedData(SerializedScriptValue::createAndSwallowExceptions(dataArg, args.GetIsolate()));
    }
}

void V8MessageEvent::webkitInitMessageEventMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    initMessageEventMethodCustom(args);
}


} // namespace WebCore
