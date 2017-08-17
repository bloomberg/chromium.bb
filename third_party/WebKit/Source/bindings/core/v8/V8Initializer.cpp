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

#include "bindings/core/v8/V8Initializer.h"

#include <memory>

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/RejectedPromises.h"
#include "bindings/core/v8/RetainedDOMInfo.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/UseCounterCallback.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8ContextSnapshot.h"
#include "bindings/core/v8/V8DOMException.h"
#include "bindings/core/v8/V8ErrorEvent.h"
#include "bindings/core/v8/V8ErrorHandler.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8IdleTaskRunner.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/ScriptWrappableVisitor.h"
#include "platform/bindings/V8PerContextData.h"
#include "platform/bindings/V8PrivateProperty.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/weborigin/SecurityViolationReportingPolicy.h"
#include "platform/wtf/AddressSanitizer.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/text/WTFString.h"
#include "platform/wtf/typed_arrays/ArrayBufferContents.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "v8/include/v8-debug.h"
#include "v8/include/v8-profiler.h"

namespace blink {

static void ReportFatalErrorInMainThread(const char* location,
                                         const char* message) {
  int memory_usage_mb = Platform::Current()->ActualMemoryUsageMB();
  DVLOG(1) << "V8 error: " << message << " (" << location
           << ").  Current memory usage: " << memory_usage_mb << " MB";
  LOG(FATAL);
}

static void ReportOOMErrorInMainThread(const char* location, bool is_js_heap) {
  int memory_usage_mb = Platform::Current()->ActualMemoryUsageMB();
  DVLOG(1) << "V8 " << (is_js_heap ? "javascript" : "process") << " OOM: ("
           << location << ").  Current memory usage: " << memory_usage_mb
           << " MB";
  OOM_CRASH();
}

static String ExtractMessageForConsole(v8::Isolate* isolate,
                                       v8::Local<v8::Value> data) {
  if (V8DOMWrapper::IsWrapper(isolate, data)) {
    v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(data);
    const WrapperTypeInfo* type = ToWrapperTypeInfo(obj);
    if (V8DOMException::wrapperTypeInfo.IsSubclass(type)) {
      DOMException* exception = V8DOMException::toImpl(obj);
      if (exception && !exception->MessageForConsole().IsEmpty())
        return exception->ToStringForConsole();
    }
  }
  return g_empty_string;
}

namespace {
MessageLevel MessageLevelFromNonFatalErrorLevel(int error_level) {
  MessageLevel level = kErrorMessageLevel;
  switch (error_level) {
    case v8::Isolate::kMessageDebug:
      level = kVerboseMessageLevel;
      break;
    case v8::Isolate::kMessageLog:
    case v8::Isolate::kMessageInfo:
      level = kInfoMessageLevel;
      break;
    case v8::Isolate::kMessageWarning:
      level = kWarningMessageLevel;
      break;
    case v8::Isolate::kMessageError:
      level = kInfoMessageLevel;
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

  if (isolate->GetEnteredContext().IsEmpty())
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
        kJSMessageSource,
        MessageLevelFromNonFatalErrorLevel(message->ErrorLevel()),
        ToCoreStringWithNullCheck(message->Get()), std::move(location)));
    return;
  }

  AccessControlStatus access_control_status = kNotSharableCrossOrigin;
  if (message->IsOpaque())
    access_control_status = kOpaqueResource;
  else if (message->IsSharedCrossOrigin())
    access_control_status = kSharableCrossOrigin;

  ErrorEvent* event =
      ErrorEvent::Create(ToCoreStringWithNullCheck(message->Get()),
                         std::move(location), &script_state->World());

  String message_for_console = ExtractMessageForConsole(isolate, data);
  if (!message_for_console.IsEmpty())
    event->SetUnsanitizedMessage("Uncaught " + message_for_console);

  V8ErrorHandler::StoreExceptionOnErrorEventWrapper(
      script_state, event, data, script_state->GetContext()->Global());
  context->DispatchErrorEvent(event, access_control_status);
}

namespace {

static RejectedPromises& RejectedPromisesOnMainThread() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(RefPtr<RejectedPromises>, rejected_promises,
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
  }

  DCHECK_EQ(data.GetEvent(), v8::kPromiseRejectWithNoHandler);

  v8::Local<v8::Promise> promise = data.GetPromise();
  v8::Isolate* isolate = promise->GetIsolate();
  ExecutionContext* context = ExecutionContext::From(script_state);

  v8::Local<v8::Value> exception = data.GetValue();
  if (V8DOMWrapper::IsWrapper(isolate, exception)) {
    // Try to get the stack & location from a wrapped exception object (e.g.
    // DOMException).
    DCHECK(exception->IsObject());
    auto private_error = V8PrivateProperty::GetDOMExceptionError(isolate);
    v8::Local<v8::Value> error =
        private_error.GetOrUndefined(exception.As<v8::Object>());
    if (!error->IsUndefined())
      exception = error;
  }

