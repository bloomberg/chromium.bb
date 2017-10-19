/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/V8NodeFilterCondition.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Node.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/Node.h"
#include "core/dom/NodeFilter.h"
#include "core/frame/UseCounter.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8PrivateProperty.h"

namespace blink {

V8NodeFilterCondition::V8NodeFilterCondition(v8::Local<v8::Value> filter,
                                             ScriptState* script_state)
    : script_state_(script_state), filter_(this) {
  // ..acceptNode(..) will only dispatch filter_ if filter_->IsObject().
  // We'll make sure filter_ is either usable by acceptNode or empty.
  // (See the fast/dom/node-filter-gc test for a case where 'empty' happens.)
  if (!filter.IsEmpty() && filter->IsObject())
    filter_.Set(script_state->GetIsolate(), filter.As<v8::Object>());
}

V8NodeFilterCondition::~V8NodeFilterCondition() {}

void V8NodeFilterCondition::TraceWrappers(
    const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(filter_.Cast<v8::Value>());
}

unsigned V8NodeFilterCondition::acceptNode(
    Node* node,
    ExceptionState& exception_state) const {
  v8::Isolate* isolate = script_state_->GetIsolate();
  DCHECK(!script_state_->GetContext().IsEmpty());
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Object> filter = filter_.NewLocal(isolate);

  if (filter.IsEmpty())
    return NodeFilter::kFilterAccept;

  v8::TryCatch exception_catcher(isolate);

  v8::Local<v8::Function> callback;
  v8::Local<v8::Value> receiver;
  if (filter->IsFunction()) {
    UseCounter::Count(CurrentExecutionContext(isolate),
                      WebFeature::kNodeFilterIsFunction);
    callback = v8::Local<v8::Function>::Cast(filter);
    receiver = v8::Undefined(isolate);
  } else {
    v8::Local<v8::Value> value;
    if (!filter
             ->Get(script_state_->GetContext(),
                   V8AtomicString(isolate, "acceptNode"))
             .ToLocal(&value) ||
        !value->IsFunction()) {
      exception_state.ThrowTypeError(
          "NodeFilter object does not have an acceptNode function");
      return NodeFilter::kFilterReject;
    }
    UseCounter::Count(CurrentExecutionContext(isolate),
                      WebFeature::kNodeFilterIsObject);
    callback = v8::Local<v8::Function>::Cast(value);
    receiver = filter;
  }

  v8::Local<v8::Value> node_wrapper = ToV8(node, script_state_.get());
  if (node_wrapper.IsEmpty()) {
    if (exception_catcher.HasCaught())
      exception_state.RethrowV8Exception(exception_catcher.Exception());
    return NodeFilter::kFilterReject;
  }

  v8::Local<v8::Value> result;
  v8::Local<v8::Value> args[] = {node_wrapper};
  if (!V8ScriptRunner::CallFunction(callback,
                                    ExecutionContext::From(script_state_.get()),
                                    receiver, 1, args, isolate)
           .ToLocal(&result)) {
    exception_state.RethrowV8Exception(exception_catcher.Exception());
    return NodeFilter::kFilterReject;
  }

  DCHECK(!result.IsEmpty());

  uint32_t uint32_value;
  if (!result->Uint32Value(script_state_->GetContext()).To(&uint32_value)) {
    exception_state.RethrowV8Exception(exception_catcher.Exception());
    return NodeFilter::kFilterReject;
  }
  return uint32_value;
}

}  // namespace blink
