// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/WorkletScriptLoader.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "core/loader/FrameFetchContext.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/fetch_initiator_type_names.h"
#include "platform/wtf/WTF.h"

namespace blink {

WorkletScriptLoader::WorkletScriptLoader(ResourceFetcher* fetcher,
                                         Client* client)
    : fetcher_(fetcher), client_(client) {}

void WorkletScriptLoader::FetchScript(const KURL& module_url_record) {
  DCHECK(IsMainThread());
  DCHECK(!GetResource());
  DCHECK(!was_script_load_complete_);

  ResourceRequest resource_request(module_url_record);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextScript);
  ResourceLoaderOptions options;
  options.initiator_info.name = FetchInitiatorTypeNames::internal;
  FetchParameters params(resource_request, options);
  ScriptResource* resource = ScriptResource::Fetch(params, fetcher_);
  if (!resource) {
    NotifyFinished(nullptr);
    return;
  }
  SetResource(resource);
  // notifyFinished() will be called later.
}

void WorkletScriptLoader::Cancel() {
  DCHECK(IsMainThread());
  if (!GetResource() || was_script_load_complete_)
    return;
  NotifyFinished(nullptr);
}

void WorkletScriptLoader::NotifyFinished(Resource* resource) {
  DCHECK(IsMainThread());
  DCHECK(!was_script_load_complete_);
  ClearResource();
  was_script_load_complete_ = true;
  if (!resource || resource->ErrorOccurred()) {
    client_->NotifyWorkletScriptLoadingFinished(this, ScriptSourceCode());
  } else {
    was_script_load_successful_ = true;
    client_->NotifyWorkletScriptLoadingFinished(
        this, ScriptSourceCode(static_cast<ScriptResource*>(resource)));
  }
  fetcher_ = nullptr;
  client_ = nullptr;
}

bool WorkletScriptLoader::WasScriptLoadSuccessful() const {
  DCHECK(was_script_load_complete_);
  return was_script_load_successful_;
}

void WorkletScriptLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(fetcher_);
  visitor->Trace(client_);
  ResourceOwner<ScriptResource, ScriptResourceClient>::Trace(visitor);
}

}  // namespace blink
