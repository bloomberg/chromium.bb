// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_CONNECT_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_CONNECT_LISTENER_H_

#include "third_party/blink/public/mojom/worker/shared_worker_client.mojom-blink.h"

#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class SharedWorker;

class SharedWorkerConnectListener final
    : public mojom::blink::SharedWorkerClient {
 public:
  explicit SharedWorkerConnectListener(SharedWorker*);
  ~SharedWorkerConnectListener() override;

  // mojom::blink::SharedWorkerClient overrides.
  void OnCreated(mojom::SharedWorkerCreationContextType) override;
  void OnConnected(const Vector<mojom::WebFeature>& features_used) override;
  void OnScriptLoadFailed() override;
  void OnFeatureUsed(mojom::WebFeature feature) override;

 private:
  Persistent<SharedWorker> worker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_SHARED_WORKER_CONNECT_LISTENER_H_
