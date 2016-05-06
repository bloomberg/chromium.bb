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

#include "bindings/core/v8/V8EventTarget.h"

#include "bindings/core/v8/V8EventListenerList.h"
#include "bindings/core/v8/V8Window.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"

namespace blink {
namespace {

void addEventListenerMethodPrologueCustom(const v8::FunctionCallbackInfo<v8::Value>& info, EventTarget*)
{
    if (info.Length() >= 3 && info[2]->IsObject()) {
        UseCounter::countIfNotPrivateScript(info.GetIsolate(), currentExecutionContext(info.GetIsolate()),
            UseCounter::AddEventListenerThirdArgumentIsObject);
    }
    if (info.Length() >= 4) {
        UseCounter::countIfNotPrivateScript(info.GetIsolate(), currentExecutionContext(info.GetIsolate()),
            UseCounter::AddEventListenerFourArguments);

    }
}

void addEventListenerMethodEpilogueCustom(const v8::FunctionCallbackInfo<v8::Value>& info, EventTarget* impl)
{
    if (info.Length() >= 2 && info[1]->IsObject() && !impl->toNode())
        addHiddenValueToArray(info.GetIsolate(), info.Holder(), info[1], V8EventTarget::eventListenerCacheIndex);
}

void removeEventListenerMethodPrologueCustom(const v8::FunctionCallbackInfo<v8::Value>& info, EventTarget*)
{
    if (info.Length() >= 3 && info[2]->IsObject()) {
        UseCounter::countIfNotPrivateScript(info.GetIsolate(), currentExecutionContext(info.GetIsolate()),
            UseCounter::RemoveEventListenerThirdArgumentIsObject);
    }
    if (info.Length() >= 4) {
        UseCounter::countIfNotPrivateScript(info.GetIsolate(), currentExecutionContext(info.GetIsolate()),
            UseCounter::RemoveEventListenerFourArguments);

    }
}

void removeEventListenerMethodEpilogueCustom(const v8::FunctionCallbackInfo<v8::Value>& info, EventTarget* impl)
{
    if (info.Length() >= 2 && info[1]->IsObject() && !impl->toNode())
        removeHiddenValueFromArray(info.GetIsolate(), info.Holder(), info[1], V8EventTarget::eventListenerCacheIndex);
}

} // namespace

void V8EventTarget::addEventListenerMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "addEventListener", "EventTarget", info.Holder(), info.GetIsolate());
    if (UNLIKELY(info.Length() < 2)) {
        setMinimumArityTypeError(exceptionState, 2, info.Length());
        exceptionState.throwIfNeeded();
        return;
    }
    EventTarget* impl = V8EventTarget::toImpl(info.Holder());
    if (!BindingSecurity::shouldAllowAccessTo(info.GetIsolate(), callingDOMWindow(info.GetIsolate()), impl, exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }
    V8StringResource<> type = info[0];
    if (!type.prepare())
        return;
    EventListener* listener = V8EventListenerList::getEventListener(ScriptState::current(info.GetIsolate()), info[1], false, ListenerFindOrCreate);
    AddEventListenerOptionsOrBoolean options;
    // TODO(dtapuska): This custom binding code can be eliminated once
    // EventListenerOptions runtime enabled feature is removed.
    // http://crbug.com/545163
    if (UNLIKELY(info.Length() <= 2) || isUndefinedOrNull(info[2])) {
        addEventListenerMethodPrologueCustom(info, impl);
        impl->addEventListener(type, listener);
        addEventListenerMethodEpilogueCustom(info, impl);
        return;
    }
    V8AddEventListenerOptionsOrBoolean::toImpl(info.GetIsolate(), info[2], options, UnionTypeConversionMode::NotNullable, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    addEventListenerMethodPrologueCustom(info, impl);
    impl->addEventListener(type, listener, options);
    addEventListenerMethodEpilogueCustom(info, impl);
}

void V8EventTarget::removeEventListenerMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "removeEventListener", "EventTarget", info.Holder(), info.GetIsolate());
    if (UNLIKELY(info.Length() < 2)) {
        setMinimumArityTypeError(exceptionState, 2, info.Length());
        exceptionState.throwIfNeeded();
        return;
    }
    EventTarget* impl = V8EventTarget::toImpl(info.Holder());
    if (!BindingSecurity::shouldAllowAccessTo(info.GetIsolate(), callingDOMWindow(info.GetIsolate()), impl, exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }
    V8StringResource<> type = info[0];
    if (!type.prepare())
        return;
    EventListener* listener = V8EventListenerList::getEventListener(ScriptState::current(info.GetIsolate()), info[1], false, ListenerFindOnly);
    EventListenerOptionsOrBoolean options;
    // TODO(dtapuska): This custom binding code can be eliminated once
    // EventListenerOptions runtime enabled feature is removed.
    // http://crbug.com/545163
    if (UNLIKELY(info.Length() <= 2) || isUndefinedOrNull(info[2])) {
        removeEventListenerMethodPrologueCustom(info, impl);
        impl->removeEventListener(type, listener);
        removeEventListenerMethodEpilogueCustom(info, impl);
        return;
    }
    V8EventListenerOptionsOrBoolean::toImpl(info.GetIsolate(), info[2], options, UnionTypeConversionMode::NotNullable, exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    removeEventListenerMethodPrologueCustom(info, impl);
    impl->removeEventListener(type, listener, options);
    removeEventListenerMethodEpilogueCustom(info, impl);
}

} // namespace blink
