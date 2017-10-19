// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IndexedDBClient.h"

#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/ContentSettingsClient.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerContentSettingsClient.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

IndexedDBClient* IndexedDBClient::Create(LocalFrame& frame) {
  return new IndexedDBClient(frame);
}

IndexedDBClient* IndexedDBClient::Create(WorkerClients& worker_clients) {
  return new IndexedDBClient(worker_clients);
}

IndexedDBClient::IndexedDBClient(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {}

IndexedDBClient::IndexedDBClient(WorkerClients& clients)
    : Supplement<WorkerClients>(clients) {}

IndexedDBClient* IndexedDBClient::From(ExecutionContext* context) {
  if (context->IsDocument())
    return static_cast<IndexedDBClient*>(Supplement<LocalFrame>::From(
        ToDocument(*context).GetFrame(), SupplementName()));

  WorkerClients* clients = ToWorkerGlobalScope(*context).Clients();
  DCHECK(clients);
  return static_cast<IndexedDBClient*>(
      Supplement<WorkerClients>::From(clients, SupplementName()));
}

bool IndexedDBClient::AllowIndexedDB(ExecutionContext* context,
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

const char* IndexedDBClient::SupplementName() {
  return "IndexedDBClient";
}

void IndexedDBClient::Trace(blink::Visitor* visitor) {
  Supplement<LocalFrame>::Trace(visitor);
  Supplement<WorkerClients>::Trace(visitor);
}

void ProvideIndexedDBClientTo(LocalFrame& frame, IndexedDBClient* client) {
  Supplement<LocalFrame>::ProvideTo(frame, IndexedDBClient::SupplementName(),
                                    client);
}

void ProvideIndexedDBClientToWorker(WorkerClients* clients,
                                    IndexedDBClient* client) {
  clients->ProvideSupplement(IndexedDBClient::SupplementName(), client);
}

}  // namespace blink
