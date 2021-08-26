// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_wasm_response_extensions.h"

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_response.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

// Wasm only has a single metadata type, but we need to tag it.
static const int kWasmModuleTag = 1;

// The |FetchDataLoader| for streaming compilation of WebAssembly code. The
// received bytes get forwarded to the V8 API class |WasmStreaming|.
class FetchDataLoaderForWasmStreaming final : public FetchDataLoader,
                                              public BytesConsumer::Client {

 public:
  FetchDataLoaderForWasmStreaming(std::shared_ptr<v8::WasmStreaming> streaming,
                                  ScriptState* script_state)
      : streaming_(std::move(streaming)), script_state_(script_state) {}

  v8::WasmStreaming* streaming() const { return streaming_.get(); }

  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!consumer_);
    DCHECK(!client_);
    client_ = client;
    consumer_ = consumer;
    consumer_->SetClient(this);
    OnStateChange();
  }

  void OnStateChange() override {
    while (true) {
      // |buffer| is owned by |consumer_|.
      const char* buffer = nullptr;
      size_t available = 0;
      BytesConsumer::Result result = consumer_->BeginRead(&buffer, &available);

      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk) {
        if (available > 0) {
          DCHECK_NE(buffer, nullptr);
          streaming_->OnBytesReceived(reinterpret_cast<const uint8_t*>(buffer),
                                      available);
        }
        result = consumer_->EndRead(available);
      }
      switch (result) {
        case BytesConsumer::Result::kShouldWait:
          NOTREACHED();
          return;
        case BytesConsumer::Result::kOk: {
          break;
        }
        case BytesConsumer::Result::kDone: {
          {
            ScriptState::Scope scope(script_state_);
            streaming_->Finish();
          }
          client_->DidFetchDataLoadedCustomFormat();
          return;
        }
        case BytesConsumer::Result::kError: {
          return AbortCompilation();
        }
      }
    }
  }

  String DebugName() const override { return "FetchDataLoaderForWasmModule"; }

  void Cancel() override {
    consumer_->Cancel();
    return AbortCompilation();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    visitor->Trace(script_state_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

  void AbortFromClient() {
    auto* exception =
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError);
    ScriptState::Scope scope(script_state_);

    // Calling ToV8 in a ScriptForbiddenScope will trigger a CHECK and
    // cause a crash. ToV8 just invokes a constructor for wrapper creation,
    // which is safe (no author script can be run). Adding AllowUserAgentScript
    // directly inside createWrapper could cause a perf impact (calling
    // isMainThread() every time a wrapper is created is expensive). Ideally,
    // resolveOrReject shouldn't be called inside a ScriptForbiddenScope.
    {
      ScriptForbiddenScope::AllowUserAgentScript allow_script;
      v8::Local<v8::Value> v8_exception =
          ToV8(exception, script_state_->GetContext()->Global(),
               script_state_->GetIsolate());
      streaming_->Abort(v8_exception);
    }
  }

 private:
  // TODO(ahaas): replace with spec-ed error types, once spec clarifies
  // what they are.
  void AbortCompilation() {
    if (script_state_->ContextIsValid()) {
      ScriptState::Scope scope(script_state_);
      streaming_->Abort(V8ThrowException::CreateTypeError(
          script_state_->GetIsolate(), "Could not download wasm module"));
    } else {
      // We are not allowed to execute a script, which indicates that we should
      // not reject the promise of the streaming compilation. By passing no
      // abort reason, we indicate the V8 side that the promise should not get
      // rejected.
      streaming_->Abort(v8::Local<v8::Value>());
    }
  }

  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;
  std::shared_ptr<v8::WasmStreaming> streaming_;
  const Member<ScriptState> script_state_;
};

// TODO(mtrofin): WasmDataLoaderClient is necessary so we may provide an
// argument to BodyStreamBuffer::startLoading, however, it fulfills
// a very small role. Consider refactoring to avoid it.
class WasmDataLoaderClient final
    : public GarbageCollected<WasmDataLoaderClient>,
      public FetchDataLoader::Client {

 public:
  explicit WasmDataLoaderClient(FetchDataLoaderForWasmStreaming* loader)
      : loader_(loader) {}
  void DidFetchDataLoadedCustomFormat() override {}
  void DidFetchDataLoadFailed() override { NOTREACHED(); }
  void Abort() override { loader_->AbortFromClient(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(loader_);
    FetchDataLoader::Client::Trace(visitor);
  }

 private:
  Member<FetchDataLoaderForWasmStreaming> loader_;
  DISALLOW_COPY_AND_ASSIGN(WasmDataLoaderClient);
};

// ExceptionToAbortStreamingScope converts a possible exception to an abort
// message for WasmStreaming instead of throwing the exception.
//
// All exceptions which happen in the setup of WebAssembly streaming compilation
// have to be passed as an abort message to V8 so that V8 can reject the promise
// associated to the streaming compilation.
class ExceptionToAbortStreamingScope {
  STACK_ALLOCATED();

 public:
  ExceptionToAbortStreamingScope(std::shared_ptr<v8::WasmStreaming> streaming,
                                 ExceptionState& exception_state)
      : streaming_(streaming), exception_state_(exception_state) {}

  ~ExceptionToAbortStreamingScope() {
    if (!exception_state_.HadException())
      return;

    streaming_->Abort(exception_state_.GetException());
    exception_state_.ClearException();
  }

 private:
  std::shared_ptr<v8::WasmStreaming> streaming_;
  ExceptionState& exception_state_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionToAbortStreamingScope);
};

