/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/ScheduledAction.h"

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

ScheduledAction* ScheduledAction::Create(ScriptState* script_state,
                                         ExecutionContext* target,
                                         const ScriptValue& handler,
                                         const Vector<ScriptValue>& arguments) {
  DCHECK(handler.IsFunction());
  if (!script_state->World().IsWorkerWorld()) {
    if (!BindingSecurity::ShouldAllowAccessToFrame(
            EnteredDOMWindow(script_state->GetIsolate()),
            ToDocument(target)->GetFrame(),
            BindingSecurity::ErrorReportOption::kDoNotReport)) {
      UseCounter::Count(target, WebFeature::kScheduledActionIgnored);
      return new ScheduledAction(script_state);
    }
  }
  return new ScheduledAction(script_state, handler, arguments);
}

ScheduledAction* ScheduledAction::Create(ScriptState* script_state,
                                         ExecutionContext* target,
                                         const String& handler) {
  if (!script_state->World().IsWorkerWorld()) {
    if (!BindingSecurity::ShouldAllowAccessToFrame(
            EnteredDOMWindow(script_state->GetIsolate()),
            ToDocument(target)->GetFrame(),
            BindingSecurity::ErrorReportOption::kDoNotReport)) {
      UseCounter::Count(target, WebFeature::kScheduledActionIgnored);
      return new ScheduledAction(script_state);
    }
  }
  return new ScheduledAction(script_state, handler);
}

ScheduledAction::~ScheduledAction() {
  // Verify that owning DOMTimer has eagerly disposed.
  DCHECK(info_.IsEmpty());
}

void ScheduledAction::Dispose() {
  code_ = String();
  info_.Clear();
  function_.Clear();
  script_state_.Clear();
}

void ScheduledAction::Execute(ExecutionContext* context) {
  if (context->IsDocument()) {
    LocalFrame* frame = ToDocument(context)->GetFrame();
    if (!frame) {
      DVLOG(1) << "ScheduledAction::execute " << this << ": no frame";
      return;
    }
    if (!context->CanExecuteScripts(kAboutToExecuteScript)) {
      DVLOG(1) << "ScheduledAction::execute " << this
               << ": frame can not execute scripts";
      return;
    }
    Execute(frame);
  } else {
    DVLOG(1) << "ScheduledAction::execute " << this << ": worker scope";
    Execute(ToWorkerGlobalScope(context));
  }
}

ScheduledAction::ScheduledAction(ScriptState* script_state,
                                 const ScriptValue& function,
                                 const Vector<ScriptValue>& arguments)
    : script_state_(script_state), info_(script_state->GetIsolate()) {
  DCHECK(function.IsFunction());
  function_.Set(script_state->GetIsolate(),
                v8::Local<v8::Function>::Cast(function.V8Value()));
  info_.ReserveCapacity(arguments.size());
  for (const ScriptValue& argument : arguments)
    info_.Append(argument.V8Value());
}

ScheduledAction::ScheduledAction(ScriptState* script_state, const String& code)
    : script_state_(script_state),
      info_(script_state->GetIsolate()),
      code_(code) {}

ScheduledAction::ScheduledAction(ScriptState* script_state)
    : script_state_(script_state), info_(script_state->GetIsolate()) {}

void ScheduledAction::Execute(LocalFrame* frame) {
  if (!script_state_->ContextIsValid()) {
    DVLOG(1) << "ScheduledAction::execute " << this << ": context is empty";
    return;
  }

  TRACE_EVENT0("v8", "ScheduledAction::execute");
  ScriptState::Scope scope(script_state_.Get());
  if (!function_.IsEmpty()) {
    DVLOG(1) << "ScheduledAction::execute " << this << ": have function";
    v8::Local<v8::Function> function =
        function_.NewLocal(script_state_->GetIsolate());
    ScriptState* script_state_for_func =
        ScriptState::From(function->CreationContext());
    if (!script_state_for_func->ContextIsValid()) {
      DVLOG(1) << "ScheduledAction::execute " << this
               << ": function's context is empty";
      return;
    }
    Vector<v8::Local<v8::Value>> info;
    CreateLocalHandlesForArgs(&info);
    V8ScriptRunner::CallFunction(
        function, frame->GetDocument(), script_state_->GetContext()->Global(),
        info.size(), info.data(), script_state_->GetIsolate());
  } else {
    DVLOG(1) << "ScheduledAction::execute " << this
             << ": executing from source";
    frame->GetScriptController().ExecuteScriptAndReturnValue(
        script_state_->GetContext(), ScriptSourceCode(code_));
  }

  // The frame might be invalid at this point because JavaScript could have
  // released it.
}

void ScheduledAction::Execute(WorkerGlobalScope* worker) {
  DCHECK(worker->GetThread()->IsCurrentThread());

  if (!script_state_->ContextIsValid()) {
    DVLOG(1) << "ScheduledAction::execute " << this << ": context is empty";
    return;
  }

  if (!function_.IsEmpty()) {
    ScriptState::Scope scope(script_state_.Get());
    v8::Local<v8::Function> function =
        function_.NewLocal(script_state_->GetIsolate());
    ScriptState* script_state_for_func =
        ScriptState::From(function->CreationContext());
    if (!script_state_for_func->ContextIsValid()) {
      DVLOG(1) << "ScheduledAction::execute " << this
               << ": function's context is empty";
      return;
    }
    Vector<v8::Local<v8::Value>> info;
    CreateLocalHandlesForArgs(&info);
    V8ScriptRunner::CallFunction(
        function, worker, script_state_->GetContext()->Global(), info.size(),
        info.data(), script_state_->GetIsolate());
  } else {
    worker->ScriptController()->Evaluate(ScriptSourceCode(code_));
  }
}

void ScheduledAction::CreateLocalHandlesForArgs(
    Vector<v8::Local<v8::Value>>* handles) {
  handles->ReserveCapacity(info_.Size());
  for (size_t i = 0; i < info_.Size(); ++i)
    handles->push_back(info_.Get(i));
}

}  // namespace blink
