// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/mojo/device_manager_impl.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"
#include "services/device/usb/mojo/device_impl.h"
#include "services/device/usb/mojo/type_converters.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_service.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#include "services/device/usb/usb_device_linux.h"
#endif  // defined(OS_CHROMEOS)

namespace device {
namespace usb {

DeviceManagerImpl::DeviceManagerImpl()
    : DeviceManagerImpl(UsbService::Create()) {}

DeviceManagerImpl::DeviceManagerImpl(std::unique_ptr<UsbService> usb_service)
    : usb_service_(std::move(usb_service)),
      observer_(this),
      weak_factory_(this) {
  if (usb_service_)
    observer_.Add(usb_service_.get());
}

DeviceManagerImpl::~DeviceManagerImpl() = default;

void DeviceManagerImpl::AddBinding(mojom::UsbDeviceManagerRequest request) {
  if (usb_service_)
    bindings_.AddBinding(this, std::move(request));
}

void DeviceManagerImpl::EnumerateDevicesAndSetClient(
    mojom::UsbDeviceManagerClientAssociatedPtrInfo client,
    EnumerateDevicesAndSetClientCallback callback) {
  usb_service_->GetDevices(base::Bind(
      &DeviceManagerImpl::OnGetDevices, weak_factory_.GetWeakPtr(),
      /*options=*/nullptr, base::Passed(&client), base::Passed(&callback)));
}

void DeviceManagerImpl::GetDevices(mojom::UsbEnumerationOptionsPtr options,
                                   GetDevicesCallback callback) {
  usb_service_->GetDevices(base::Bind(
      &DeviceManagerImpl::OnGetDevices, weak_factory_.GetWeakPtr(),
      base::Passed(&options), /*client=*/nullptr, base::Passed(&callback)));
}

void DeviceManagerImpl::GetDevice(const std::string& guid,
                                  mojom::UsbDeviceRequest device_request,
                                  mojom::UsbDeviceClientPtr device_client) {
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device)
    return;

  DeviceImpl::Create(std::move(device), std::move(device_request),
                     std::move(device_client));
}

#if defined(OS_ANDROID)
void DeviceManagerImpl::RefreshDeviceInfo(const std::string& guid,
                                          RefreshDeviceInfoCallback callback) {
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (device->permission_granted()) {
    std::move(callback).Run(mojom::UsbDeviceInfo::From(*device));
    return;
  }

  device->RequestPermission(
      base::BindOnce(&DeviceManagerImpl::OnPermissionGrantedToRefresh,
                     weak_factory_.GetWeakPtr(), device, std::move(callback)));
}

void DeviceManagerImpl::OnPermissionGrantedToRefresh(
    scoped_refptr<UsbDevice> device,
    RefreshDeviceInfoCallback callback,
    bool granted) {
  DCHECK_EQ(granted, device->permission_granted());
  if (!device->permission_granted()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(mojom::UsbDeviceInfo::From(*device));
}
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
void DeviceManagerImpl::CheckAccess(const std::string& guid,
                                    CheckAccessCallback callback) {
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (device) {
    device->CheckUsbAccess(std::move(callback));
  } else {
    LOG(ERROR) << "Was asked to check access to non-existent USB device: "
               << guid;
    std::move(callback).Run(false);
  }
}

void DeviceManagerImpl::OpenFileDescriptor(
    const std::string& guid,
    OpenFileDescriptorCallback callback) {
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device) {
    LOG(ERROR) << "Was asked to open non-existent USB device: " << guid;
    std::move(callback).Run(base::File());
  } else {
    auto copyable_callback =
        base::AdaptCallbackForRepeating(std::move(callback));
    auto devpath =
        static_cast<device::UsbDeviceLinux*>(device.get())->device_path();
    chromeos::PermissionBrokerClient::Get()->OpenPath(
        devpath,
        base::BindOnce(&DeviceManagerImpl::OnOpenFileDescriptor,
                       weak_factory_.GetWeakPtr(), copyable_callback),
        base::BindOnce(&DeviceManagerImpl::OnOpenFileDescriptorError,
                       weak_factory_.GetWeakPtr(), copyable_callback));
  }
}

void DeviceManagerImpl::OnOpenFileDescriptor(
    OpenFileDescriptorCallback callback,
    base::ScopedFD fd) {
  std::move(callback).Run(base::File(fd.release()));
}

void DeviceManagerImpl::OnOpenFileDescriptorError(
    OpenFileDescriptorCallback callback,
    const std::string& error_name,
    const std::string& message) {
  LOG(ERROR) << "Failed to open USB device file: " << error_name << " "
             << message;
  std::move(callback).Run(base::File());
}
#endif  // defined(OS_CHROMEOS)

void DeviceManagerImpl::SetClient(
    mojom::UsbDeviceManagerClientAssociatedPtrInfo client) {
  DCHECK(client);
  mojom::UsbDeviceManagerClientAssociatedPtr client_ptr;
  client_ptr.Bind(std::move(client));
  clients_.AddPtr(std::move(client_ptr));
}

void DeviceManagerImpl::OnGetDevices(
    mojom::UsbEnumerationOptionsPtr options,
    mojom::UsbDeviceManagerClientAssociatedPtrInfo client,
    GetDevicesCallback callback,
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  std::vector<mojom::UsbDeviceFilterPtr> filters;
  if (options)
    filters.swap(options->filters);

  std::vector<mojom::UsbDeviceInfoPtr> device_infos;
  for (const auto& device : devices) {
    mojom::UsbDeviceInfoPtr device_info = mojom::UsbDeviceInfo::From(*device);
    if (UsbDeviceFilterMatchesAny(filters, *device_info)) {
      device_infos.push_back(std::move(device_info));
    }
  }

  std::move(callback).Run(std::move(device_infos));

  if (client)
    SetClient(std::move(client));
}

void DeviceManagerImpl::OnDeviceAdded(scoped_refptr<UsbDevice> device) {
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  DCHECK(device_info);
  clients_.ForAllPtrs([&device_info](mojom::UsbDeviceManagerClient* client) {
    client->OnDeviceAdded(device_info->Clone());
  });
}

void DeviceManagerImpl::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  DCHECK(device_info);
  clients_.ForAllPtrs([&device_info](mojom::UsbDeviceManagerClient* client) {
    client->OnDeviceRemoved(device_info->Clone());
  });
}

void DeviceManagerImpl::WillDestroyUsbService() {
  observer_.RemoveAll();
  usb_service_ = nullptr;

  // Close all the connections.
  bindings_.CloseAllBindings();
  clients_.CloseAll();
}

}  // namespace usb
}  // namespace device
