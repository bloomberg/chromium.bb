/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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

#include "bindings/core/v8/WorkerOrWorkletScriptController.h"

#include <memory>

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8ErrorHandler.h"
#include "bindings/core/v8/V8Initializer.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/ErrorEvent.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/WorkerThreadDebugger.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/bindings/ConditionalFeatures.h"
#include "platform/bindings/V8DOMWrapper.h"
#include "platform/bindings/V8ObjectConstructor.h"
#include "platform/bindings/WrapperTypeInfo.h"
#include "platform/heap/ThreadState.h"
#include "public/platform/Platform.h"
#include "v8/include/v8.h"

namespace blink {

class WorkerOrWorkletScriptController::ExecutionState final {
  STACK_ALLOCATED();

 public:
  explicit ExecutionState(WorkerOrWorkletScriptController* controller)
      : had_exception(false),
        controller_(controller),
        outer_state_(controller->execution_state_) {
    controller_->execution_state_ = this;
  }

  ~ExecutionState() { controller_->execution_state_ = outer_state_; }

  bool had_exception;
  String error_message;
  std::unique_ptr<SourceLocation> location_;
  ScriptValue exception;
  Member<ErrorEvent> error_event_from_imported_script_;

  // A ExecutionState context is stack allocated by
  // WorkerOrWorkletScriptController::evaluate(), with the contoller using it
  // during script evaluation. To handle nested evaluate() uses,
  // ExecutionStates are chained together;
  // |m_outerState| keeps a pointer to the context object one level out
  // (or 0, if outermost.) Upon return from evaluate(), the
  // WorkerOrWorkletScriptController's ExecutionState is popped and the
  // previous one restored (see above dtor.)
  //
  // With Oilpan, |m_outerState| isn't traced. It'll be "up the stack"
  // and its fields will be traced when scanning the stack.
  Member<WorkerOrWorkletScriptController> controller_;
  ExecutionState* outer_state_;
};

WorkerOrWorkletScriptController* WorkerOrWorkletScriptController::Create(
    WorkerOrWorkletGlobalScope* global_scope,
    v8::Isolate* isolate) {
  return new WorkerOrWorkletScriptController(global_scope, isolate);
}

WorkerOrWorkletScriptController::WorkerOrWorkletScriptController(
    WorkerOrWorkletGlobalScope* global_scope,
    v8::Isolate* isolate)
    : global_scope_(global_scope),
      isolate_(isolate),
      execution_forbidden_(false),
      rejected_promises_(RejectedPromises::Create()),
      execution_state_(0) {
  DCHECK(isolate);
  world_ =
      DOMWrapperWorld::Create(isolate, DOMWrapperWorld::WorldType::kWorker);
}

WorkerOrWorkletScriptController::~WorkerOrWorkletScriptController() {
  DCHECK(!rejected_promises_);
}

void WorkerOrWorkletScriptController::Dispose() {
  rejected_promises_->Dispose();
  rejected_promises_ = nullptr;

  world_->Dispose();

  DisposeContextIfNeeded();
}

void WorkerOrWorkletScriptController::DisposeContextIfNeeded() {
  if (!IsContextInitialized())
    return;

  if (global_scope_->IsWorkerGlobalScope() ||
      global_scope_->IsThreadedWorkletGlobalScope()) {
    ScriptState::Scope scope(script_state_.Get());
    WorkerThreadDebugger* debugger = WorkerThreadDebugger::From(isolate_);
    debugger->ContextWillBeDestroyed(global_scope_->GetThread(),
                                     script_state_->GetContext());
  }
  script_state_->DisposePerContextData();
}

bool WorkerOrWorkletScriptController::InitializeContextIfNeeded(
    const String& human_readable_name) {
  v8::HandleScope handle_scope(isolate_);

  if (IsContextInitialized())
    return true;

  // Create a new v8::Context with the worker/worklet as the global object
  // (aka the inner global).
  ScriptWrappable* script_wrappable = global_scope_->GetScriptWrappable();
  const WrapperTypeInfo* wrapper_type_info =
      script_wrappable->GetWrapperTypeInfo();
  v8::Local<v8::FunctionTemplate> global_interface_template =
      wrapper_type_info->domTemplate(isolate_, *world_);
  if (global_interface_template.IsEmpty())
    return false;
  v8::Local<v8::ObjectTemplate> global_template =
      global_interface_template->InstanceTemplate();
  v8::Local<v8::Context> context;
  {
    // Initialize V8 extensions before creating the context.
    Vector<const char*> extension_names;
    if (global_scope_->IsServiceWorkerGlobalScope() &&
        Platform::Current()->AllowScriptExtensionForServiceWorker(
            ToWorkerGlobalScope(global_scope_.Get())->Url())) {
      const V8Extensions& extensions = ScriptController::RegisteredExtensions();
      extension_names.ReserveInitialCapacity(extensions.size());
      for (const auto* extension : extensions)
        extension_names.push_back(extension->name());
    }
    v8::ExtensionConfiguration extension_configuration(extension_names.size(),
                                                       extension_names.data());

    V8PerIsolateData::UseCounterDisabledScope use_counter_disabled(
        V8PerIsolateData::From(isolate_));
    context =
        v8::Context::New(isolate_, &extension_configuration, global_template);
  }
  if (context.IsEmpty())
    return false;

  script_state_ = ScriptState::Create(context, world_);

  ScriptState::Scope scope(script_state_.Get());

  // Associate the global proxy object, the global object and the worker
  // instance (C++ object) as follows.
  //
  //   global proxy object <====> worker or worklet instance
  //                               ^
  //                               |
  //   global object       --------+
  //
  // Per HTML spec, there is no corresponding object for workers to WindowProxy.
  // However, V8 always creates the global proxy object, we associate these
  // objects in the same manner as WindowProxy and Window.
  //
  // a) worker or worklet instance --> global proxy object
  // As we shouldn't expose the global object to author scripts, we map the
  // worker or worklet instance to the global proxy object.
  // b) global proxy object --> worker or worklet instance
  // Blink's callback functions are called by V8 with the global proxy object,
  // we need to map the global proxy object to the worker or worklet instance.
  // c) global object --> worker or worklet instance
  // The global proxy object is NOT considered as a wrapper object of the
  // worker or worklet instance because it's not an instance of
  // v8::FunctionTemplate of worker or worklet, especially note that
  // v8::Object::FindInstanceInPrototypeChain skips the global proxy object.
  // Thus we need to map the global object to the worker or worklet instance.

  // The global proxy object.  Note this is not the global object.
  v8::Local<v8::Object> global_proxy = context->Global();
  v8::Local<v8::Object> associated_wrapper =
      V8DOMWrapper::AssociateObjectWithWrapper(isolate_, script_wrappable,
                                               wrapper_type_info, global_proxy);
  CHECK(global_proxy == associated_wrapper);

  // The global object, aka worker/worklet wrapper object.
  v8::Local<v8::Object> global_object =
      global_proxy->GetPrototype().As<v8::Object>();
  V8DOMWrapper::SetNativeInfo(isolate_, global_object, wrapper_type_info,
                              script_wrappable);

  // All interfaces must be registered to V8PerContextData.
  // So we explicitly call constructorForType for the global object.
  V8PerContextData::From(context)->ConstructorForType(wrapper_type_info);

  // Name new context for debugging. For main thread worklet global scopes
  // this is done once the context is initialized.
  if (global_scope_->IsWorkerGlobalScope() ||
      global_scope_->IsThreadedWorkletGlobalScope()) {
    WorkerThreadDebugger* debugger = WorkerThreadDebugger::From(isolate_);
    debugger->ContextCreated(global_scope_->GetThread(), context);
  }

  // Set the human readable name for the world if the call passes an actual
  // |human_readable name|.
  if (!human_readable_name.IsEmpty()) {
    world_->SetNonMainWorldHumanReadableName(world_->GetWorldId(),
                                             human_readable_name);
  }

  InstallConditionalFeaturesOnGlobal(wrapper_type_info, script_state_.Get());

  return true;
}

ScriptValue WorkerOrWorkletScriptController::Evaluate(
    const String& script,
    const String& file_name,
    const TextPosition& script_start_position,
    CachedMetadataHandler* cache_handler,
    V8CacheOptions v8_cache_options) {
  TRACE_EVENT1("devtools.timeline", "EvaluateScript", "data",
               InspectorEvaluateScriptEvent::Data(nullptr, file_name,
                                                  script_start_position));
  if (!InitializeContextIfNeeded(String()))
    return ScriptValue();

  ScriptState::Scope scope(script_state_.Get());

  if (!disable_eval_pending_.IsEmpty()) {
    script_state_->GetContext()->AllowCodeGenerationFromStrings(false);
    script_state_->GetContext()->SetErrorMessageForCodeGenerationFromStrings(
        V8String(isolate_, disable_eval_pending_));
    disable_eval_pending_ = String();
  }

  v8::TryCatch block(isolate_);

  v8::Local<v8::Script> compiled_script;
  v8::MaybeLocal<v8::Value> maybe_result;
  if (V8ScriptRunner::CompileScript(script_state_.Get(), script, file_name,
                                    String(), script_start_position,
                                    cache_handler, kSharableCrossOrigin,
                                    v8_cache_options)
          .ToLocal(&compiled_script))
    maybe_result = V8ScriptRunner::RunCompiledScript(isolate_, compiled_script,
                                                     global_scope_);

  if (!block.CanContinue()) {
    ForbidExecution();
    return ScriptValue();
  }

  if (block.HasCaught()) {
    v8::Local<v8::Message> message = block.Message();
    execution_state_->had_exception = true;
    execution_state_->error_message = ToCoreString(message->Get());
    execution_state_->location_ = SourceLocation::FromMessage(
        isolate_, message, ExecutionContext::From(script_state_.Get()));
    execution_state_->exception =
        ScriptValue(script_state_.Get(), block.Exception());
    block.Reset();
  } else {
    execution_state_->had_exception = false;
  }

  v8::Local<v8::Value> result;
  if (!maybe_result.ToLocal(&result) || result->IsUndefined())
    return ScriptValue();

  return ScriptValue(script_state_.Get(), result);
}

bool WorkerOrWorkletScriptController::Evaluate(
    const ScriptSourceCode& source_code,
    ErrorEvent** error_event,
    CachedMetadataHandler* cache_handler,
    V8CacheOptions v8_cache_options) {
  if (IsExecutionForbidden())
    return false;

  ExecutionState state(this);
  Evaluate(source_code.Source(), source_code.Url().GetString(),
           source_code.StartPosition(), cache_handler, v8_cache_options);
  if (IsExecutionForbidden())
    return false;

  ScriptState::Scope scope(script_state_.Get());
  if (state.had_exception) {
    if (error_event) {
      if (state.error_event_from_imported_script_) {
        // Propagate inner error event outwards.
        *error_event = state.error_event_from_imported_script_.Release();
        return false;
      }
      if (global_scope_->ShouldSanitizeScriptError(state.location_->Url(),
                                                   kNotSharableCrossOrigin)) {
        *error_event = ErrorEvent::CreateSanitizedError(world_.Get());
      } else {
        *error_event =
            ErrorEvent::Create(state.error_message, state.location_->Clone(),
                               state.exception, world_.Get());
      }
      V8ErrorHandler::StoreExceptionOnErrorEventWrapper(
          script_state_.Get(), *error_event, state.exception.V8Value(),
          script_state_->GetContext()->Global());
    } else {
      DCHECK(!global_scope_->ShouldSanitizeScriptError(
          state.location_->Url(), kNotSharableCrossOrigin));
      ErrorEvent* event = nullptr;
      if (state.error_event_from_imported_script_) {
        event = state.error_event_from_imported_script_.Release();
      } else {
        event =
            ErrorEvent::Create(state.error_message, state.location_->Clone(),
                               state.exception, world_.Get());
      }
      global_scope_->DispatchErrorEvent(event, kNotSharableCrossOrigin);
    }
    return false;
  }
  return true;
}

ScriptValue WorkerOrWorkletScriptController::EvaluateAndReturnValueForTest(
    const ScriptSourceCode& source_code) {
  ExecutionState state(this);
  return Evaluate(source_code.Source(), source_code.Url().GetString(),
                  source_code.StartPosition(), nullptr, kV8CacheOptionsDefault);
}

void WorkerOrWorkletScriptController::ForbidExecution() {
  DCHECK(global_scope_->IsContextThread());
  execution_forbidden_ = true;
}

bool WorkerOrWorkletScriptController::IsExecutionForbidden() const {
  DCHECK(global_scope_->IsContextThread());
  return execution_forbidden_;
}

void WorkerOrWorkletScriptController::DisableEval(const String& error_message) {
  disable_eval_pending_ = error_message;
}

void WorkerOrWorkletScriptController::RethrowExceptionFromImportedScript(
    ErrorEvent* error_event,
    ExceptionState& exception_state) {
  const String& error_message = error_event->message();
  if (execution_state_)
    execution_state_->error_event_from_imported_script_ = error_event;
  exception_state.RethrowV8Exception(
      V8ThrowException::CreateError(isolate_, error_message));
}

DEFINE_TRACE(WorkerOrWorkletScriptController) {
  visitor->Trace(global_scope_);
}

}  // namespace blink
