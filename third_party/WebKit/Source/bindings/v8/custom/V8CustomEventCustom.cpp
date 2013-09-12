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

#include "config.h"
#include "V8CustomEvent.h"

#include "RuntimeEnabledFeatures.h"
#include "V8Event.h"
#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "core/dom/ContextFeatures.h"

namespace WebCore {

static v8::Handle<v8::Value> cacheState(v8::Handle<v8::Object> customEvent, v8::Handle<v8::Value> detail)
{
    customEvent->SetHiddenValue(V8HiddenPropertyName::detail(), detail);
    return detail;
}


void V8CustomEvent::detailAttributeGetterCustom(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    CustomEvent* event = V8CustomEvent::toNative(info.Holder());

    v8::Handle<v8::Value> result = info.Holder()->GetHiddenValue(V8HiddenPropertyName::detail());

    if (!result.IsEmpty()) {
        v8SetReturnValue(info, result);
        return;
    }

    if (!event->serializedDetail()) {
        // If we're in an isolated world and the event was created in the main world,
        // we need to find the 'detail' property on the main world wrapper and clone it.
        v8::Local<v8::Value> mainWorldDetail = getHiddenValueFromMainWorldWrapper(info.GetIsolate(), event, V8HiddenPropertyName::detail());
        if (!mainWorldDetail.IsEmpty())
            event->setSerializedDetail(SerializedScriptValue::createAndSwallowExceptions(mainWorldDetail, info.GetIsolate()));
    }

    if (event->serializedDetail()) {
        result = event->serializedDetail()->deserialize();
        v8SetReturnValue(info, cacheState(info.Holder(), result));
        return;
    }

    v8SetReturnValue(info, cacheState(info.Holder(), v8::Null(info.GetIsolate())));
}

void V8CustomEvent::initCustomEventMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    CustomEvent* event = V8CustomEvent::toNative(args.Holder());
    ASSERT(!event->serializedDetail());

    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, typeArg, args[0]);
    V8TRYCATCH_VOID(bool, canBubbleArg, args[1]->BooleanValue());
    V8TRYCATCH_VOID(bool, cancelableArg, args[2]->BooleanValue());
    v8::Handle<v8::Value> detailsArg = args[3];

    event->initEvent(typeArg, canBubbleArg, cancelableArg);

    if (!detailsArg.IsEmpty()) {
        args.Holder()->SetHiddenValue(V8HiddenPropertyName::detail(), detailsArg);
        if (isolatedWorldForIsolate(args.GetIsolate()))
            event->setSerializedDetail(SerializedScriptValue::createAndSwallowExceptions(detailsArg, args.GetIsolate()));
    }
}

} // namespace WebCore
