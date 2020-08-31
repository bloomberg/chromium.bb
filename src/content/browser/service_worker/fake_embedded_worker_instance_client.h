// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_FAKE_EMBEDDED_WORKER_INSTANCE_CLIENT_H_
#define CONTENT_BROWSER_SERVICE_WORKER_FAKE_EMBEDDED_WORKER_INSTANCE_CLIENT_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"

namespace content {

class EmbeddedWorkerTestHelper;

// The default fake for blink::mojom::EmbeddedWorkerInstanceClient. It responds
// to Start/Stop/etc messages without starting an actual service worker thread.
// It is owned by EmbeddedWorkerTestHelper and by default the lifetime is tied
// to the Mojo connection.
class FakeEmbeddedWorkerInstanceClient
    : public blink::mojom::EmbeddedWorkerInstanceClient {
 public:
  // |helper| must outlive this instance.
  explicit FakeEmbeddedWorkerInstanceClient(EmbeddedWorkerTestHelper* helper);
  ~FakeEmbeddedWorkerInstanceClient() override;

  EmbeddedWorkerTestHelper* helper() { return helper_; }

  base::WeakPtr<FakeEmbeddedWorkerInstanceClient> GetWeakPtr();

  mojo::AssociatedRemote<blink::mojom::EmbeddedWorkerInstanceHost>& host() {
    return host_;
  }

  void Bind(mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient>
                receiver);
  void RunUntilBound();

  // Closes the binding and deletes |this|.
  void Disconnect();

 protected:
  // blink::mojom::EmbeddedWorkerInstanceClient implementation.
  void StartWorker(blink::mojom::EmbeddedWorkerStartParamsPtr params) override;
  void StopWorker() override;

  virtual void EvaluateScript();

  void DidPopulateScriptCacheMap();

  blink::mojom::EmbeddedWorkerStartParamsPtr& start_params() {
    return start_params_;
  }

  virtual void OnConnectionError();

 private:
  void CallOnConnectionError();

  // |helper_| owns |this|.
  EmbeddedWorkerTestHelper* const helper_;

  blink::mojom::EmbeddedWorkerStartParamsPtr start_params_;
  mojo::AssociatedRemote<blink::mojom::EmbeddedWorkerInstanceHost> host_;

  mojo::Receiver<blink::mojom::EmbeddedWorkerInstanceClient> receiver_{this};
  base::OnceClosure quit_closure_for_bind_;

  base::WeakPtrFactory<FakeEmbeddedWorkerInstanceClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeEmbeddedWorkerInstanceClient);
};

// A EmbeddedWorkerInstanceClient fake that doesn't respond to the Start/Stop
// message until instructed to do so.
class DelayedFakeEmbeddedWorkerInstanceClient
    : public FakeEmbeddedWorkerInstanceClient {
 public:
  explicit DelayedFakeEmbeddedWorkerInstanceClient(
      EmbeddedWorkerTestHelper* helper);
  ~DelayedFakeEmbeddedWorkerInstanceClient() override;

  // Unblocks the Start/StopWorker() call to this instance. May be called before
  // or after the Start/StopWorker() call.
  void UnblockStartWorker();
  void UnblockStopWorker();

  // Returns after Start/StopWorker() is called.
  void RunUntilStartWorker();
  void RunUntilStopWorker();

 protected:
  void StartWorker(blink::mojom::EmbeddedWorkerStartParamsPtr params) override;
  void StopWorker() override;

 private:
  void CompleteStopWorker();

  enum class State { kWillBlock, kWontBlock, kBlocked, kCompleted };
  State start_state_ = State::kWillBlock;
  State stop_state_ = State::kWillBlock;
  base::OnceClosure quit_closure_for_start_worker_;
  base::OnceClosure quit_closure_for_stop_worker_;

  // Valid after StartWorker() until start is unblocked.
  blink::mojom::EmbeddedWorkerStartParamsPtr start_params_;

  DISALLOW_COPY_AND_ASSIGN(DelayedFakeEmbeddedWorkerInstanceClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_FAKE_EMBEDDED_WORKER_INSTANCE_CLIENT_H_