  String error_message;
  AccessControlStatus cors_status = kNotSharableCrossOrigin;
  std::unique_ptr<SourceLocation> location;

  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(isolate, exception);
  if (!message.IsEmpty()) {
    // message->Get() can be empty here. https://crbug.com/450330
    error_message = ToCoreStringWithNullCheck(message->Get());
    location = SourceLocation::FromMessage(isolate, message, context);
    if (message->IsSharedCrossOrigin())
      cors_status = kSharableCrossOrigin;
  } else {
    location =
        SourceLocation::Create(context->Url().GetString(), 0, 0, nullptr);
  }

  String message_for_console =
      ExtractMessageForConsole(isolate, data.GetValue());
  if (!message_for_console.IsEmpty())
    error_message = "Uncaught " + message_for_console;

  rejected_promises.RejectedWithNoHandler(script_state, data, error_message,
                                          std::move(location), cors_status);
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

  DCHECK(execution_context->IsWorkerGlobalScope());
  WorkerOrWorkletScriptController* script_controller =
      ToWorkerGlobalScope(execution_context)->ScriptController();
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
    if (ContentSecurityPolicy* policy =
            ToDocument(execution_context)->GetContentSecurityPolicy()) {
      v8::String::Value source_str(source);
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

  v8::Local<v8::WasmCompiledModule> module =
      v8::Local<v8::WasmCompiledModule>::Cast(source);
  if (static_cast<size_t>(module->GetWasmWireBytes()->Length()) >
      kWasmWireBytesLimit) {
    ThrowRangeException(
        args.GetIsolate(),
        "WebAssembly.Instance is disallowed on the main thread, "
        "if the buffer size is larger than 4KB. Use "
        "WebAssembly.instantiate.");
    return true;
  }
  return false;
}

static void InitializeV8Common(v8::Isolate* isolate) {
  isolate->AddGCPrologueCallback(V8GCController::GcPrologue);
  isolate->AddGCEpilogueCallback(V8GCController::GcEpilogue);
  std::unique_ptr<ScriptWrappableVisitor> visitor(
      new ScriptWrappableVisitor(isolate));
  V8PerIsolateData::From(isolate)->SetScriptWrappableVisitor(
      std::move(visitor));
  isolate->SetEmbedderHeapTracer(
      V8PerIsolateData::From(isolate)->GetScriptWrappableVisitor());

  isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kScoped);

  isolate->SetUseCounterCallback(&UseCounterCallback);
  isolate->SetWasmModuleCallback(WasmModuleOverride);
  isolate->SetWasmInstanceCallback(WasmInstanceOverride);

  V8ContextSnapshot::EnsureInterfaceTemplates(isolate);
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

  void* Reserve(size_t length) override {
    return WTF::ArrayBufferContents::ReserveMemory(length);
  }

  void Free(void* data, size_t length, AllocationMode mode) override {
    switch (mode) {
      case AllocationMode::kNormal:
        Free(data, length);
        return;
      case AllocationMode::kReservation:
        WTF::ArrayBufferContents::ReleaseReservedMemory(data, length);
        return;
      default:
        NOTREACHED();
    }
  }

