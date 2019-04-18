/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/v8_initializer.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/rejected_promises.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/use_counter_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_context_snapshot.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_embedder_graph_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_error_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_task_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_wasm_response_extensions.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_violation_reporting_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/blink/renderer/platform/wtf/stack_util.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/typed_arrays/array_buffer_contents.h"
#include "v8/include/v8-profiler.h"
#include "v8/include/v8.h"

namespace blink {

static void ReportFatalErrorInMainThread(const char* location,
                                         const char* message) {
  DVLOG(1) << "V8 error: " << message << " (" << location << ").";
  LOG(FATAL);
}

static void ReportOOMErrorInMainThread(const char* location, bool is_js_heap) {
  DVLOG(1) << "V8 " << (is_js_heap ? "javascript" : "process") << " OOM: ("
           << location << ").";
  OOM_CRASH();
}

static String ExtractMessageForConsole(v8::Isolate* isolate,
                                       v8::Local<v8::Value> data) {
  if (V8DOMWrapper::IsWrapper(isolate, data)) {
    v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(data);
    const WrapperTypeInfo* type = ToWrapperTypeInfo(obj);
    if (V8DOMException::GetWrapperTypeInfo()->IsSubclass(type)) {
      DOMException* exception = V8DOMException::ToImpl(obj);
      if (exception && !exception->MessageForConsole().IsEmpty())
        return exception->ToStringForConsole();
    }
  }
  return g_empty_string;
}

namespace {
mojom::ConsoleMessageLevel MessageLevelFromNonFatalErrorLevel(int error_level) {
  mojom::ConsoleMessageLevel level = mojom::ConsoleMessageLevel::kError;
  switch (error_level) {
    case v8::Isolate::kMessageDebug:
      level = mojom::ConsoleMessageLevel::kVerbose;
      break;
    case v8::Isolate::kMessageLog:
    case v8::Isolate::kMessageInfo:
      level = mojom::ConsoleMessageLevel::kInfo;
      break;
    case v8::Isolate::kMessageWarning:
      level = mojom::ConsoleMessageLevel::kWarning;
      break;
    case v8::Isolate::kMessageError:
      level = mojom::ConsoleMessageLevel::kInfo;
      break;
    default:
      NOTREACHED();
  }
  return level;
}

// NOTE: when editing this, please also edit the error messages we throw when
// the size is exceeded (see uses of the constant), which use the human-friendly
// "4KB" text.
const size_t kWasmWireBytesLimit = 1 << 12;

}  // namespace

void V8Initializer::MessageHandlerInMainThread(v8::Local<v8::Message> message,
                                               v8::Local<v8::Value> data) {
  DCHECK(IsMainThread());
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  if (isolate->GetEnteredOrMicrotaskContext().IsEmpty())
    return;

  // If called during context initialization, there will be no entered context.
  ScriptState* script_state = ScriptState::Current(isolate);
  if (!script_state->ContextIsValid())
    return;

  ExecutionContext* context = ExecutionContext::From(script_state);
  std::unique_ptr<SourceLocation> location =
      SourceLocation::FromMessage(isolate, message, context);

  if (message->ErrorLevel() != v8::Isolate::kMessageError) {
    context->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kJavaScript,
        MessageLevelFromNonFatalErrorLevel(message->ErrorLevel()),
        ToCoreStringWithNullCheck(message->Get()), std::move(location)));
    return;
  }

  const auto sanitize_script_errors = message->IsSharedCrossOrigin()
                                          ? SanitizeScriptErrors::kDoNotSanitize
                                          : SanitizeScriptErrors::kSanitize;

  ErrorEvent* event = ErrorEvent::Create(
      ToCoreStringWithNullCheck(message->Get()), std::move(location),
      ScriptValue::From(script_state, data), &script_state->World());

  String message_for_console = ExtractMessageForConsole(isolate, data);
  if (!message_for_console.IsEmpty())
    event->SetUnsanitizedMessage("Uncaught " + message_for_console);

  context->DispatchErrorEvent(event, sanitize_script_errors);
}

