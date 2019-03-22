// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_wasm_response_extensions.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_response.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/body_stream_buffer.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

namespace {

// The |FetchDataLoader| for streaming compilation of WebAssembly code. The
// received bytes get forwarded to the V8 API class |WasmStreaming|.
class FetchDataLoaderForWasmStreaming final : public FetchDataLoader,
                                              public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderForWasmStreaming);

 public:
  FetchDataLoaderForWasmStreaming(ScriptState* script_state,
                                  std::shared_ptr<v8::WasmStreaming> streaming)
      : streaming_(std::move(streaming)), script_state_(script_state) {}

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
          streaming_->Finish();
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
  TraceWrapperMember<BytesConsumer> consumer_;
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

void StreamFromResponseCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
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

  FetchDataLoaderForWasmStreaming* loader =
      new FetchDataLoaderForWasmStreaming(script_state, streaming);
  response->BodyBuffer()->StartLoading(loader, new WasmDataLoaderClient(),
                                       exception_state);
}

}  // namespace

void WasmResponseExtensions::Initialize(v8::Isolate* isolate) {
  isolate->SetWasmStreamingCallback(StreamFromResponseCallback);
}

}  // namespace blink
