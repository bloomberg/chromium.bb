// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/WorkletModuleScriptFetcher.h"

#include "platform/CrossThreadFunctional.h"

namespace blink {

WorkletModuleScriptFetcher::WorkletModuleScriptFetcher(
    WorkletModuleResponsesMapProxy* module_responses_map_proxy)
    : module_responses_map_proxy_(module_responses_map_proxy) {
  DCHECK(module_responses_map_proxy_);
}

void WorkletModuleScriptFetcher::Trace(blink::Visitor* visitor) {
  visitor->Trace(module_responses_map_proxy_);
  ModuleScriptFetcher::Trace(visitor);
}

void WorkletModuleScriptFetcher::Fetch(FetchParameters& fetch_params,
                                       ModuleScriptFetcher::Client* client) {
  SetClient(client);
  module_responses_map_proxy_->ReadEntry(fetch_params, this);
}

void WorkletModuleScriptFetcher::OnRead(
    const ModuleScriptCreationParams& params) {
  Finalize(params, nullptr /* error_message */);
}

void WorkletModuleScriptFetcher::OnFailed() {
  Finalize(WTF::nullopt, nullptr /* error_message */);
}

void WorkletModuleScriptFetcher::Finalize(
    const WTF::Optional<ModuleScriptCreationParams>& params,
    ConsoleMessage* error_message) {
  NotifyFetchFinished(params, error_message);
}

}  // namespace blink