  void SetProtection(void* data,
                     size_t length,
                     Protection protection) override {
    switch (protection) {
      case Protection::kNoAccess:
        WTF::SetSystemPagesInaccessible(data, length);
        return;
      case Protection::kReadWrite:
        (void)WTF::SetSystemPagesAccessible(data, length);
        return;
      default:
        NOTREACHED();
    }
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

void V8Initializer::InitializeMainThread(intptr_t* reference_table) {
  DCHECK(IsMainThread());

  WTF::ArrayBufferContents::Initialize(AdjustAmountOfExternalAllocatedMemory);

  DEFINE_STATIC_LOCAL(ArrayBufferAllocator, array_buffer_allocator, ());
  auto v8_extras_mode = RuntimeEnabledFeatures::ExperimentalV8ExtrasEnabled()
                            ? gin::IsolateHolder::kStableAndExperimentalV8Extras
                            : gin::IsolateHolder::kStableV8Extras;
  gin::IsolateHolder::Initialize(gin::IsolateHolder::kNonStrictMode,
                                 v8_extras_mode, &array_buffer_allocator);

  // NOTE: Some threads (namely utility threads) don't have a scheduler.
  WebScheduler* scheduler = Platform::Current()->CurrentThread()->Scheduler();

  V8PerIsolateData::V8ContextSnapshotMode v8_context_snapshot_mode =
      Platform::Current()->IsTakingV8ContextSnapshot()
          ? V8PerIsolateData::V8ContextSnapshotMode::kTakeSnapshot
          : V8PerIsolateData::V8ContextSnapshotMode::kUseSnapshot;
  if (v8_context_snapshot_mode ==
          V8PerIsolateData::V8ContextSnapshotMode::kUseSnapshot &&
      !RuntimeEnabledFeatures::V8ContextSnapshotEnabled()) {
    v8_context_snapshot_mode =
        V8PerIsolateData::V8ContextSnapshotMode::kDontUseSnapshot;
    reference_table = nullptr;
  }
  V8ContextSnapshot::SetReferenceTable(reference_table);

  // When timer task runner is used for PerIsolateData, GC tasks are getting
  // throttled and memory usage goes up. For now we're using loading task queue
  // to prevent this.
  // TODO(altimin): Consider switching to timerTaskRunner here.
  v8::Isolate* isolate = V8PerIsolateData::Initialize(
      scheduler ? scheduler->LoadingTaskRunner()
                : Platform::Current()->CurrentThread()->GetWebTaskRunner(),
      reference_table, v8_context_snapshot_mode);

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
  if (RuntimeEnabledFeatures::V8IdleTasksEnabled()) {
    V8PerIsolateData::EnableIdleTasks(
        isolate, WTF::MakeUnique<V8IdleTaskRunner>(scheduler));
  }

  isolate->SetPromiseRejectCallback(PromiseRejectHandlerInMainThread);

  if (v8::HeapProfiler* profiler = isolate->GetHeapProfiler()) {
    profiler->SetWrapperClassInfoProvider(
        WrapperTypeInfo::kNodeClassId, &RetainedDOMInfo::CreateRetainedDOMInfo);
    profiler->SetGetRetainerInfosCallback(&V8GCController::GetRetainerInfos);
  }

  DCHECK(ThreadState::MainThreadState());
  ThreadState::MainThreadState()->RegisterTraceDOMWrappers(
      isolate, V8GCController::TraceDOMWrappers,
      ScriptWrappableVisitor::InvalidateDeadObjectsInMarkingDeque,
      ScriptWrappableVisitor::PerformCleanup);

  V8PerIsolateData::From(isolate)->SetThreadDebugger(
      WTF::MakeUnique<MainThreadDebugger>(isolate));

  BindingSecurity::InitWrapperCreationSecurityCheck();
}

static void ReportFatalErrorInWorker(const char* location,
                                     const char* message) {
  // FIXME: We temporarily deal with V8 internal error situations such as
  // out-of-memory by crashing the worker.
  LOG(FATAL);
}

static void MessageHandlerInWorker(v8::Local<v8::Message> message,
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
        kJSMessageSource,
        MessageLevelFromNonFatalErrorLevel(message->ErrorLevel()),
        ToCoreStringWithNullCheck(message->Get()), std::move(location)));
    return;
  }

  ErrorEvent* event =
      ErrorEvent::Create(ToCoreStringWithNullCheck(message->Get()),
                         std::move(location), &script_state->World());

  AccessControlStatus cors_status = message->IsSharedCrossOrigin()
                                        ? kSharableCrossOrigin
                                        : kNotSharableCrossOrigin;

  // If execution termination has been triggered as part of constructing
  // the error event from the v8::Message, quietly leave.
  if (!isolate->IsExecutionTerminating()) {
    V8ErrorHandler::StoreExceptionOnErrorEventWrapper(
        script_state, event, data, script_state->GetContext()->Global());
    ExecutionContext::From(script_state)
        ->DispatchErrorEvent(event, cors_status);
  }

  per_isolate_data->SetReportingException(false);
}

// Stack size for workers is limited to 500KB because default stack size for
// secondary threads is 512KB on Mac OS X. See GetDefaultThreadStackSize() in
// base/threading/platform_thread_mac.mm for details.
static const int kWorkerMaxStackSize = 500 * 1024;

// This function uses a local stack variable to determine the isolate's stack
// limit. AddressSanitizer may relocate that local variable to a fake stack,
// which may lead to problems during JavaScript execution.  Therefore we disable
// AddressSanitizer for V8Initializer::initializeWorker().
NO_SANITIZE_ADDRESS
void V8Initializer::InitializeWorker(v8::Isolate* isolate) {
  InitializeV8Common(isolate);

  isolate->AddMessageListenerWithErrorLevel(
      MessageHandlerInWorker,
      v8::Isolate::kMessageError | v8::Isolate::kMessageWarning |
          v8::Isolate::kMessageInfo | v8::Isolate::kMessageDebug |
          v8::Isolate::kMessageLog);
  isolate->SetFatalErrorHandler(ReportFatalErrorInWorker);

  uint32_t here;
  isolate->SetStackLimit(reinterpret_cast<uintptr_t>(&here) -
                         kWorkerMaxStackSize);
  isolate->SetPromiseRejectCallback(PromiseRejectHandlerInWorker);
}

}  // namespace blink
