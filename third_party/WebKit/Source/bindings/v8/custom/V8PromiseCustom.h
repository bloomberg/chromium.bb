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
        InternalDerivedPromiseIndex,
        InternalFieldCount, // This entry must always be at the bottom.
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

    static v8::Local<v8::Object> createPromise(v8::Handle<v8::Object> creationContext, v8::Isolate*);

    // |promise| must be a Promise instance.
    static v8::Local<v8::Object> getInternal(v8::Handle<v8::Object> promise);

    // |internal| must be a Promise internal object.
    static PromiseState getState(v8::Handle<v8::Object> internal);

    // Return true if |maybePromise| is a Promise instance.
    static bool isPromise(v8::Handle<v8::Value> maybePromise, v8::Isolate*);

    // Coerces |maybePromise| to a Promise instance.
    static v8::Local<v8::Object> toPromise(v8::Handle<v8::Value> maybePromise, v8::Isolate*);

    // |promise| must be a Promise instance.
    static void resolve(v8::Handle<v8::Object> promise, v8::Handle<v8::Value> result, v8::Isolate*);

    // |promise| must be a Promise instance.
    static void reject(v8::Handle<v8::Object> promise, v8::Handle<v8::Value> result, v8::Isolate*);

    // |promise| must be a Promise instance.
    // |onFulfilled| and |onRejected| can be an empty value respectively.
    // Appends |onFulfilled| and/or |onRejected| handlers to |promise|.
    static v8::Local<v8::Object> then(v8::Handle<v8::Object> promise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Isolate*);

    // |promise| must be a Promise instance.
    // Set a |promise|'s value and propagate it to derived promises.
    static void setValue(v8::Handle<v8::Object> promise, v8::Handle<v8::Value>, v8::Isolate*);

    // |promise| must be a Promise instance.
    // Set a |promise|'s failure reason and propagate it to derived promises.
    static void setReason(v8::Handle<v8::Object> promise, v8::Handle<v8::Value>, v8::Isolate*);

    // |promise| must be a Promise instance.
    // Propagate a |promise|'s value or reason to all of its derived promies.
    static void propagateToDerived(v8::Handle<v8::Object> promise, v8::Isolate*);

    // |derivedPromise| and |originator| must be a Promise instance.
    // |onFulfilled| and |onRejected| can be an empty value respectively.
    // Propagate |originator|'s state to |derivedPromise|.
    static void updateDerived(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Object> originator, v8::Isolate*);

    // |derivedPromise| must be a Promise instance.
    // Propagate a value to |derivedPromise|.
    static void updateDerivedFromValue(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Value>, v8::Isolate*);

    // |derivedPromise| must be a Promise instance.
    // Propagate a failure reason to |derivedPromise|.
    static void updateDerivedFromReason(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Value>, v8::Isolate*);

    // |derivedPromise| and |promise| must be a Promise instance.
    // |onFulfilled| and |onRejected| can be an empty value respectively.
    // Propagate |promise|'s state to |derivedPromise|.
    static void updateDerivedFromPromise(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Object> promise, v8::Isolate*);

    // Returns a Promise instance that will be fulfilled or rejected by
    // |thenable|'s result.
    static v8::Local<v8::Object> coerceThenable(v8::Handle<v8::Object> thenable, v8::Handle<v8::Function> then, v8::Isolate*);

    // |promise| must be a Promise instance.
    // Applies a transformation to an argument and use it to update derived
    // promies.
    static void callHandler(v8::Handle<v8::Object> promise, v8::Handle<v8::Function> handler, v8::Handle<v8::Value> argument, PromiseState originatorState, v8::Isolate*);
};

} // namespace WebCore

#endif // V8PromiseCustom_h