void V8Initializer::MessageHandlerInWorker(v8::Local<v8::Message> message,
                                           v8::Local<v8::Value> data) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  V8PerIsolateData* per_isolate_data = V8PerIsolateData::From(isolate);

  // During the frame teardown, there may not be a valid context.
  ScriptState* script_state = ScriptState::Current(isolate);
  if (!script_state->ContextIsValid())
    return;

  // Exceptions that occur in error handler should be ignored since in that case
  // WorkerGlobalScope::dispatchErrorEvent will send the exception to the worker
  // object.
  if (per_isolate_data->IsReportingException())
    return;

  per_isolate_data->SetReportingException(true);

  ExecutionContext* context = ExecutionContext::From(script_state);
  std::unique_ptr<SourceLocation> location =
      SourceLocation::FromMessage(isolate, message, context);

  if (message->ErrorLevel() != v8::Isolate::kMessageError) {
    context->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kJavaScript,
        MessageLevelFromNonFatalErrorLevel(message->ErrorLevel()),
        ToCoreStringWithNullCheck(message->Get()), std::move(location)));
    return;
  }

  ErrorEvent* event = ErrorEvent::Create(
      ToCoreStringWithNullCheck(message->Get()), std::move(location),
      ScriptValue::From(script_state, data), &script_state->World());

  const auto sanitize_script_errors = message->IsSharedCrossOrigin()
                                          ? SanitizeScriptErrors::kDoNotSanitize
                                          : SanitizeScriptErrors::kSanitize;

  // If execution termination has been triggered as part of constructing
  // the error event from the v8::Message, quietly leave.
  if (!isolate->IsExecutionTerminating()) {
    ExecutionContext::From(script_state)
        ->DispatchErrorEvent(event, sanitize_script_errors);
  }

  per_isolate_data->SetReportingException(false);
}

namespace {

static RejectedPromises& RejectedPromisesOnMainThread() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(scoped_refptr<RejectedPromises>, rejected_promises,
                      (RejectedPromises::Create()));
  return *rejected_promises;
}

}  // namespace

void V8Initializer::ReportRejectedPromisesOnMainThread() {
  RejectedPromisesOnMainThread().ProcessQueue();
}

static void PromiseRejectHandler(v8::PromiseRejectMessage data,
                                 RejectedPromises& rejected_promises,
                                 ScriptState* script_state) {
  if (data.GetEvent() == v8::kPromiseHandlerAddedAfterReject) {
    rejected_promises.HandlerAdded(data);
    return;
  } else if (data.GetEvent() == v8::kPromiseRejectAfterResolved ||
             data.GetEvent() == v8::kPromiseResolveAfterResolved) {
    // Ignore reject/resolve after resolved.
    return;
  }

  DCHECK_EQ(v8::kPromiseRejectWithNoHandler, data.GetEvent());

  v8::Isolate* isolate = script_state->GetIsolate();
  ExecutionContext* context = ExecutionContext::From(script_state);

  v8::Local<v8::Value> exception = data.GetValue();
  if (V8DOMWrapper::IsWrapper(isolate, exception)) {
    // Try to get the stack & location from a wrapped exception object (e.g.
    // DOMException).
    DCHECK(exception->IsObject());
    auto private_error = V8PrivateProperty::GetDOMExceptionError(isolate);
    v8::Local<v8::Value> error;
    if (private_error.GetOrUndefined(exception.As<v8::Object>())
            .ToLocal(&error) &&
        !error->IsUndefined()) {
      exception = error;
    }
  }

  String error_message;
  SanitizeScriptErrors sanitize_script_errors = SanitizeScriptErrors::kSanitize;
  std::unique_ptr<SourceLocation> location;

  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(isolate, exception);
  if (!message.IsEmpty()) {
    // message->Get() can be empty here. https://crbug.com/450330
    error_message = ToCoreStringWithNullCheck(message->Get());
    location = SourceLocation::FromMessage(isolate, message, context);
    if (message->IsSharedCrossOrigin())
      sanitize_script_errors = SanitizeScriptErrors::kDoNotSanitize;
  } else {
    location = std::make_unique<SourceLocation>(context->Url().GetString(), 0,
                                                0, nullptr);
  }

  String message_for_console =
      ExtractMessageForConsole(isolate, data.GetValue());
  if (!message_for_console.IsEmpty())
    error_message = "Uncaught " + message_for_console;

  rejected_promises.RejectedWithNoHandler(script_state, data, error_message,
                                          std::move(location),
                                          sanitize_script_errors);
}

