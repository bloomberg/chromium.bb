/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "web/IndexedDBClientImpl.h"

#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/ContentSettingsClient.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/web/WebKit.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WorkerContentSettingsClient.h"

namespace blink {

IndexedDBClient* IndexedDBClientImpl::Create(LocalFrame& frame) {
  return new IndexedDBClientImpl(frame);
}

IndexedDBClient* IndexedDBClientImpl::Create(WorkerClients& worker_clients) {
  return new IndexedDBClientImpl(worker_clients);
}

IndexedDBClientImpl::IndexedDBClientImpl(LocalFrame& frame)
    : IndexedDBClient(frame) {}

IndexedDBClientImpl::IndexedDBClientImpl(WorkerClients& worker_clients)
    : IndexedDBClient(worker_clients) {}

bool IndexedDBClientImpl::AllowIndexedDB(ExecutionContext* context,
                                         const String& name) {
  DCHECK(context->IsContextThread());
  SECURITY_DCHECK(context->IsDocument() || context->IsWorkerGlobalScope());

  if (context->IsDocument()) {
    Document* document = ToDocument(context);
    LocalFrame* frame = document->GetFrame();
    if (!frame)
      return false;
    DCHECK(frame->GetContentSettingsClient());
    return frame->GetContentSettingsClient()->AllowIndexedDB(
        name, context->GetSecurityOrigin());
  }

  WorkerGlobalScope& worker_global_scope = *ToWorkerGlobalScope(context);
  return WorkerContentSettingsClient::From(worker_global_scope)
      ->AllowIndexedDB(name);
}

}  // namespace blink
