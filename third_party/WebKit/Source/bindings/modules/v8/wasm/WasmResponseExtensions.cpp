// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/modules/v8/wasm/WasmResponseExtensions.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/modules/v8/V8Response.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/FetchDataLoader.h"
#include "platform/heap/Handle.h"
#include "wtf/RefPtr.h"

namespace blink {

namespace {

class FetchDataLoaderAsWasmModule final : public FetchDataLoader,
                                          public BytesConsumer::Client {
  USING_GARBAGE_COLLECTED_MIXIN(FetchDataLoaderAsWasmModule);

 public:
  FetchDataLoaderAsWasmModule(ScriptPromiseResolver* resolver,
                              ScriptState* scriptState)
      : m_resolver(resolver),
        m_builder(scriptState->isolate()),
        m_scriptState(scriptState) {}

  void start(BytesConsumer* consumer,
             FetchDataLoader::Client* client) override {
    DCHECK(!m_consumer);
    DCHECK(!m_client);
    m_client = client;
    m_consumer = consumer;
    m_consumer->setClient(this);
    onStateChange();
  }

  void onStateChange() override {
    while (true) {
      // {buffer} is owned by {m_consumer}.
      const char* buffer = nullptr;
      size_t available = 0;
      BytesConsumer::Result result = m_consumer->beginRead(&buffer, &available);

      if (result == BytesConsumer::Result::ShouldWait)
        return;
      if (result == BytesConsumer::Result::Ok) {
        if (available > 0) {
          DCHECK_NE(buffer, nullptr);
          m_builder.OnBytesReceived(reinterpret_cast<const uint8_t*>(buffer),
                                    available);
        }
        result = m_consumer->endRead(available);
      }
      switch (result) {
        case BytesConsumer::Result::ShouldWait:
          NOTREACHED();
          return;
        case BytesConsumer::Result::Ok: {
          break;
        }
        case BytesConsumer::Result::Done: {
          v8::Isolate* isolate = m_scriptState->isolate();
          ScriptState::Scope scope(m_scriptState.get());

          {
            // The TryCatch destructor will clear the exception. We
            // scope the block here to ensure tight control over the
            // lifetime of the exception.
            v8::TryCatch trycatch(isolate);
            v8::Local<v8::WasmCompiledModule> module;
            if (m_builder.Finish().ToLocal(&module)) {
              DCHECK(!trycatch.HasCaught());
              ScriptValue scriptValue(m_scriptState.get(), module);
              m_resolver->resolve(scriptValue);
            } else {
              DCHECK(trycatch.HasCaught());
              m_resolver->reject(trycatch.Exception());
            }
          }

          m_client->didFetchDataLoadedCustomFormat();
          return;
        }
        case BytesConsumer::Result::Error: {
          // TODO(mtrofin): do we need an abort on the wasm side?
          // Something like "m_outStream->abort()" maybe?
          return rejectPromise();
        }
      }
    }
  }

  void cancel() override {
    m_consumer->cancel();
    return rejectPromise();
  }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_consumer);
    visitor->trace(m_resolver);
    visitor->trace(m_client);
    FetchDataLoader::trace(visitor);
    BytesConsumer::Client::trace(visitor);
  }

 private:
  // TODO(mtrofin): replace with spec-ed error types, once spec clarifies
  // what they are.
  void rejectPromise() {
    m_resolver->reject(V8ThrowException::createTypeError(
        m_scriptState->isolate(), "Could not download wasm module"));
  }
  Member<BytesConsumer> m_consumer;
  Member<ScriptPromiseResolver> m_resolver;
  Member<FetchDataLoader::Client> m_client;
  v8::WasmModuleObjectBuilder m_builder;
  const RefPtr<ScriptState> m_scriptState;
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
  explicit WasmDataLoaderClient() {}
  void didFetchDataLoadedCustomFormat() override {}
  void didFetchDataLoadFailed() override { NOTREACHED(); }
};

// This callback may be entered as a promise is resolved, or directly
// from the overload callback.
// See
// https://github.com/WebAssembly/design/blob/master/Web.md#webassemblycompile
void compileFromResponseCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  ExceptionState exceptionState(args.GetIsolate(),
                                ExceptionState::ExecutionContext, "WebAssembly",
                                "compile");
  ExceptionToRejectPromiseScope rejectPromiseScope(args, exceptionState);

  ScriptState* scriptState = ScriptState::forReceiverObject(args);
  if (!scriptState->getExecutionContext()) {
    v8SetReturnValue(args, ScriptPromise().v8Value());
    return;
  }

  if (args.Length() < 1 || !args[0]->IsObject() ||
      !V8Response::hasInstance(args[0], args.GetIsolate())) {
    v8SetReturnValue(
        args, ScriptPromise::reject(
                  scriptState, V8ThrowException::createTypeError(
                                   scriptState->isolate(),
                                   "Promise argument must be called with a "
                                   "Promise<Response> object"))
                  .v8Value());
    return;
  }

  Response* response = V8Response::toImpl(v8::Local<v8::Object>::Cast(args[0]));
  ScriptPromise promise;
  if (response->isBodyLocked() || response->bodyUsed()) {
    promise = ScriptPromise::reject(
        scriptState,
        V8ThrowException::createTypeError(
            scriptState->isolate(),
            "Cannot compile WebAssembly.Module from an already read Response"));
  } else {
    ScriptPromiseResolver* resolver =
        ScriptPromiseResolver::create(scriptState);
    if (response->bodyBuffer()) {
      promise = resolver->promise();
      response->bodyBuffer()->startLoading(
          new FetchDataLoaderAsWasmModule(resolver, scriptState),
          new WasmDataLoaderClient());
    } else {
      promise = ScriptPromise::reject(
          scriptState,
          V8ThrowException::createTypeError(
              scriptState->isolate(), "Response object has a null body."));
    }
  }
  v8SetReturnValue(args, promise.v8Value());
}

// See https://crbug.com/708238 for tracking avoiding the hand-generated code.
bool wasmCompileOverload(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 1 || !args[0]->IsObject())
    return false;

  if (!args[0]->IsPromise() &&
      !V8Response::hasInstance(args[0], args.GetIsolate()))
    return false;

  v8::Isolate* isolate = args.GetIsolate();
  ScriptState* scriptState = ScriptState::forReceiverObject(args);

  v8::Local<v8::Function> compileCallback =
      v8::Function::New(isolate, compileFromResponseCallback);

  ScriptPromiseResolver* scriptPromiseResolver =
      ScriptPromiseResolver::create(scriptState);
  // treat either case of parameter as
  // Promise.resolve(parameter)
  // as per https://www.w3.org/2001/tag/doc/promises-guide#resolve-arguments

  // Ending with:
  //    return Promise.resolve(parameter).then(compileCallback);
  ScriptPromise parameterAsPromise = scriptPromiseResolver->promise();
  v8SetReturnValue(args, ScriptPromise::cast(scriptState, args[0])
                             .then(compileCallback)
                             .v8Value());

  // resolve the first parameter promise.
  scriptPromiseResolver->resolve(ScriptValue::from(scriptState, args[0]));
  return true;
}

}  // namespace

void WasmResponseExtensions::initialize(v8::Isolate* isolate) {
  isolate->SetWasmCompileCallback(wasmCompileOverload);
}

}  // namespace blink
