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

#include "third_party/blink/renderer/bindings/core/v8/scheduled_action.h"

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

ScheduledAction* ScheduledAction::Create(ScriptState* script_state,
                                         ExecutionContext* target,
                                         V8Function* handler,
                                         const Vector<ScriptValue>& arguments) {
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
  DCHECK(arguments_.IsEmpty());
  DCHECK(!function_);
  DCHECK(!script_state_);
}

void ScheduledAction::Dispose() {
  code_ = String();
  arguments_.clear();
  function_.Clear();
  script_state_->Reset();
  script_state_.Clear();
}

void ScheduledAction::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(function_);
}

void ScheduledAction::Execute(ExecutionContext* context) {
  if (!script_state_->ContextIsValid()) {
    DVLOG(1) << "ScheduledAction::execute " << this << ": context is empty";
    return;
  }
  // ExecutionContext::CanExecuteScripts() relies on the current context to
  // determine if it is allowed. Enter the scope here.
  ScriptState::Scope scope(script_state_->Get());

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
                                 V8Function* function,
                                 const Vector<ScriptValue>& arguments)
    : ScheduledAction(script_state) {
  function_ = function;
  arguments_ = arguments;
}

ScheduledAction::ScheduledAction(ScriptState* script_state, const String& code)
    : ScheduledAction(script_state) {
  code_ = code;
}

ScheduledAction::ScheduledAction(ScriptState* script_state)
    : script_state_(ScriptStateProtectingContext::Create(script_state)) {}

// https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#timers
void ScheduledAction::Execute(LocalFrame* frame) {
  DCHECK(script_state_->ContextIsValid());

  TRACE_EVENT0("v8", "ScheduledAction::execute");
  if (function_) {
    DVLOG(1) << "ScheduledAction::execute " << this << ": have function";

    // The method context is set in
    // https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-settimeout
    // as "the object on which the method for which the algorithm is running is
    // implemented (a Window or WorkerGlobalScope object)."
    ScriptWrappable* method_context =
        ToScriptWrappable(script_state_->GetContext()->Global());

    // 7.2. Run the appropriate set of steps from the following list:
    //
    //      If the first method argument is a Function:
    //
    //        Invoke the Function. Use the third and subsequent method
    //        arguments (if any) as the arguments for invoking the Function.
    //        Use method context proxy as the callback this value.
    //
    // InvokeAndReportException() is responsible for converting the method
    // context to the method context proxy, if needed.
    function_->InvokeAndReportException(method_context, arguments_);
  } else {
    DVLOG(1) << "ScheduledAction::execute " << this
             << ": executing from source";
    frame->GetScriptController().ExecuteScriptAndReturnValue(
        script_state_->GetContext(),
        ScriptSourceCode(code_,
                         ScriptSourceLocationType::kEvalForScheduledAction),
        KURL(), kNotSharableCrossOrigin);
  }

  // The frame might be invalid at this point because JavaScript could have
  // released it.
}

// https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#timers
void ScheduledAction::Execute(WorkerGlobalScope* worker) {
  DCHECK(worker->GetThread()->IsCurrentThread());

  if (!script_state_->ContextIsValid()) {
    DVLOG(1) << "ScheduledAction::execute " << this << ": context is empty";
    return;
  }

  if (function_) {
    // The method context is set in
    // https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-settimeout
    // as "the object on which the method for which the algorithm is running is
    // implemented (a Window or WorkerGlobalScope object)."
    ScriptWrappable* method_context =
        ToScriptWrappable(script_state_->GetContext()->Global());

    // 7.2. Run the appropriate set of steps from the following list:
    //
    //      If the first method argument is a Function:
    //
    //         Invoke the Function. Use the third and subsequent method
    //         arguments (if any) as the arguments for invoking the Function.
    //         Use method context proxy as the callback this value.
    //
    // InvokeAndReportException() is responsible for converting the method
    // context to the method context proxy, if needed.
    function_->InvokeAndReportException(method_context, arguments_);
  } else {
    worker->ScriptController()->Evaluate(
        ScriptSourceCode(code_,
                         ScriptSourceLocationType::kEvalForScheduledAction),
        kNotSharableCrossOrigin);
  }
}

}  // namespace blink
