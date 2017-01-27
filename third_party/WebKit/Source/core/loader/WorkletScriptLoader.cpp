// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/WorkletScriptLoader.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "core/loader/FrameFetchContext.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "wtf/WTF.h"

namespace blink {

WorkletScriptLoader::WorkletScriptLoader(ResourceFetcher* fetcher,
                                         Client* client)
    : m_fetcher(fetcher), m_client(client) {}

void WorkletScriptLoader::fetchScript(const String& scriptURL) {
  DCHECK(isMainThread());
  DCHECK(!resource());
  DCHECK(!m_wasScriptLoadComplete);

  ResourceRequest resourceRequest(scriptURL);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextScript);
  FetchRequest request(resourceRequest, FetchInitiatorTypeNames::internal);
  ScriptResource* resource = ScriptResource::fetch(request, m_fetcher);
  if (!resource) {
    notifyFinished(nullptr);
    return;
  }
  setResource(resource);
  // notifyFinished() will be called later.
}

void WorkletScriptLoader::cancel() {
  DCHECK(isMainThread());
  if (!resource() || m_wasScriptLoadComplete)
    return;
  notifyFinished(nullptr);
}

void WorkletScriptLoader::notifyFinished(Resource* resource) {
  DCHECK(isMainThread());
  DCHECK(!m_wasScriptLoadComplete);
  clearResource();
  m_wasScriptLoadComplete = true;
  if (!resource || resource->errorOccurred()) {
    m_client->notifyWorkletScriptLoadingFinished(this, ScriptSourceCode());
  } else {
    m_wasScriptLoadSuccessful = true;
    m_client->notifyWorkletScriptLoadingFinished(
        this, ScriptSourceCode(static_cast<ScriptResource*>(resource)));
  }
  m_fetcher = nullptr;
  m_client = nullptr;
}

bool WorkletScriptLoader::wasScriptLoadSuccessful() const {
  DCHECK(m_wasScriptLoadComplete);
  return m_wasScriptLoadSuccessful;
}

DEFINE_TRACE(WorkletScriptLoader) {
  visitor->trace(m_fetcher);
  visitor->trace(m_client);
  ResourceOwner<ScriptResource, ScriptResourceClient>::trace(visitor);
}

}  // namespace blink