static void PromiseRejectHandlerInMainThread(v8::PromiseRejectMessage data) {
  DCHECK(IsMainThread());

  v8::Local<v8::Promise> promise = data.GetPromise();

  v8::Isolate* isolate = promise->GetIsolate();

  // TODO(ikilpatrick): Remove this check, extensions tests that use
  // extensions::ModuleSystemTest incorrectly don't have a valid script state.
  LocalDOMWindow* window = CurrentDOMWindow(isolate);
  if (!window || !window->IsCurrentlyDisplayedInFrame())
    return;

  // Bail out if called during context initialization.
  ScriptState* script_state = ScriptState::Current(isolate);
  if (!script_state->ContextIsValid())
    return;

  PromiseRejectHandler(data, RejectedPromisesOnMainThread(), script_state);
}

static void PromiseRejectHandlerInWorker(v8::PromiseRejectMessage data) {
  v8::Local<v8::Promise> promise = data.GetPromise();

  // Bail out if called during context initialization.
  v8::Isolate* isolate = promise->GetIsolate();
  ScriptState* script_state = ScriptState::Current(isolate);
  if (!script_state->ContextIsValid())
    return;

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!execution_context)
    return;

  auto* script_controller =
      To<WorkerGlobalScope>(execution_context)->ScriptController();
  DCHECK(script_controller);

  PromiseRejectHandler(data, *script_controller->GetRejectedPromises(),
                       script_state);
}

static void FailedAccessCheckCallbackInMainThread(v8::Local<v8::Object> holder,
                                                  v8::AccessType type,
                                                  v8::Local<v8::Value> data) {
  // FIXME: We should modify V8 to pass in more contextual information (context,
  // property, and object).
  BindingSecurity::FailedAccessCheckFor(v8::Isolate::GetCurrent(),
                                        WrapperTypeInfo::Unwrap(data), holder);
}

static bool CodeGenerationCheckCallbackInMainThread(
    v8::Local<v8::Context> context,
    v8::Local<v8::String> source) {
  if (ExecutionContext* execution_context = ToExecutionContext(context)) {
    DCHECK(execution_context->IsDocument() ||
           execution_context->IsMainThreadWorkletGlobalScope());
    if (ContentSecurityPolicy* policy =
            execution_context->GetContentSecurityPolicy()) {
      v8::String::Value source_str(context->GetIsolate(), source);
      UChar snippet[ContentSecurityPolicy::kMaxSampleLength + 1];
      size_t len = std::min((sizeof(snippet) / sizeof(UChar)) - 1,
                            static_cast<size_t>(source_str.length()));
      memcpy(snippet, *source_str, len * sizeof(UChar));
      snippet[len] = 0;
      return policy->AllowEval(
          ScriptState::From(context), SecurityViolationReportingPolicy::kReport,
          ContentSecurityPolicy::kWillThrowException, snippet);
    }
  }
  return false;
}

static bool WasmCodeGenerationCheckCallbackInMainThread(
    v8::Local<v8::Context> context,
    v8::Local<v8::String> source) {
  if (ExecutionContext* execution_context = ToExecutionContext(context)) {
    if (ContentSecurityPolicy* policy =
            To<Document>(execution_context)->GetContentSecurityPolicy()) {
      v8::String::Value source_str(context->GetIsolate(), source);
      UChar snippet[ContentSecurityPolicy::kMaxSampleLength + 1];
      size_t len = std::min((sizeof(snippet) / sizeof(UChar)) - 1,
                            static_cast<size_t>(source_str.length()));
      memcpy(snippet, *source_str, len * sizeof(UChar));
      snippet[len] = 0;
      // Wasm code generation is allowed if we have either the wasm-eval
      // directive or the unsafe-eval directive. However, we only recognize
      // wasm-eval for certain schemes
      return policy->AllowWasmEval(ScriptState::From(context),
                                   SecurityViolationReportingPolicy::kReport,
                                   ContentSecurityPolicy::kWillThrowException,
                                   snippet) ||
             policy->AllowEval(ScriptState::From(context),
                               SecurityViolationReportingPolicy::kReport,
                               ContentSecurityPolicy::kWillThrowException,
                               snippet);
    }
  }
  return false;
}

