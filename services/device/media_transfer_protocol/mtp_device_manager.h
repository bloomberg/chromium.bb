// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_MEDIA_TRANSFER_PROTOCOL_MTP_DEVICE_MANAGER_H_
#define SERVICES_DEVICE_MEDIA_TRANSFER_PROTOCOL_MTP_DEVICE_MANAGER_H_

#include <memory>
#include <string>

#include "device/media_transfer_protocol/media_transfer_protocol_manager.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"

#if !defined(OS_CHROMEOS)
#error "Only used on ChromeOS"
#endif

namespace device {

// This is the implementation of device::mojom::MtpManager which provides
// various methods to get information of MTP (Media Transfer Protocol) devices.
class MtpDeviceManager : public mojom::MtpManager {
 public:
  MtpDeviceManager();
  ~MtpDeviceManager() override;

  void AddBinding(mojom::MtpManagerRequest request);

  // Implements mojom::MtpManager.
  void EnumerateStoragesAndSetClient(
      mojom::MtpManagerClientAssociatedPtrInfo client,
      EnumerateStoragesAndSetClientCallback callback) override;

 private:
  std::unique_ptr<MediaTransferProtocolManager::Observer> observer_;
  mojo::BindingSet<mojom::MtpManager> bindings_;

  DISALLOW_COPY_AND_ASSIGN(MtpDeviceManager);
};

}  // namespace device
#endif  // SERVICES_DEVICE_MEDIA_TRANSFER_PROTOCOL_MTP_DEVICE_MANAGER_H_
