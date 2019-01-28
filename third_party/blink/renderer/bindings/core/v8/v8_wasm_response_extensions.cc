// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_wasm_response_extensions.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_response.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

// Wasm only has a single metadata type, but we need to tag it.
static const int kWasmModuleTag = 1;

// The |FetchDataLoader| for streaming compilation of WebAssembly code. The
// received bytes get forwarded to the V8 API class |WasmStreaming|.
class FetchDataLoaderForWasmStreaming final : public FetchDataLoader,
                                              public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderForWasmStreaming);

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

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    visitor->Trace(script_state_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
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
    : public GarbageCollectedFinalized<WasmDataLoaderClient>,
      public FetchDataLoader::Client {
  WTF_MAKE_NONCOPYABLE(WasmDataLoaderClient);
  USING_GARBAGE_COLLECTED_MIXIN(WasmDataLoaderClient);

 public:
  WasmDataLoaderClient() = default;
  void DidFetchDataLoadedCustomFormat() override {}
  void DidFetchDataLoadFailed() override { NOTREACHED(); }
  void Abort() override {
    // TODO(ricea): This should probably cause the promise owned by
    // v8::WasmModuleObjectBuilderStreaming to reject with an AbortError
    // DOMException. As it is, the cancellation will cause it to reject with a
    // TypeError later.
  }
};

// ExceptionToAbortStreamingScope converts a possible exception to an abort
// message for WasmStreaming instead of throwing the exception.
//
// All exceptions which happen in the setup of WebAssembly streaming compilation
// have to be passed as an abort message to V8 so that V8 can reject the promise
// associated to the streaming compilation.
class ExceptionToAbortStreamingScope {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ExceptionToAbortStreamingScope);

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
};

SingleCachedMetadataHandler* GetCachedMetadataHandler(ScriptState* script_state,
                                                      const KURL& url) {
  if (!RuntimeEnabledFeatures::WasmCodeCacheEnabled())
    return nullptr;
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!execution_context)
    return nullptr;
  ResourceFetcher* fetcher = execution_context->Fetcher();
  if (!fetcher)
    return nullptr;
  if (!url.IsValid())
    return nullptr;
  Resource* resource = fetcher->CachedResource(url);
  if (!resource)
    return nullptr;

  // Wasm modules should be fetched as raw resources.
  DCHECK_EQ(ResourceType::kRaw, resource->GetType());
  RawResource* raw_resource = ToRawResource(resource);
  return raw_resource->ScriptCacheHandler();
}

class WasmStreamingClient : public v8::WasmStreaming::Client {
 public:
  WasmStreamingClient(const KURL& url,
                      v8::Isolate* isolate,
                      v8::Local<v8::Context> context)
      : url_(url), isolate_(isolate), context_(isolate, context) {
    context_.SetWeak();
  }

  void OnModuleCompiled(v8::CompiledWasmModule compiled_module) override {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "v8.wasm.compiledModule", TRACE_EVENT_SCOPE_THREAD,
                         "url", url_.GetString().Utf8());

    // Don't cache if Context has been destroyed.
    if (context_.IsEmpty())
      return;

    v8::HandleScope handle_scope(isolate_);
    auto context = context_.Get(isolate_);
    ScriptState* script_state = ScriptState::From(context);
    SingleCachedMetadataHandler* cache_handler =
        GetCachedMetadataHandler(script_state, url_);
    if (!cache_handler)
      return;

    v8::MemorySpan<const uint8_t> wire_bytes =
        compiled_module.GetWireBytesRef();
    // Our heuristic for whether it's worthwhile to cache is that the module
    // was fully compiled and it is "large". Wire bytes size is likely to be
    // highly correlated with compiled module size so we use it to avoid the
    // cost of serializing when not caching.
    const size_t kWireBytesSizeThresholdBytes = 1UL << 17;  // 128 KB.
    if (wire_bytes.size() < kWireBytesSizeThresholdBytes)
      return;

    v8::OwnedBuffer serialized_module = compiled_module.Serialize();
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "v8.wasm.cachedModule", TRACE_EVENT_SCOPE_THREAD,
                         "producedCacheSize", serialized_module.size);
    cache_handler->SetCachedMetadata(
        kWasmModuleTag,
        reinterpret_cast<const uint8_t*>(serialized_module.buffer.get()),
        serialized_module.size);
  }

 private:
  KURL url_;
  v8::Isolate* isolate_;
  v8::Global<v8::Context> context_;

  DISALLOW_COPY_AND_ASSIGN(WasmStreamingClient);
};

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

  if (response->MimeType() != "application/wasm") {
    exception_state.ThrowTypeError(
        "Incorrect response MIME type. Expected 'application/wasm'.");
    return;
  }

  Body::BodyLocked body_locked = response->IsBodyLocked(exception_state);
  if (body_locked == Body::BodyLocked::kBroken)
    return;

  if (body_locked == Body::BodyLocked::kLocked ||
      response->IsBodyUsed(exception_state) == Body::BodyUsed::kUsed) {
    DCHECK(!exception_state.HadException());
    exception_state.ThrowTypeError(
        "Cannot compile WebAssembly.Module from an already read Response");
    return;
  }

  if (exception_state.HadException())
    return;

  if (!response->BodyBuffer()) {
    exception_state.ThrowTypeError("Response object has a null body.");
    return;
  }

  KURL url(response->url());
  SingleCachedMetadataHandler* cache_handler =
      GetCachedMetadataHandler(script_state, url);
  if (cache_handler) {
    streaming->SetClient(std::make_shared<WasmStreamingClient>(
        url, args.GetIsolate(), script_state->GetContext()));
    scoped_refptr<CachedMetadata> cached_module =
        cache_handler->GetCachedMetadata(kWasmModuleTag);
    if (cached_module) {
      TRACE_EVENT_INSTANT2(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                           "v8.wasm.moduleCacheHit", TRACE_EVENT_SCOPE_THREAD,
                           "url", url.GetString().Utf8(), "consumedCacheSize",
                           cached_module->size());
      bool is_valid = streaming->SetCompiledModuleBytes(
          reinterpret_cast<const uint8_t*>(cached_module->Data()),
          cached_module->size());
      if (!is_valid) {
        TRACE_EVENT_INSTANT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                             "v8.wasm.moduleCacheInvalid",
                             TRACE_EVENT_SCOPE_THREAD);
        cache_handler->ClearCachedMetadata(
            CachedMetadataHandler::kSendToPlatform);
      }
    }
  }

  FetchDataLoaderForWasmStreaming* loader =
      MakeGarbageCollected<FetchDataLoaderForWasmStreaming>(streaming,
                                                            script_state);
  response->BodyBuffer()->StartLoading(
      loader, MakeGarbageCollected<WasmDataLoaderClient>(), exception_state);
}

}  // namespace

void WasmResponseExtensions::Initialize(v8::Isolate* isolate) {
  isolate->SetWasmStreamingCallback(StreamFromResponseCallback);
}

}  // namespace blink