static bool WasmThreadsEnabledCallback(v8::Local<v8::Context> context) {
  ExecutionContext* execution_context = ToExecutionContext(context);
  if (!execution_context)
    return false;

  return RuntimeEnabledFeatures::WebAssemblyThreadsEnabled(execution_context);
}

v8::Local<v8::Value> NewRangeException(v8::Isolate* isolate,
                                       const char* message) {
  return v8::Exception::RangeError(
      v8::String::NewFromOneByte(isolate,
                                 reinterpret_cast<const uint8_t*>(message),
                                 v8::NewStringType::kNormal)
          .ToLocalChecked());
}

void ThrowRangeException(v8::Isolate* isolate, const char* message) {
  isolate->ThrowException(NewRangeException(isolate, message));
}

static bool WasmModuleOverride(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Return false if we want the base behavior to proceed.
  if (!WTF::IsMainThread() || args.Length() < 1)
    return false;
  v8::Local<v8::Value> source = args[0];
  if ((source->IsArrayBuffer() &&
       v8::Local<v8::ArrayBuffer>::Cast(source)->ByteLength() >
           kWasmWireBytesLimit) ||
      (source->IsArrayBufferView() &&
       v8::Local<v8::ArrayBufferView>::Cast(source)->ByteLength() >
           kWasmWireBytesLimit)) {
    ThrowRangeException(args.GetIsolate(),
                        "WebAssembly.Compile is disallowed on the main thread, "
                        "if the buffer size is larger than 4KB. Use "
                        "WebAssembly.compile, or compile on a worker thread.");
    // Return true because we injected new behavior and we do not
    // want the default behavior.
    return true;
  }
  return false;
}

static bool WasmInstanceOverride(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Return false if we want the base behavior to proceed.
  if (!WTF::IsMainThread() || args.Length() < 1)
    return false;
  v8::Local<v8::Value> source = args[0];
  if (!source->IsWebAssemblyCompiledModule())
    return false;

  v8::CompiledWasmModule compiled_module =
      v8::Local<v8::WasmModuleObject>::Cast(source)->GetCompiledModule();
  if (compiled_module.GetWireBytesRef().size() > kWasmWireBytesLimit) {
    ThrowRangeException(
        args.GetIsolate(),
        "WebAssembly.Instance is disallowed on the main thread, "
        "if the buffer size is larger than 4KB. Use "
        "WebAssembly.instantiate.");
    return true;
  }
  return false;
}

static v8::MaybeLocal<v8::Promise> HostImportModuleDynamically(
    v8::Local<v8::Context> context,
    v8::Local<v8::ScriptOrModule> v8_referrer,
    v8::Local<v8::String> v8_specifier) {
  ScriptState* script_state = ScriptState::From(context);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  Modulator* modulator = Modulator::From(script_state);
  if (!modulator) {
    resolver->Reject();
    return v8::Local<v8::Promise>::Cast(promise.V8Value());
  }

  String specifier = ToCoreStringWithNullCheck(v8_specifier);
  v8::Local<v8::Value> v8_referrer_resource_url =
      v8_referrer->GetResourceName();
  KURL referrer_resource_url;
  if (v8_referrer_resource_url->IsString()) {
    String referrer_resource_url_str =
        ToCoreString(v8::Local<v8::String>::Cast(v8_referrer_resource_url));
    if (!referrer_resource_url_str.IsEmpty())
      referrer_resource_url = KURL(NullURL(), referrer_resource_url_str);
  }
  ReferrerScriptInfo referrer_info =
      ReferrerScriptInfo::FromV8HostDefinedOptions(
          context, v8_referrer->GetHostDefinedOptions());
  modulator->ResolveDynamically(specifier, referrer_resource_url, referrer_info,
                                resolver);
  return v8::Local<v8::Promise>::Cast(promise.V8Value());
}

