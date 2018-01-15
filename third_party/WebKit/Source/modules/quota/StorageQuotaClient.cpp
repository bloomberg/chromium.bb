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

#include "modules/quota/StorageQuotaClient.h"

#include "bindings/modules/v8/v8_storage_error_callback.h"
#include "bindings/modules/v8/v8_storage_quota_callback.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/page/Page.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/quota/DOMError.h"
#include "public/platform/TaskType.h"
#include "public/web/WebFrameClient.h"

namespace blink {

namespace {

void RequestStorageQuotaCallback(V8StorageQuotaCallback* success_callback,
                                 V8StorageErrorCallback* error_callback,
                                 mojom::QuotaStatusCode status_code,
                                 int64_t usage_in_bytes,
                                 int64_t granted_quota_in_bytes) {
  if (status_code != mojom::QuotaStatusCode::kOk) {
    if (error_callback) {
      error_callback->InvokeAndReportException(
          nullptr, DOMError::Create(static_cast<ExceptionCode>(status_code)));
    }
    return;
  }

  if (success_callback) {
    success_callback->InvokeAndReportException(nullptr, granted_quota_in_bytes);
  }
}

}  // namespace

StorageQuotaClient::StorageQuotaClient() = default;

StorageQuotaClient::~StorageQuotaClient() = default;

void StorageQuotaClient::RequestQuota(ScriptState* script_state,
                                      mojom::StorageType storage_type,
                                      unsigned long long new_quota_in_bytes,
                                      V8StorageQuotaCallback* success_callback,
                                      V8StorageErrorCallback* error_callback) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);
  DCHECK(execution_context->IsDocument())
      << "Quota requests are not supported in workers";

  Document* document = ToDocument(execution_context);
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(document->GetFrame());
  web_frame->Client()->RequestStorageQuota(
      storage_type, new_quota_in_bytes,
      WTF::Bind(&RequestStorageQuotaCallback,
                WrapPersistentCallbackFunction(success_callback),
                WrapPersistentCallbackFunction(error_callback)));
}

const char* StorageQuotaClient::SupplementName() {
  return "StorageQuotaClient";
}

StorageQuotaClient* StorageQuotaClient::From(ExecutionContext* context) {
  if (!context->IsDocument())
    return nullptr;
  return static_cast<StorageQuotaClient*>(
      Supplement<Page>::From(ToDocument(context)->GetPage(), SupplementName()));
}

void ProvideStorageQuotaClientTo(Page& page, StorageQuotaClient* client) {
  page.ProvideSupplement(StorageQuotaClient::SupplementName(), client);
}

}  // namespace blink