void SendCachedData(String response_url,
                    base::Time response_time,
                    ExecutionContext* execution_context,
                    Vector<uint8_t> serialized_module) {
  if (!execution_context)
    return;
  scoped_refptr<CachedMetadata> cached_metadata =
      CachedMetadata::CreateFromSerializedData(std::move(serialized_module));

  blink::mojom::CodeCacheHost* code_cache_host =
      ExecutionContext::GetCodeCacheHostFromContext(execution_context);
  base::span<const uint8_t> serialized_data = cached_metadata->SerializedData();
  CachedMetadataSender::SendToCodeCacheHost(
      code_cache_host, mojom::blink::CodeCacheType::kWebAssembly, response_url,
      response_time, serialized_data.data(), serialized_data.size());
}

class WasmStreamingClient : public v8::WasmStreaming::Client {
 public:
  WasmStreamingClient(const String& response_url,
                      const base::Time& response_time,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                      ExecutionContext* execution_context)
      : response_url_(response_url.IsolatedCopy()),
        response_time_(response_time),
        main_thread_task_runner_(std::move(task_runner)),
        execution_context_(execution_context) {}

  void OnModuleCompiled(v8::CompiledWasmModule compiled_module) override {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "v8.wasm.compiledModule", TRACE_EVENT_SCOPE_THREAD,
                         "url", response_url_.Utf8());
    v8::MemorySpan<const uint8_t> wire_bytes =
        compiled_module.GetWireBytesRef();
    // Our heuristic for whether it's worthwhile to cache is that the module
    // was fully compiled and the size is such that loading from the cache will
    // improve startup time. Use wire bytes size since it should be correlated
    // with module size.
    // TODO(bbudge) This is set very low to compare performance of caching with
    // baseline compilation. Adjust this test once we know which sizes benefit.
    const size_t kWireBytesSizeThresholdBytes = 1UL << 10;  // 1 KB.
    if (wire_bytes.size() < kWireBytesSizeThresholdBytes)
      return;

    v8::OwnedBuffer serialized_module = compiled_module.Serialize();
    // V8 might not be able to serialize the module.
    if (serialized_module.size == 0)
      return;

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "v8.wasm.cachedModule", TRACE_EVENT_SCOPE_THREAD,
                         "producedCacheSize", serialized_module.size);

    // The resources needed for caching may have been GC'ed, but we should still
    // save the compiled module. Use the platform API directly.
    Vector<uint8_t> serialized_data = CachedMetadata::GetSerializedData(
        kWasmModuleTag,
        reinterpret_cast<const uint8_t*>(serialized_module.buffer.get()),
        serialized_module.size);

    // Make sure the data could be copied.
    if (serialized_data.size() < serialized_module.size)
      return;

    // TODO(mythria): Add support for worklets and remove this code that uses
    // per-process interface.
    if (!main_thread_task_runner_.get()) {
      SendCachedData(response_url_, response_time_, execution_context_.Lock(),
                     std::move(serialized_data));
      return;
    }

    main_thread_task_runner_->PostTask(
        FROM_HERE, ConvertToBaseOnceCallback(WTF::CrossThreadBindOnce(
                       &SendCachedData, response_url_, response_time_,
                       execution_context_, std::move(serialized_data))));
  }

  void SetBuffer(scoped_refptr<CachedMetadata> cached_module) {
    cached_module_ = cached_module;
  }

 private:
  String response_url_;
  base::Time response_time_;
  scoped_refptr<CachedMetadata> cached_module_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  CrossThreadWeakPersistent<ExecutionContext> execution_context_;

  DISALLOW_COPY_AND_ASSIGN(WasmStreamingClient);
};

scoped_refptr<base::SingleThreadTaskRunner> GetContextTaskRunner(
    ExecutionContext& execution_context) {
  if (execution_context.IsWorkerGlobalScope()) {
    WorkerOrWorkletGlobalScope& global_scope =
        To<WorkerOrWorkletGlobalScope>(execution_context);
    return global_scope.GetThread()
        ->GetWorkerBackingThread()
        .BackingThread()
        .GetTaskRunner();
  }

  if (execution_context.IsWindow()) {
    return Thread::MainThread()->GetTaskRunner();
  }

  DCHECK(execution_context.IsWorkletGlobalScope());
  return nullptr;
}

void StreamFromResponseCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "v8.wasm.streamFromResponseCallback",
                       TRACE_EVENT_SCOPE_THREAD);
  ExceptionState exception_state(args.GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "WebAssembly", "compile");
  std::shared_ptr<v8::WasmStreaming> streaming =
      v8::WasmStreaming::Unpack(args.GetIsolate(), args.Data());
  ExceptionToAbortStreamingScope exception_scope(streaming, exception_state);

  ScriptState* script_state = ScriptState::ForCurrentRealm(args);
  if (!script_state->ContextIsValid()) {
    // We do not have an execution context, we just abort streaming compilation
    // immediately without error.
    streaming->Abort(v8::Local<v8::Value>());
    return;
  }

  Response* response =
      V8Response::ToImplWithTypeCheck(args.GetIsolate(), args[0]);
  if (!response) {
    exception_state.ThrowTypeError(
        "An argument must be provided, which must be a "
        "Response or Promise<Response> object");
    return;
  }

  if (!response->ok()) {
    exception_state.ThrowTypeError("HTTP status code is not ok");
    return;
  }

  // The spec explicitly disallows any extras on the Content-Type header,
  // so we check against ContentType() rather than MimeType(), which
  // implicitly strips extras.
  if (response->ContentType().LowerASCII() != "application/wasm") {
    exception_state.ThrowTypeError(
        "Incorrect response MIME type. Expected 'application/wasm'.");
    return;
  }

  if (response->IsBodyLocked() || response->IsBodyUsed()) {
    exception_state.ThrowTypeError(
        "Cannot compile WebAssembly.Module from an already read Response");
    return;
  }

  if (!response->BodyBuffer()) {
    // Since the status is 2xx (ok), this must be status 204 (No Content),
    // status 205 (Reset Content) or a malformed status 200 (OK).
    exception_state.ThrowWasmCompileError("Empty WebAssembly module");
    return;
  }

  String url = response->url();
  const std::string& url_utf8 = url.Utf8();
  streaming->SetUrl(url_utf8.c_str(), url_utf8.size());
  if (auto* cache_handler =
          response->BodyBuffer()->GetCachedMetadataHandler()) {
    auto* execution_context = ExecutionContext::From(script_state);
    DCHECK_NE(execution_context, nullptr);

    auto client = std::make_shared<WasmStreamingClient>(
        url, response->GetResponse()->InternalResponse()->ResponseTime(),
        GetContextTaskRunner(*execution_context), execution_context);
    streaming->SetClient(client);
    scoped_refptr<CachedMetadata> cached_module =
        cache_handler->GetCachedMetadata(kWasmModuleTag);
    if (cached_module) {
      TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                           "v8.wasm.moduleCacheHit", TRACE_EVENT_SCOPE_THREAD,
                           "url", url.Utf8(), "consumedCacheSize",
                           cached_module->size());
      bool is_valid = streaming->SetCompiledModuleBytes(
          reinterpret_cast<const uint8_t*>(cached_module->Data()),
          cached_module->size());
      if (is_valid) {
        // Keep the buffer alive until V8 is ready to deserialize it.
        // TODO(bbudge) V8 should notify us if deserialization fails, so we
        // can release the data and reset the cache.
        client->SetBuffer(cached_module);
      } else {
        TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                             "v8.wasm.moduleCacheInvalid",
                             TRACE_EVENT_SCOPE_THREAD);
        // TODO(mythria): Also support using context specific code cache host
        // here. When we pass nullptr for CodeCacheHost we use per-process
        // interface. Currently this code is run on a thread started via a
        // Platform::PostJob. So it isn't safe to use CodeCacheHost interface
        // that was bound on the frame / worker threads. We should instead post
        // a task back to the frame / worker threads with the required data
        // which can then write to generated code caches.
        cache_handler->ClearCachedMetadata(
            /*code_cache_host*/ nullptr,
            CachedMetadataHandler::kClearPersistentStorage);
      }
    }
  }

  FetchDataLoaderForWasmStreaming* loader =
      MakeGarbageCollected<FetchDataLoaderForWasmStreaming>(streaming,
                                                            script_state);
  response->BodyBuffer()->StartLoading(
      loader, MakeGarbageCollected<WasmDataLoaderClient>(loader),
      exception_state);
}

}  // namespace

void WasmResponseExtensions::Initialize(v8::Isolate* isolate) {
  isolate->SetWasmStreamingCallback(StreamFromResponseCallback);
}

}  // namespace blink
