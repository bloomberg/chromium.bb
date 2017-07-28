// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkletModuleResponsesMap.h"

#include "platform/wtf/Optional.h"

namespace blink {

namespace {

bool IsValidURL(const KURL& url) {
  return !url.IsEmpty() && url.IsValid();
}

}  // namespace

class WorkletModuleResponsesMap::Entry
    : public GarbageCollectedFinalized<Entry> {
 public:
  enum class State { kFetching, kFetched, kFailed };

  State GetState() const { return state_; }
  ModuleScriptCreationParams GetParams() const { return *params_; }

  void AddClient(Client* client) {
    // Clients can be added only while a module script is being fetched.
    DCHECK_EQ(State::kFetching, state_);
    clients_.push_back(client);
  }

  void NotifyUpdate(const ModuleScriptCreationParams& params) {
    AdvanceState(State::kFetched);
    params_.emplace(params);
    for (Client* client : clients_)
      client->OnRead(params);
    clients_.clear();
  }

  void NotifyFailure() {
    AdvanceState(State::kFailed);
    for (Client* client : clients_)
      client->OnFailed();
    clients_.clear();
  }

  DEFINE_INLINE_TRACE() { visitor->Trace(clients_); }

 private:
  void AdvanceState(State new_state) {
    switch (state_) {
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

  State state_ = State::kFetching;
  WTF::Optional<ModuleScriptCreationParams> params_;
  HeapVector<Member<Client>> clients_;
};

// Implementation of the first half of the custom fetch defined in the
// "fetch a worklet script" algorithm:
// https://drafts.css-houdini.org/worklets/#fetch-a-worklet-script
//
// "To perform the fetch given request, perform the following steps:"
// Step 1: "Let cache be the moduleResponsesMap."
// Step 2: "Let url be request's url."
void WorkletModuleResponsesMap::ReadOrCreateEntry(const KURL& url,
                                                  Client* client) {
  DCHECK(IsMainThread());
  if (!IsValidURL(url)) {
    client->OnFailed();
    return;
  }

  auto it = entries_.find(url);
  if (it != entries_.end()) {
    Entry* entry = it->value;
    switch (entry->GetState()) {
      case Entry::State::kFetching:
        // Step 3: "If cache contains an entry with key url whose value is
        // "fetching", wait until that entry's value changes, then proceed to
        // the next step."
        entry->AddClient(client);
        return;
      case Entry::State::kFetched:
        // Step 4: "If cache contains an entry with key url, asynchronously
        // complete this algorithm with that entry's value, and abort these
        // steps."
        client->OnRead(entry->GetParams());
        return;
      case Entry::State::kFailed:
        // Module fetching failed before. Abort following steps.
        client->OnFailed();
        return;
    }
  }

  // Step 5: "Create an entry in cache with key url and value "fetching"."
  entries_.insert(url, new Entry);

  // Step 6: "Fetch request."
  // Running the callback with an empty params will make the fetcher to fallback
  // to regular module loading and Write() will be called once the fetch is
  // complete.
  client->OnFetchNeeded();
}

// Implementation of the second half of the custom fetch defined in the
// "fetch a worklet script" algorithm:
// https://drafts.css-houdini.org/worklets/#fetch-a-worklet-script
void WorkletModuleResponsesMap::UpdateEntry(
    const KURL& url,
    const ModuleScriptCreationParams& params) {
  DCHECK(IsMainThread());
  DCHECK(IsValidURL(url));
  DCHECK(entries_.Contains(url));
  Entry* entry = entries_.find(url)->value;

  // Step 7: "Let response be the result of fetch when it asynchronously
  // completes."
  // Step 8: "Set the value of the entry in cache whose key is url to response,
  // and asynchronously complete this algorithm with response."
  entry->NotifyUpdate(params);
}

void WorkletModuleResponsesMap::InvalidateEntry(const KURL& url) {
  DCHECK(IsMainThread());
  DCHECK(IsValidURL(url));
  DCHECK(entries_.Contains(url));
  Entry* entry = entries_.find(url)->value;
  entry->NotifyFailure();
}

DEFINE_TRACE(WorkletModuleResponsesMap) {
  visitor->Trace(entries_);
}

}  // namespace blink
