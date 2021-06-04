// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_STORAGE_TEST_UTILS_H_
#define COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_STORAGE_TEST_UTILS_H_

#include <string>

#include "base/callback.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace storage {

// A test implementation of ServiceWorkerDataPipeStateNotifier.
class FakeServiceWorkerDataPipeStateNotifier
    : public mojom::ServiceWorkerDataPipeStateNotifier {
 public:
  FakeServiceWorkerDataPipeStateNotifier();
  ~FakeServiceWorkerDataPipeStateNotifier() override;

  mojo::PendingRemote<mojom::ServiceWorkerDataPipeStateNotifier>
  BindNewPipeAndPassRemote();

  int32_t WaitUntilComplete();

 private:
  // mojom::ServiceWorkerDataPipeStateNotifier implementations:
  void OnComplete(int32_t status) override;

  absl::optional<int32_t> complete_status_;
  base::OnceClosure on_complete_callback_;
  mojo::Receiver<mojom::ServiceWorkerDataPipeStateNotifier> receiver_{this};
};

namespace test {

// Reads all data from the given |handle| and returns data as a string.
// This is similar to mojo::BlockingCopyToString() but a bit different. This
// doesn't wait synchronously but keeps posting a task when |handle| returns
// MOJO_RESULT_SHOULD_WAIT. In some tests, waiting for consumer handles
// synchronously doesn't work because producers and consumers live on the same
// sequence.
// TODO(bashi): Make producers and consumers live on different sequences then
// use mojo::BlockingCopyToString().
std::string ReadDataPipeViaRunLoop(mojo::ScopedDataPipeConsumerHandle handle);

}  // namespace test
}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_SERVICE_WORKER_SERVICE_WORKER_STORAGE_TEST_UTILS_H_
