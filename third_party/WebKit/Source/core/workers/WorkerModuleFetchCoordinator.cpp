// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerModuleFetchCoordinator.h"

#include "core/loader/modulescript/DocumentModuleScriptFetcher.h"
#include "core/loader/modulescript/ModuleScriptFetcher.h"
#include "platform/loader/fetch/ResourceFetcher.h"

namespace blink {

// Represents an inflight module fetch request.
class WorkerModuleFetchCoordinator::Request final
    : public GarbageCollectedFinalized<Request>,
      public ModuleScriptFetcher::Client {
  USING_GARBAGE_COLLECTED_MIXIN(WorkerModuleFetchCoordinator::Request);

 public:
  enum class State { kInitial, kFetching, kFetched, kFailed };

  static Request* Create(
      WorkerModuleFetchCoordinator* coordinator,
      ResourceFetcher* resource_fetcher,
      WorkerOrWorkletModuleFetchCoordinator::Client* client) {
    DCHECK(IsMainThread());
    return new Request(coordinator, resource_fetcher, client);
  }

  void Fetch(FetchParameters& fetch_params) {
    DCHECK(IsMainThread());
    DCHECK_EQ(State::kInitial, state_);
    AdvanceState(State::kFetching);
    module_fetcher_->Fetch(fetch_params, this);
  }

  void NotifyFetchFinished(
      const WTF::Optional<ModuleScriptCreationParams>& params,
      const HeapVector<Member<ConsoleMessage>>& error_messages) override {
    DCHECK(IsMainThread());
    // The request can be aborted during the resource fetch.
    if (state_ == State::kFailed)
      return;
    if (params) {
      AdvanceState(State::kFetched);
      client_->OnFetched(*params);
    } else {
      AdvanceState(State::kFailed);
      // TODO(nhiroki): Add |error_messages| to the context's message storage.
      client_->OnFailed();
    }
    module_fetcher_.Clear();
    coordinator_->OnFetched(this);
    // 'this' may be destroyed at this point. Don't touch this anymore.
  }

  void Abort() {
    AdvanceState(State::kFailed);
    module_fetcher_.Clear();
    client_->OnFailed();
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(coordinator_);
    visitor->Trace(module_fetcher_);
    visitor->Trace(client_);
  }

 private:
  Request(WorkerModuleFetchCoordinator* coordinator,
          ResourceFetcher* resource_fetcher,
          WorkerOrWorkletModuleFetchCoordinator::Client* client)
      : coordinator_(coordinator),
        module_fetcher_(new DocumentModuleScriptFetcher(resource_fetcher)),
        client_(client) {}

  void AdvanceState(State new_state) {
    switch (state_) {
      case State::kInitial:
        DCHECK_EQ(new_state, State::kFetching);
        break;
      case State::kFetching:
        DCHECK(new_state == State::kFetched || new_state == State::kFailed);
        break;
      case State::kFetched:
      case State::kFailed:
        NOTREACHED();
        break;
    }
    state_ = new_state;
  }

  State state_ = State::kInitial;

  Member<WorkerModuleFetchCoordinator> coordinator_;
  Member<DocumentModuleScriptFetcher> module_fetcher_;
  Member<WorkerOrWorkletModuleFetchCoordinator::Client> client_;
};

WorkerModuleFetchCoordinator* WorkerModuleFetchCoordinator::Create(
    ResourceFetcher* resource_fetcher) {
  return new WorkerModuleFetchCoordinator(resource_fetcher);
}

void WorkerModuleFetchCoordinator::Fetch(FetchParameters& fetch_params,
                                         Client* client) {
  DCHECK(IsMainThread());
  Request* request = Request::Create(this, resource_fetcher_, client);
  inflight_requests_.insert(request);
  request->Fetch(fetch_params);
}

void WorkerModuleFetchCoordinator::Dispose() {
  DCHECK(IsMainThread());
  for (auto request : inflight_requests_)
    request->Abort();
  inflight_requests_.clear();
}

void WorkerModuleFetchCoordinator::Trace(blink::Visitor* visitor) {
  visitor->Trace(resource_fetcher_);
  visitor->Trace(inflight_requests_);
}

WorkerModuleFetchCoordinator::WorkerModuleFetchCoordinator(
    ResourceFetcher* resource_fetcher)
    : resource_fetcher_(resource_fetcher) {}

void WorkerModuleFetchCoordinator::OnFetched(Request* request) {
  DCHECK(IsMainThread());
  DCHECK(inflight_requests_.Contains(request));
  inflight_requests_.erase(request);
}

}  // namespace blink
