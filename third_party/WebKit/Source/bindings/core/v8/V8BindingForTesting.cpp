// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8BindingForTesting.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Settings.h"
#include "core/testing/DummyPageHolder.h"

namespace blink {

RefPtr<ScriptStateForTesting> ScriptStateForTesting::Create(
    v8::Local<v8::Context> context,
    RefPtr<DOMWrapperWorld> world) {
  RefPtr<ScriptStateForTesting> script_state =
      WTF::AdoptRef(new ScriptStateForTesting(context, std::move(world)));
  // This ref() is for keeping this ScriptState alive as long as the v8::Context
  // is alive.  This is deref()ed in the weak callback of the v8::Context.
  script_state->AddRef();
  return script_state;
}

ScriptStateForTesting::ScriptStateForTesting(v8::Local<v8::Context> context,
                                             RefPtr<DOMWrapperWorld> world)
    : ScriptState(context, std::move(world)) {}

V8TestingScope::V8TestingScope()
    : holder_(DummyPageHolder::Create()),
      handle_scope_(GetIsolate()),
      context_(GetScriptState()->GetContext()),
      context_scope_(GetContext()),
      try_catch_(GetIsolate()) {
  GetFrame().GetSettings()->SetScriptEnabled(true);
}

ScriptState* V8TestingScope::GetScriptState() const {
  return ToScriptStateForMainWorld(holder_->GetDocument().GetFrame());
}

ExecutionContext* V8TestingScope::GetExecutionContext() const {
  return ExecutionContext::From(GetScriptState());
}

v8::Isolate* V8TestingScope::GetIsolate() const {
  return GetScriptState()->GetIsolate();
}

v8::Local<v8::Context> V8TestingScope::GetContext() const {
  return context_;
}

ExceptionState& V8TestingScope::GetExceptionState() {
  return exception_state_;
}

Page& V8TestingScope::GetPage() {
  return holder_->GetPage();
}

LocalFrame& V8TestingScope::GetFrame() {
  return holder_->GetFrame();
}

Document& V8TestingScope::GetDocument() {
  return holder_->GetDocument();
}

V8TestingScope::~V8TestingScope() {
  // TODO(yukishiino): We put this statement here to clear an exception from
  // the isolate.  Otherwise, the leak detector complains.  Really mysterious
  // hack.
  v8::Function::New(GetContext(), nullptr);
}

}  // namespace blink