// https://html.spec.whatwg.org/C/#hostgetimportmetaproperties
static void HostGetImportMetaProperties(v8::Local<v8::Context> context,
                                        v8::Local<v8::Module> module,
                                        v8::Local<v8::Object> meta) {
  ScriptState* script_state = ScriptState::From(context);
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  Modulator* modulator = Modulator::From(script_state);
  if (!modulator)
    return;

  // TODO(shivanisha): Can a valid source url be passed to the constructor.
  ModuleImportMeta host_meta = modulator->HostGetImportMetaProperties(
      ModuleRecord(isolate, module, KURL()));

  // 3. Return <<Record { [[Key]]: "url", [[Value]]: urlString }>>. [spec text]
  v8::Local<v8::String> url_key = V8String(isolate, "url");
  v8::Local<v8::String> url_value = V8String(isolate, host_meta.Url());
  meta->CreateDataProperty(context, url_key, url_value).ToChecked();
}

static void InitializeV8Common(v8::Isolate* isolate) {
  isolate->AddGCPrologueCallback(V8GCController::GcPrologue);
  isolate->AddGCEpilogueCallback(V8GCController::GcEpilogue);
  isolate->SetEmbedderHeapTracer(static_cast<v8::EmbedderHeapTracer*>(
      V8PerIsolateData::From(isolate)->GetUnifiedHeapController()));
  isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kScoped);
  isolate->SetUseCounterCallback(&UseCounterCallback);
  isolate->SetWasmModuleCallback(WasmModuleOverride);
  isolate->SetWasmInstanceCallback(WasmInstanceOverride);
  isolate->SetWasmThreadsEnabledCallback(WasmThreadsEnabledCallback);
  isolate->SetHostImportModuleDynamicallyCallback(HostImportModuleDynamically);
  isolate->SetHostInitializeImportMetaObjectCallback(
      HostGetImportMetaProperties);

  V8ContextSnapshot::EnsureInterfaceTemplates(isolate);

  WasmResponseExtensions::Initialize(isolate);
}

namespace {

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
  // Allocate() methods return null to signal allocation failure to V8, which
  // should respond by throwing a RangeError, per
  // http://www.ecma-international.org/ecma-262/6.0/#sec-createbytedatablock.
  void* Allocate(size_t size) override {
    return WTF::ArrayBufferContents::AllocateMemoryOrNull(
        size, WTF::ArrayBufferContents::kZeroInitialize);
  }

  void* AllocateUninitialized(size_t size) override {
    return WTF::ArrayBufferContents::AllocateMemoryOrNull(
        size, WTF::ArrayBufferContents::kDontInitialize);
  }

  void Free(void* data, size_t size) override {
    WTF::ArrayBufferContents::FreeMemory(data);
  }
};

}  // namespace

static void AdjustAmountOfExternalAllocatedMemory(int64_t diff) {
#if DCHECK_IS_ON()
  static int64_t process_total = 0;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, ());
  {
    MutexLocker locker(mutex);

    process_total += diff;
    DCHECK_GE(process_total, 0)
        << "total amount = " << process_total << ", diff = " << diff;
  }
#endif

  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(diff);
}

void V8Initializer::InitializeMainThread(const intptr_t* reference_table) {
  DCHECK(IsMainThread());

  WTF::ArrayBufferContents::Initialize(AdjustAmountOfExternalAllocatedMemory);

  DEFINE_STATIC_LOCAL(ArrayBufferAllocator, array_buffer_allocator, ());
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kNonStrictMode,
                                 &array_buffer_allocator, reference_table);

  ThreadScheduler* scheduler = ThreadScheduler::Current();

