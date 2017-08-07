/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "StorageQuotaClient.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/page/Page.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/quota/DeprecatedStorageQuotaCallbacksImpl.h"
#include "modules/quota/StorageErrorCallback.h"
#include "modules/quota/StorageQuotaCallback.h"
#include "public/platform/WebStorageQuotaType.h"
#include "public/web/WebFrameClient.h"

namespace blink {

StorageQuotaClient::StorageQuotaClient() {}

StorageQuotaClient::~StorageQuotaClient() {}

void StorageQuotaClient::RequestQuota(ScriptState* script_state,
                                      WebStorageQuotaType storage_type,
                                      unsigned long long new_quota_in_bytes,
                                      StorageQuotaCallback* success_callback,
                                      StorageErrorCallback* error_callback) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);
  DCHECK(execution_context->IsDocument())
      << "Quota requests are not supported in workers";

  Document* document = ToDocument(execution_context);
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(document->GetFrame());
  StorageQuotaCallbacks* callbacks =
      DeprecatedStorageQuotaCallbacksImpl::Create(success_callback,
                                                  error_callback);
  web_frame->Client()->RequestStorageQuota(storage_type, new_quota_in_bytes,
                                           callbacks);
}

const char* StorageQuotaClient::SupplementName() {
  return "StorageQuotaClient";
}

StorageQuotaClient* StorageQuotaClient::From(ExecutionContext* context) {
  if (!context->IsDocument())
    return 0;
  return static_cast<StorageQuotaClient*>(
      Supplement<Page>::From(ToDocument(context)->GetPage(), SupplementName()));
}

void ProvideStorageQuotaClientTo(Page& page, StorageQuotaClient* client) {
  page.ProvideSupplement(StorageQuotaClient::SupplementName(), client);
}

}  // namespace blink
