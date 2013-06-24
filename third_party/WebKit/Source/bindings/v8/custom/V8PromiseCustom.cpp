/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "bindings/v8/custom/V8PromiseCustom.h"

#include "V8Promise.h"
#include "V8PromiseResolver.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include <v8.h>

namespace WebCore {

void V8Promise::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8SetReturnValue(args, v8::Local<v8::Value>());
    v8::Isolate* isolate = args.GetIsolate();
    if (!args.Length() || args[0].IsEmpty() || !args[0]->IsFunction()) {
        throwTypeError("Promise constructor takes a function argument", isolate);
        return;
    }
    v8::Handle<v8::Function> init = args[0].As<v8::Function>();
    v8::Local<v8::ObjectTemplate> internalTemplate = v8::ObjectTemplate::New();
    internalTemplate->SetInternalFieldCount(V8PromiseCustom::InternalFieldCount);
    v8::Local<v8::Object> internal = internalTemplate->NewInstance();
    v8::Local<v8::Object> promise = V8DOMWrapper::createWrapper(args.Holder(), &V8Promise::info, 0, isolate);
    v8::Local<v8::Object> promiseResolver = V8DOMWrapper::createWrapper(args.Holder(), &V8PromiseResolver::info, 0, isolate);

    internal->SetInternalField(V8PromiseCustom::InternalStateIndex, v8::NumberObject::New(V8PromiseCustom::Pending));
    internal->SetInternalField(V8PromiseCustom::InternalResultIndex, v8::Undefined());
    internal->SetInternalField(V8PromiseCustom::InternalFulfillCallbackIndex, v8::Array::New());
    internal->SetInternalField(V8PromiseCustom::InternalRejectCallbackIndex, v8::Array::New());

    promise->SetInternalField(v8DOMWrapperObjectIndex, internal);
    promiseResolver->SetInternalField(v8DOMWrapperObjectIndex, internal);

    v8::Handle<v8::Value> argv[] = {
        promiseResolver,
    };
    v8::TryCatch trycatch;
    if (V8ScriptRunner::callFunction(init, getScriptExecutionContext(), promise, WTF_ARRAY_LENGTH(argv), argv).IsEmpty()) {
        // FIXME: An exception is thrown. Reject the promise.
    }
    v8SetReturnValue(args, promise);
    return;
}

} // namespace WebCore
