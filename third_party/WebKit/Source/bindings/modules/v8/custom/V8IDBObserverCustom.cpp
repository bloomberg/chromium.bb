// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/V8IDBObserver.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMWrapper.h"
#include "bindings/modules/v8/V8IDBObserverCallback.h"
#include "bindings/modules/v8/V8IDBObserverInit.h"

namespace blink {

void V8IDBObserver::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(info.GetIsolate(), ExceptionState::ConstructionContext, "IDBObserver");

    if (UNLIKELY(info.Length() < 1)) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(1, info.Length()));
        return;
    }

    v8::Local<v8::Object> wrapper = info.Holder();

    if (!info[0]->IsFunction()) {
        exceptionState.throwTypeError("The callback provided as parameter 1 is not a function.");
        return;
    }

    if (info.Length() > 1 && !isUndefinedOrNull(info[1]) && !info[1]->IsObject()) {
        exceptionState.throwTypeError("parameter 2 ('options') is not an object.");
        return;
    }

    IDBObserverInit idbObserverInit;
    V8IDBObserverInit::toImpl(info.GetIsolate(), info[1], idbObserverInit, exceptionState);
    if (exceptionState.hadException())
        return;

    IDBObserverCallback* callback = new V8IDBObserverCallback(v8::Local<v8::Function>::Cast(info[0]), wrapper, ScriptState::current(info.GetIsolate()));
    IDBObserver* observer = IDBObserver::create(*callback, idbObserverInit);
    if (exceptionState.hadException())
        return;
    DCHECK(observer);
    v8SetReturnValue(info, V8DOMWrapper::associateObjectWithWrapper(info.GetIsolate(), observer, &wrapperTypeInfo, wrapper));
}

} // namespace blink
