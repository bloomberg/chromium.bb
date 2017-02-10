// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/CompositorWorkerProxyClient.h"

namespace blink {

CompositorWorkerProxyClient* CompositorWorkerProxyClient::from(
    WorkerClients* clients) {
  return static_cast<CompositorWorkerProxyClient*>(
      Supplement<WorkerClients>::from(clients, supplementName()));
}

const char* CompositorWorkerProxyClient::supplementName() {
  return "CompositorWorkerProxyClient";
}

void provideCompositorWorkerProxyClientTo(WorkerClients* clients,
                                          CompositorWorkerProxyClient* client) {
  clients->provideSupplement(CompositorWorkerProxyClient::supplementName(),
                             client);
}

}  // namespace blink
