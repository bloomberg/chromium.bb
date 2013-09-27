/*
 * Copyright (C) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8PromiseCustom_h
#define V8PromiseCustom_h

#include "bindings/v8/WrapperTypeInfo.h"

#include <v8.h>

namespace WebCore {

class V8PromiseCustom {
public:
    enum InternalFieldIndex {
        InternalStateIndex,
        InternalResultIndex,
        InternalFulfillCallbackIndex,
        InternalRejectCallbackIndex,
        InternalFieldCount, // This entry must always be at the bottom.
    };

    enum WrapperCallbackEnvironmentFieldIndex {
        WrapperCallbackEnvironmentPromiseIndex,
        WrapperCallbackEnvironmentCallbackIndex,
        WrapperCallbackEnvironmentFieldCount, // This entry must always be at the bottom.
    };

    enum PromiseAllEnvironmentFieldIndex {
        PromiseAllEnvironmentPromiseIndex,
        PromiseAllEnvironmentCountdownIndex,
        PromiseAllEnvironmentIndexIndex,
        PromiseAllEnvironmentResultsIndex,
        PromiseAllEnvironmentFieldCount, // This entry must always be at the bottom.
    };

    enum PrimitiveWrapperFieldIndex {
        PrimitiveWrapperPrimitiveIndex,
        PrimitiveWrapperFieldCount, // This entry must always be at the bottom.
    };

    enum PromiseState {
        Pending,
        Fulfilled,
        Rejected,
        Following,
    };

    enum SynchronousMode {
        Synchronous,
        Asynchronous,
    };

    static v8::Local<v8::Object> createPromise(v8::Handle<v8::Object> creationContext, v8::Isolate*);

    // |promise| must be a Promise instance.
    static void fulfill(v8::Handle<v8::Object> promise, v8::Handle<v8::Value> result, SynchronousMode, v8::Isolate*);
    // |promise| must be a Promise instance.
    static void resolve(v8::Handle<v8::Object> promise, v8::Handle<v8::Value> result, SynchronousMode, v8::Isolate*);
    // |promise| must be a Promise instance.
    static void reject(v8::Handle<v8::Object> promise, v8::Handle<v8::Value> result, SynchronousMode, v8::Isolate*);

    // |promise| must be a Promise instance.
    // |fulfillCallback| and |rejectCallback| can be an empty function respectively.
    static void append(v8::Handle<v8::Object> promise, v8::Handle<v8::Function> fulfillCallback, v8::Handle<v8::Function> rejectCallback, v8::Isolate*);

    // |promise| must be a Promise instance.
    static v8::Local<v8::Object> getInternal(v8::Handle<v8::Object> promise);

    // |internal| must be a Promise internal object.
    // Clear the Promise internal object with the given state and result.
    // This function clears callbacks in the object.
    static void clearInternal(v8::Handle<v8::Object> internal, PromiseState, v8::Handle<v8::Value> result, v8::Isolate*);

    // |internal| must be a Promise internal object.
    static PromiseState getState(v8::Handle<v8::Object> internal);
    // |internal| must be a Promise internal object.
    static void setState(v8::Handle<v8::Object> internal, PromiseState, v8::Isolate*);

    // Call |function| synchronously or asynchronously, depending on |mode|.
    // If |function| throws an exception, this function catches it and does not rethrow.
    static void call(v8::Handle<v8::Function> /* function */, v8::Handle<v8::Object> receiver, v8::Handle<v8::Value> result, SynchronousMode /* mode */, v8::Isolate*);

    // Return true if |maybePromise| is a Promise object.
    static bool isPromise(v8::Handle<v8::Value> maybePromise, v8::Isolate*);

    // Coerces |maybePromise| to a Promise object.
    static v8::Local<v8::Object> toPromise(v8::Handle<v8::Value> maybePromise, v8::Isolate*);
};

} // namespace WebCore

#endif // V8PromiseCustom_h
