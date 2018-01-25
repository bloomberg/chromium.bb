// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/wasm/WasmResponseExtensions.h"

#include "base/memory/scoped_refptr.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8Response.h"
#include "core/dom/ExecutionContext.h"
#include "core/fetch/BodyStreamBuffer.h"
#include "core/fetch/FetchDataLoader.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/heap/Handle.h"

namespace blink {

namespace {

class FetchDataLoaderAsWasmModule final : public FetchDataLoader,
                                          public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsWasmModule);

 public:
  FetchDataLoaderAsWasmModule(ScriptState* script_state)
      : builder_(script_state->GetIsolate()), script_state_(script_state) {}

  void Start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!consumer_);
    DCHECK(!client_);
    client_ = client;
    consumer_ = consumer;
    consumer_->SetClient(this);
    OnStateChange();
  }

  v8::Local<v8::Promise> GetPromise() { return builder_.GetPromise(); }

  void OnStateChange() override {
    while (true) {
      // {buffer} is owned by {m_consumer}.
      const char* buffer = nullptr;
      size_t available = 0;
      BytesConsumer::Result result = consumer_->BeginRead(&buffer, &available);

      if (result == BytesConsumer::Result::kShouldWait)
        return;
      if (result == BytesConsumer::Result::kOk) {
        if (available > 0) {
          DCHECK_NE(buffer, nullptr);
          builder_.OnBytesReceived(reinterpret_cast<const uint8_t*>(buffer),
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
          ScriptState::Scope scope(script_state_.get());
          builder_.Finish();
          client_->DidFetchDataLoadedCustomFormat();
          return;
        }
        case BytesConsumer::Result::kError: {
          return AbortCompilation();
        }
      }
    }
  }

  String DebugName() const override { return "FetchDataLoaderAsWasmModule"; }

  void Cancel() override {
    consumer_->Cancel();
    return AbortCompilation();
  }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(consumer_);
    visitor->Trace(client_);
    FetchDataLoader::Trace(visitor);
    BytesConsumer::Client::Trace(visitor);
  }

 private:
  // TODO(mtrofin): replace with spec-ed error types, once spec clarifies
  // what they are.
  void AbortCompilation() {
    ScriptState::Scope scope(script_state_.get());
    if (!ScriptForbiddenScope::IsScriptForbidden()) {
      builder_.Abort(V8ThrowException::CreateTypeError(
          script_state_->GetIsolate(), "Could not download wasm module"));
    } else {
      // We are not allowed to execute a script, which indicates that we should
      // not reject the promise of the streaming compilation. By passing no
      // abort reason, we indicate the V8 side that the promise should not get
      // rejected.
      builder_.Abort(v8::Local<v8::Value>());
    }
  }
  Member<BytesConsumer> consumer_;
  Member<FetchDataLoader::Client> client_;
  v8::WasmModuleObjectBuilderStreaming builder_;
  const scoped_refptr<ScriptState> script_state_;
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
  explicit WasmDataLoaderClient() = default;
  void DidFetchDataLoadedCustomFormat() override {}
  void DidFetchDataLoadFailed() override { NOTREACHED(); }
};

// This callback may be entered as a promise is resolved, or directly
// from the overload callback.
// See
// https://github.com/WebAssembly/design/blob/master/Web.md#webassemblycompile
void CompileFromResponseCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ExceptionState exception_state(args.GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "WebAssembly", "compile");
  ExceptionToRejectPromiseScope reject_promise_scope(args, exception_state);

  ScriptState* script_state = ScriptState::ForCurrentRealm(args);
  if (!ExecutionContext::From(script_state)) {
    V8SetReturnValue(args, ScriptPromise().V8Value());
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

  if (response->MimeType() != "application/wasm") {
    exception_state.ThrowTypeError(
        "Incorrect response MIME type. Expected 'application/wasm'.");
    return;
  }

  if (response->IsBodyLocked() || response->bodyUsed()) {
    exception_state.ThrowTypeError(
        "Cannot compile WebAssembly.Module from an already read Response");
    return;
  }

  if (!response->BodyBuffer()) {
    exception_state.ThrowTypeError("Response object has a null body.");
    return;
  }

  FetchDataLoaderAsWasmModule* loader =
      new FetchDataLoaderAsWasmModule(script_state);
  v8::Local<v8::Value> promise = loader->GetPromise();
  response->BodyBuffer()->StartLoading(loader, new WasmDataLoaderClient());

  V8SetReturnValue(args, promise);
}

// See https://crbug.com/708238 for tracking avoiding the hand-generated code.
void WasmCompileStreamingImpl(const v8::FunctionCallbackInfo<v8::Value>& args) {
  ScriptState* script_state = ScriptState::ForCurrentRealm(args);
  V8PerIsolateData* per_isolate_data =
      V8PerIsolateData::From(script_state->GetIsolate());

  // An unique key of the v8::FunctionTemplate cache in V8PerIsolateData.
  // Everyone uses address of something as a key, so the address of |unique_key|
  // is guaranteed to be unique for the function template cache.
  static const int unique_key = 0;
  v8::Local<v8::FunctionTemplate> function_template =
      per_isolate_data->FindOrCreateOperationTemplate(
          script_state->World(), &unique_key, CompileFromResponseCallback,
          v8::Local<v8::Value>(), v8::Local<v8::Signature>(), 1);
  v8::Local<v8::Function> compile_callback;
  if (!function_template->GetFunction(script_state->GetContext())
           .ToLocal(&compile_callback)) {
    return;  // Throw an exception.
  }

  // treat either case of parameter as
  // Promise.resolve(parameter)
  // as per https://www.w3.org/2001/tag/doc/promises-guide#resolve-arguments

  // Ending with:
  //    return Promise.resolve(parameter).then(compileCallback);
  V8SetReturnValue(args, ScriptPromise::Cast(script_state, args[0])
                             .Then(compile_callback)
                             .V8Value());
}

}  // namespace

void WasmResponseExtensions::Initialize(v8::Isolate* isolate) {
  if (RuntimeEnabledFeatures::WebAssemblyStreamingEnabled()) {
    isolate->SetWasmCompileStreamingCallback(WasmCompileStreamingImpl);
  }
}

}  // namespace blink
