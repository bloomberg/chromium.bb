// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/media_transfer_protocol/mtp_device_manager.h"

#include <utility>
#include <vector>

namespace device {

namespace {

class ForwardingMediaTransferProtocolManagerObserver
    : public MediaTransferProtocolManager::Observer {
 public:
  explicit ForwardingMediaTransferProtocolManagerObserver(
      mojom::MtpManagerClientAssociatedPtr client)
      : client_(std::move(client)) {}

  void StorageAttached(
      const device::mojom::MtpStorageInfo& storage_info) override {
    client_->StorageAttached(storage_info.Clone());
  }

  void StorageDetached(const std::string& storage_name) override {
    client_->StorageDetached(storage_name);
  }

 private:
  mojom::MtpManagerClientAssociatedPtr client_;
};

void EnumerateStorageCallbackWrapper(
    mojom::MtpManager::EnumerateStoragesAndSetClientCallback callback,
    std::vector<const mojom::MtpStorageInfo*> storage_info_list) {
  std::vector<mojom::MtpStorageInfoPtr> storage_info_ptr_list(
      storage_info_list.size());
  for (size_t i = 0; i < storage_info_list.size(); ++i)
    storage_info_ptr_list[i] = storage_info_list[i]->Clone();
  std::move(callback).Run(std::move(storage_info_ptr_list));
}

}  // namespace

MtpDeviceManager::MtpDeviceManager() {}

MtpDeviceManager::~MtpDeviceManager() {}

void MtpDeviceManager::AddBinding(mojom::MtpManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

// mojom::MtpManager override.
void MtpDeviceManager::EnumerateStoragesAndSetClient(
    mojom::MtpManagerClientAssociatedPtrInfo client,
    EnumerateStoragesAndSetClientCallback callback) {
  if (!client.is_valid())
    return;

  DCHECK(!observer_);
  mojom::MtpManagerClientAssociatedPtr client_ptr;
  client_ptr.Bind(std::move(client));
  observer_ = std::make_unique<ForwardingMediaTransferProtocolManagerObserver>(
      std::move(client_ptr));
  MediaTransferProtocolManager::GetInstance()->AddObserverAndEnumerateStorages(
      observer_.get(),
      base::BindOnce(EnumerateStorageCallbackWrapper, std::move(callback)));
}

}  // namespace device
