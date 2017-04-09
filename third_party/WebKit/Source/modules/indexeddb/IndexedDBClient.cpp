// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IndexedDBClient.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"

namespace blink {

IndexedDBClient::IndexedDBClient(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {}

IndexedDBClient::IndexedDBClient(WorkerClients& clients)
    : Supplement<WorkerClients>(clients) {}

IndexedDBClient* IndexedDBClient::From(ExecutionContext* context) {
  if (context->IsDocument())
    return static_cast<IndexedDBClient*>(Supplement<LocalFrame>::From(
        ToDocument(*context).GetFrame(), SupplementName()));

  WorkerClients* clients = ToWorkerGlobalScope(*context).Clients();
  ASSERT(clients);
  return static_cast<IndexedDBClient*>(
      Supplement<WorkerClients>::From(clients, SupplementName()));
}

const char* IndexedDBClient::SupplementName() {
  return "IndexedDBClient";
}

DEFINE_TRACE(IndexedDBClient) {
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
