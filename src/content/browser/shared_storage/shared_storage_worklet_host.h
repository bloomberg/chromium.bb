// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_

#include "content/common/content_export.h"
#include "content/services/shared_storage_worklet/public/mojom/shared_storage_worklet_service.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"
#include "url/origin.h"

namespace content {

class SharedStorageDocumentServiceImpl;
class SharedStorageURLLoaderFactoryProxy;
class SharedStorageWorkletDriver;

// The SharedStorageWorkletHost is responsible for getting worklet operation
// requests (i.e. addModule and runOperation) from the renderer (i.e. document
// that is hosting the worklet) and running it on the
// `SharedStorageWorkletService`. It will also handle the commands from the
// `SharedStorageWorkletService` (i.e. storage access, console log) which
// could happen while running those worklet operations.
//
// The SharedStorageWorkletHost lives in the `SharedStorageWorkletHostManager`,
// and the SharedStorageWorkletHost's lifetime is bounded by the earliest of the
// two timepoints:
// 1. When the outstanding worklet operations have finished on or after the
// document's destruction, but before the keepalive timeout.
// 2. The keepalive timeout is reached after the worklet's owner document is
// destroyed.
class CONTENT_EXPORT SharedStorageWorkletHost
    : public shared_storage_worklet::mojom::SharedStorageWorkletServiceClient {
 public:
  enum class AddModuleState {
    kNotInitiated,
    kInitiated,
  };

  using KeepAliveFinishedCallback =
      base::OnceCallback<void(SharedStorageWorkletHost*)>;

  explicit SharedStorageWorkletHost(
      std::unique_ptr<SharedStorageWorkletDriver> driver,
      SharedStorageDocumentServiceImpl& document_service);
  ~SharedStorageWorkletHost() override;

  void AddModuleOnWorklet(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          frame_url_loader_factory,
      const url::Origin& frame_origin,
      const GURL& script_source_url,
      blink::mojom::SharedStorageDocumentService::AddModuleOnWorkletCallback
          callback);
  void RunOperationOnWorklet(const std::string& name,
                             const std::vector<uint8_t>& serialized_data);

  // Whether there are unfinished worklet operations (i.e. addModule() and
  // runOperation()).
  bool HasPendingOperations();

  // Called by the `SharedStorageWorkletHostManager` for this host to enter
  // keep-alive phase.
  void EnterKeepAliveOnDocumentDestroyed(KeepAliveFinishedCallback callback);

  // shared_storage_worklet::mojom::SharedStorageWorkletServiceClient.
  void SharedStorageSet(const std::string& key,
                        const std::string& value,
                        bool ignore_if_present,
                        SharedStorageSetCallback callback) override;
  void SharedStorageAppend(const std::string& key,
                           const std::string& value,
                           SharedStorageAppendCallback callback) override;
  void SharedStorageDelete(const std::string& key,
                           SharedStorageDeleteCallback callback) override;
  void SharedStorageClear(SharedStorageClearCallback callback) override;
  void SharedStorageGet(const std::string& key,
                        SharedStorageGetCallback callback) override;
  void SharedStorageKeys(
      mojo::PendingRemote<
          shared_storage_worklet::mojom::SharedStorageEntriesListener>
          pending_listener) override;
  void SharedStorageEntries(
      mojo::PendingRemote<
          shared_storage_worklet::mojom::SharedStorageEntriesListener>
          pending_listener) override;
  void SharedStorageLength(SharedStorageLengthCallback callback) override;
  void ConsoleLog(const std::string& message) override;

 protected:
  // virtual for testing
  virtual void OnAddModuleOnWorkletFinished(
      blink::mojom::SharedStorageDocumentService::AddModuleOnWorkletCallback
          callback,
      bool success,
      const std::string& error_message);

  virtual void OnRunOperationOnWorkletFinished(
      bool success,
      const std::string& error_message);

  base::OneShotTimer& GetKeepAliveTimerForTesting() {
    return keep_alive_timer_;
  }

 private:
  // Returns whether the the worklet has entered keep-alive phase. During
  // keep-alive: the attempt to log console messages will be ignored; and the
  // completion of the last pending operation will terminate the worklet.
  bool IsInKeepAlivePhase() const;

  // Run `keep_alive_finished_callback_` to destroy `this`. Called when the last
  // pending operation has finished, or when a timeout is reached after entering
  // the keep-alive phase.
  void FinishKeepAlive();

  // Increment `pending_operations_count_`. Called when receiving a addModule
  // or runOperation operation.
  void IncrementPendingOperationsCount();

  // Decrement `pending_operations_count_`. Called when finishing handling a
  // addModule or runOperation operation.
  void DecrementPendingOperationsCount();

  // virtual for testing
  virtual base::TimeDelta GetKeepAliveTimeout() const;

  shared_storage_worklet::mojom::SharedStorageWorkletService*
  GetAndConnectToSharedStorageWorkletService();

  AddModuleState add_module_state_ = AddModuleState::kNotInitiated;

  // Responsible for initializing the `SharedStorageWorkletService`.
  std::unique_ptr<SharedStorageWorkletDriver> driver_;

  // When this worklet is created, `document_service_` is set to the associated
  // `SharedStorageDocumentServiceImpl` and is always valid. Reset automatically
  // when `SharedStorageDocumentServiceImpl` is destroyed.
  base::WeakPtr<SharedStorageDocumentServiceImpl> document_service_;

  // The number of unfinished worklet requests, including addModule and
  // runOperation.
  uint32_t pending_operations_count_ = 0u;

  // Timer for starting and ending the keep-alive phase.
  base::OneShotTimer keep_alive_timer_;

  // Set when the worklet host enters keep-alive phase.
  KeepAliveFinishedCallback keep_alive_finished_callback_;

  // Both `shared_storage_worklet_service_`
  // and `shared_storage_worklet_service_client_` are bound in
  // GetAndConnectToSharedStorageWorkletService().
  //
  // `shared_storage_worklet_service_client_` is associated specifically with
  // the pipe that `shared_storage_worklet_service_` runs on, as we want the
  // messages initiated from the worklet (e.g. storage access, console log) to
  // be well ordered with respect to the corresponding request's callback
  // message which will be interpreted as the completion of an operation.
  mojo::Remote<shared_storage_worklet::mojom::SharedStorageWorkletService>
      shared_storage_worklet_service_;

  mojo::AssociatedReceiver<
      shared_storage_worklet::mojom::SharedStorageWorkletServiceClient>
      shared_storage_worklet_service_client_{this};

  // The proxy is used to limit the request that the worklet can make, e.g. to
  // ensure the URL is not modified by a compromised worklet; to enforce the
  // application/javascript request header; to enforce same-origin mode; etc.
  std::unique_ptr<SharedStorageURLLoaderFactoryProxy> url_loader_factory_proxy_;

  base::WeakPtrFactory<SharedStorageWorkletHost> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_