#if defined(USE_V8_CONTEXT_SNAPSHOT)
  V8PerIsolateData::V8ContextSnapshotMode v8_context_snapshot_mode =
      Platform::Current()->IsTakingV8ContextSnapshot()
          ? V8PerIsolateData::V8ContextSnapshotMode::kTakeSnapshot
          : V8PerIsolateData::V8ContextSnapshotMode::kUseSnapshot;
#else
  V8PerIsolateData::V8ContextSnapshotMode v8_context_snapshot_mode =
      V8PerIsolateData::V8ContextSnapshotMode::kDontUseSnapshot;
#endif  // USE_V8_CONTEXT_SNAPSHOT

  v8::Isolate* isolate = V8PerIsolateData::Initialize(scheduler->V8TaskRunner(),
                                                      v8_context_snapshot_mode);
  scheduler->SetV8Isolate(isolate);

  // ThreadState::isolate_ needs to be set before setting the EmbedderHeapTracer
  // as setting the tracer indicates that a V8 garbage collection should trace
  // over to Blink.
  DCHECK(ThreadState::MainThreadState());

  ThreadState::MainThreadState()->RegisterTraceDOMWrappers(
      isolate, V8GCController::TraceDOMWrappers);

  InitializeV8Common(isolate);

  isolate->SetOOMErrorHandler(ReportOOMErrorInMainThread);

  isolate->SetFatalErrorHandler(ReportFatalErrorInMainThread);
  isolate->AddMessageListenerWithErrorLevel(
      MessageHandlerInMainThread,
      v8::Isolate::kMessageError | v8::Isolate::kMessageWarning |
          v8::Isolate::kMessageInfo | v8::Isolate::kMessageDebug |
          v8::Isolate::kMessageLog);
  isolate->SetFailedAccessCheckCallbackFunction(
      FailedAccessCheckCallbackInMainThread);
  isolate->SetAllowCodeGenerationFromStringsCallback(
      CodeGenerationCheckCallbackInMainThread);
  isolate->SetAllowWasmCodeGenerationCallback(
      WasmCodeGenerationCheckCallbackInMainThread);
  if (RuntimeEnabledFeatures::V8IdleTasksEnabled()) {
    V8PerIsolateData::EnableIdleTasks(
        isolate, std::make_unique<V8IdleTaskRunner>(scheduler));
  }

  isolate->SetPromiseRejectCallback(PromiseRejectHandlerInMainThread);

  if (v8::HeapProfiler* profiler = isolate->GetHeapProfiler()) {
    profiler->AddBuildEmbedderGraphCallback(
        &EmbedderGraphBuilder::BuildEmbedderGraphCallback, nullptr);
  }

  V8PerIsolateData::From(isolate)->SetThreadDebugger(
      std::make_unique<MainThreadDebugger>(isolate));
}

static void ReportFatalErrorInWorker(const char* location,
                                     const char* message) {
  // FIXME: We temporarily deal with V8 internal error situations such as
  // out-of-memory by crashing the worker.
  LOG(FATAL);
}

// Stack size for workers is limited to 500KB because default stack size for
// secondary threads is 512KB on Mac OS X. See GetDefaultThreadStackSize() in
// base/threading/platform_thread_mac.mm for details.
static const int kWorkerMaxStackSize = 500 * 1024;

void V8Initializer::InitializeWorker(v8::Isolate* isolate) {
  InitializeV8Common(isolate);

  isolate->AddMessageListenerWithErrorLevel(
      MessageHandlerInWorker,
      v8::Isolate::kMessageError | v8::Isolate::kMessageWarning |
          v8::Isolate::kMessageInfo | v8::Isolate::kMessageDebug |
          v8::Isolate::kMessageLog);
  isolate->SetFatalErrorHandler(ReportFatalErrorInWorker);

  isolate->SetStackLimit(WTF::GetCurrentStackPosition() - kWorkerMaxStackSize);
  isolate->SetPromiseRejectCallback(PromiseRejectHandlerInWorker);
}

}  // namespace blink
