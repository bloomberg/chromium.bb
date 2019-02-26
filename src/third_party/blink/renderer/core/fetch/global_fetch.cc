// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/global_fetch.h"

#include "third_party/blink/renderer/core/fetch/fetch_manager.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/request_init.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

template <typename T>
class GlobalFetchImpl final
    : public GarbageCollectedFinalized<GlobalFetchImpl<T>>,
      public GlobalFetch::ScopedFetcher,
      public Supplement<T> {
  USING_GARBAGE_COLLECTED_MIXIN(GlobalFetchImpl);

 public:
  static const char kSupplementName[];

  static ScopedFetcher* From(T& supplementable,
                             ExecutionContext* execution_context) {
    GlobalFetchImpl* supplement =
        Supplement<T>::template From<GlobalFetchImpl>(supplementable);
    if (!supplement) {
      supplement = new GlobalFetchImpl(execution_context);
      Supplement<T>::ProvideTo(supplementable, supplement);
    }
    return supplement;
  }

  ScriptPromise Fetch(ScriptState* script_state,
                      const RequestInfo& input,
                      const RequestInit* init,
                      ExceptionState& exception_state) override {
    ExecutionContext* execution_context = fetch_manager_->GetExecutionContext();
    if (!script_state->ContextIsValid() || !execution_context) {
      // TODO(yhirano): Should this be moved to bindings?
      exception_state.ThrowTypeError("The global scope is shutting down.");
      return ScriptPromise();
    }

    // "Let |r| be the associated request of the result of invoking the
    // initial value of Request as constructor with |input| and |init| as
    // arguments. If this throws an exception, reject |p| with it."
    Request* r = Request::Create(script_state, input, init, exception_state);
    if (exception_state.HadException())
      return ScriptPromise();

    probe::willSendXMLHttpOrFetchNetworkRequest(execution_context, r->url());
    FetchRequestData* request_data =
        r->PassRequestData(script_state, exception_state);
    if (exception_state.HadException())
      return ScriptPromise();
    auto promise = fetch_manager_->Fetch(script_state, request_data,
                                         r->signal(), exception_state);
    if (exception_state.HadException())
      return ScriptPromise();

    return promise;
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(fetch_manager_);
    ScopedFetcher::Trace(visitor);
    Supplement<T>::Trace(visitor);
  }

 private:
  explicit GlobalFetchImpl(ExecutionContext* execution_context)
      : fetch_manager_(FetchManager::Create(execution_context)) {}

  Member<FetchManager> fetch_manager_;
};

// static
template <typename T>
const char GlobalFetchImpl<T>::kSupplementName[] = "GlobalFetchImpl";

}  // namespace

GlobalFetch::ScopedFetcher::~ScopedFetcher() {}

GlobalFetch::ScopedFetcher* GlobalFetch::ScopedFetcher::From(
    LocalDOMWindow& window) {
  return GlobalFetchImpl<LocalDOMWindow>::From(window,
                                               window.GetExecutionContext());
}

GlobalFetch::ScopedFetcher* GlobalFetch::ScopedFetcher::From(
    WorkerGlobalScope& worker) {
  return GlobalFetchImpl<WorkerGlobalScope>::From(worker,
                                                  worker.GetExecutionContext());
}

void GlobalFetch::ScopedFetcher::Trace(blink::Visitor* visitor) {}

ScriptPromise GlobalFetch::fetch(ScriptState* script_state,
                                 LocalDOMWindow& window,
                                 const RequestInfo& input,
                                 const RequestInit* init,
                                 ExceptionState& exception_state) {
  UseCounter::Count(window.GetExecutionContext(), WebFeature::kFetch);
  if (!window.GetFrame()) {
    exception_state.ThrowTypeError("The global scope is shutting down.");
    return ScriptPromise();
  }
  return ScopedFetcher::From(window)->Fetch(script_state, input, init,
                                            exception_state);
}

ScriptPromise GlobalFetch::fetch(ScriptState* script_state,
                                 WorkerGlobalScope& worker,
                                 const RequestInfo& input,
                                 const RequestInit* init,
                                 ExceptionState& exception_state) {
  UseCounter::Count(worker.GetExecutionContext(), WebFeature::kFetch);
  return ScopedFetcher::From(worker)->Fetch(script_state, input, init,
                                            exception_state);
}

}  // namespace blink
