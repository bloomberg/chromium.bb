// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_worklet_host.h"

#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_url_loader_factory_proxy.h"
#include "content/browser/shared_storage/shared_storage_worklet_driver.h"
#include "content/common/renderer.mojom.h"

namespace content {

namespace {

constexpr base::TimeDelta kKeepAliveTimeout = base::Seconds(2);

}  // namespace

SharedStorageWorkletHost::SharedStorageWorkletHost(
    std::unique_ptr<SharedStorageWorkletDriver> driver,
    SharedStorageDocumentServiceImpl& document_service)
    : driver_(std::move(driver)),
      document_service_(document_service.GetWeakPtr()) {}

SharedStorageWorkletHost::~SharedStorageWorkletHost() = default;

void SharedStorageWorkletHost::AddModuleOnWorklet(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        frame_url_loader_factory,
    const url::Origin& frame_origin,
    const GURL& script_source_url,
    blink::mojom::SharedStorageDocumentService::AddModuleOnWorkletCallback
        callback) {
  IncrementPendingOperationsCount();

  if (add_module_state_ == AddModuleState::kInitiated) {
    OnAddModuleOnWorkletFinished(
        std::move(callback), /*success=*/false,
        /*error_message=*/
        "sharedStorage.worklet.addModule() can only be "
        "invoked once per browsing context.");
    return;
  }

  add_module_state_ = AddModuleState::kInitiated;

  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;

  url_loader_factory_proxy_ =
      std::make_unique<SharedStorageURLLoaderFactoryProxy>(
          std::move(frame_url_loader_factory),
          url_loader_factory.InitWithNewPipeAndPassReceiver(), frame_origin,
          script_source_url);

  GetAndConnectToSharedStorageWorkletService()->AddModule(
      std::move(url_loader_factory), script_source_url,
      base::BindOnce(&SharedStorageWorkletHost::OnAddModuleOnWorkletFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SharedStorageWorkletHost::RunOperationOnWorklet(
    const std::string& name,
    const std::vector<uint8_t>& serialized_data) {
  IncrementPendingOperationsCount();

  if (add_module_state_ != AddModuleState::kInitiated) {
    OnRunOperationOnWorkletFinished(
        /*success=*/false,
        /*error_message=*/
        "sharedStorage.worklet.addModule() has to be called before "
        "sharedStorage.runOperation().");
    return;
  }

  GetAndConnectToSharedStorageWorkletService()->RunOperation(
      name, serialized_data,
      base::BindOnce(&SharedStorageWorkletHost::OnRunOperationOnWorkletFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool SharedStorageWorkletHost::HasPendingOperations() {
  return pending_operations_count_ > 0;
}

void SharedStorageWorkletHost::EnterKeepAliveOnDocumentDestroyed(
    KeepAliveFinishedCallback callback) {
  // At this point the `SharedStorageDocumentServiceImpl` is being destroyed, so
  // `document_service_` is still valid. But it will be auto reset soon after.
  DCHECK(document_service_);
  DCHECK(HasPendingOperations());
  DCHECK(keep_alive_finished_callback_.is_null());

  keep_alive_finished_callback_ = std::move(callback);

  keep_alive_timer_.Start(
      FROM_HERE, GetKeepAliveTimeout(),
      base::BindOnce(&SharedStorageWorkletHost::FinishKeepAlive,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SharedStorageWorkletHost::SharedStorageSet(
    const std::string& key,
    const std::string& value,
    bool ignore_if_present,
    SharedStorageSetCallback callback) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  std::move(callback).Run(
      /*success=*/false,
      /*error_message=*/"sharedStorage.set() is not implemented");
}

void SharedStorageWorkletHost::SharedStorageAppend(
    const std::string& key,
    const std::string& value,
    SharedStorageAppendCallback callback) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  std::move(callback).Run(
      /*success=*/false,
      /*error_message=*/"sharedStorage.append() is not implemented");
}

void SharedStorageWorkletHost::SharedStorageDelete(
    const std::string& key,
    SharedStorageDeleteCallback callback) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  std::move(callback).Run(
      /*success=*/false,
      /*error_message=*/"sharedStorage.delete() is not implemented");
}

void SharedStorageWorkletHost::SharedStorageClear(
    SharedStorageClearCallback callback) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  std::move(callback).Run(
      /*success=*/false,
      /*error_message=*/"sharedStorage.clear() is not implemented");
}

void SharedStorageWorkletHost::SharedStorageGet(
    const std::string& key,
    SharedStorageGetCallback callback) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  std::move(callback).Run(
      /*success=*/false,
      /*error_message=*/"sharedStorage.get() is not implemented", /*value=*/{});
}

void SharedStorageWorkletHost::SharedStorageKeys(
    mojo::PendingRemote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>
        pending_listener) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  mojo::Remote<shared_storage_worklet::mojom::SharedStorageEntriesListener>
      listener(std::move(pending_listener));

  listener->DidReadEntries(
      /*success=*/false,
      /*error_message=*/"sharedStorage.keys() is not implemented",
      /*entries=*/{},
      /*has_more_entries=*/false);
}

void SharedStorageWorkletHost::SharedStorageEntries(
    mojo::PendingRemote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>
        pending_listener) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  mojo::Remote<shared_storage_worklet::mojom::SharedStorageEntriesListener>
      listener(std::move(pending_listener));

  listener->DidReadEntries(
      /*success=*/false,
      /*error_message=*/"sharedStorage.entries() is not implemented",
      /*entries=*/{},
      /*has_more_entries=*/false);
}

void SharedStorageWorkletHost::SharedStorageLength(
    SharedStorageLengthCallback callback) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  std::move(callback).Run(
      /*success=*/false,
      /*error_message=*/"sharedStorage.length() is not implemented",
      /*length=*/0);
}

void SharedStorageWorkletHost::ConsoleLog(const std::string& message) {
  if (!document_service_) {
    DCHECK(IsInKeepAlivePhase());
    return;
  }

  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  devtools_instrumentation::LogWorkletMessage(
      static_cast<RenderFrameHostImpl&>(document_service_->render_frame_host()),
      blink::mojom::ConsoleMessageLevel::kInfo, message);
}

void SharedStorageWorkletHost::OnAddModuleOnWorkletFinished(
    blink::mojom::SharedStorageDocumentService::AddModuleOnWorkletCallback
        callback,
    bool success,
    const std::string& error_message) {
  std::move(callback).Run(success, error_message);

  DecrementPendingOperationsCount();
}

void SharedStorageWorkletHost::OnRunOperationOnWorkletFinished(
    bool success,
    const std::string& error_message) {
  if (!success && document_service_) {
    DCHECK(!IsInKeepAlivePhase());
    devtools_instrumentation::LogWorkletMessage(
        static_cast<RenderFrameHostImpl&>(
            document_service_->render_frame_host()),
        blink::mojom::ConsoleMessageLevel::kError, error_message);
  }

  DecrementPendingOperationsCount();
}

bool SharedStorageWorkletHost::IsInKeepAlivePhase() const {
  return !!keep_alive_finished_callback_;
}

void SharedStorageWorkletHost::FinishKeepAlive() {
  // This will remove this worklet host from the manager.
  std::move(keep_alive_finished_callback_).Run(this);

  // Do not add code after this. SharedStorageWorkletHost has been destroyed.
}

void SharedStorageWorkletHost::IncrementPendingOperationsCount() {
  base::CheckedNumeric<uint32_t> count = pending_operations_count_;
  pending_operations_count_ = (++count).ValueOrDie();
}

void SharedStorageWorkletHost::DecrementPendingOperationsCount() {
  base::CheckedNumeric<uint32_t> count = pending_operations_count_;
  pending_operations_count_ = (--count).ValueOrDie();

  if (!IsInKeepAlivePhase() || pending_operations_count_)
    return;

  FinishKeepAlive();
}

base::TimeDelta SharedStorageWorkletHost::GetKeepAliveTimeout() const {
  return kKeepAliveTimeout;
}

shared_storage_worklet::mojom::SharedStorageWorkletService*
SharedStorageWorkletHost::GetAndConnectToSharedStorageWorkletService() {
  if (!shared_storage_worklet_service_) {
    driver_->StartWorkletService(
        shared_storage_worklet_service_.BindNewPipeAndPassReceiver());

    shared_storage_worklet_service_->BindSharedStorageWorkletServiceClient(
        shared_storage_worklet_service_client_.BindNewEndpointAndPassRemote());
  }

  return shared_storage_worklet_service_.get();
}

}  // namespace content
