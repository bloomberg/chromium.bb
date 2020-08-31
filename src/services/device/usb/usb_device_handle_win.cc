// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_handle_win.h"

#include <windows.h>  // Must be in front of other Windows header files.

#include <usbioctl.h>
#include <usbspec.h>
#include <winioctl.h>
#include <winusb.h>

#include <algorithm>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/object_watcher.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/usb/usb_context.h"
#include "services/device/usb/usb_descriptors.h"
#include "services/device/usb/usb_device_win.h"
#include "services/device/usb/usb_service.h"

namespace device {

using mojom::UsbControlTransferRecipient;
using mojom::UsbControlTransferType;
using mojom::UsbTransferDirection;
using mojom::UsbTransferStatus;

namespace {

const base::StringPiece16 kWinUsbDriverName = L"winusb";

uint8_t BuildRequestFlags(UsbTransferDirection direction,
                          UsbControlTransferType request_type,
                          UsbControlTransferRecipient recipient) {
  uint8_t flags = 0;

  switch (direction) {
    case UsbTransferDirection::OUTBOUND:
      flags |= BMREQUEST_HOST_TO_DEVICE << 7;
      break;
    case UsbTransferDirection::INBOUND:
      flags |= BMREQUEST_DEVICE_TO_HOST << 7;
      break;
  }

  switch (request_type) {
    case UsbControlTransferType::STANDARD:
      flags |= BMREQUEST_STANDARD << 5;
      break;
    case UsbControlTransferType::CLASS:
      flags |= BMREQUEST_CLASS << 5;
      break;
    case UsbControlTransferType::VENDOR:
      flags |= BMREQUEST_VENDOR << 5;
      break;
    case UsbControlTransferType::RESERVED:
      flags |= 4 << 5;  // Not defined by usbspec.h.
      break;
  }

  switch (recipient) {
    case UsbControlTransferRecipient::DEVICE:
      flags |= BMREQUEST_TO_DEVICE;
      break;
    case UsbControlTransferRecipient::INTERFACE:
      flags |= BMREQUEST_TO_INTERFACE;
      break;
    case UsbControlTransferRecipient::ENDPOINT:
      flags |= BMREQUEST_TO_ENDPOINT;
      break;
    case UsbControlTransferRecipient::OTHER:
      flags |= BMREQUEST_TO_OTHER;
      break;
  }

  return flags;
}

bool ResetPipeBlocking(WINUSB_INTERFACE_HANDLE handle, UCHAR pipeId) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  if (!WinUsb_ResetPipe(handle, pipeId)) {
    USB_PLOG(DEBUG) << "Failed to reset pipe " << static_cast<int>(pipeId);
    return false;
  }

  return true;
}

bool SetCurrentAlternateSettingBlocking(WINUSB_INTERFACE_HANDLE handle,
                                        UCHAR settingNumber) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);
  if (!WinUsb_SetCurrentAlternateSetting(handle, settingNumber)) {
    USB_PLOG(DEBUG) << "Failed to set alternate setting "
                    << static_cast<int>(settingNumber);
    return false;
  }

  return true;
}

}  // namespace

// Encapsulates waiting for the completion of an overlapped event.
class UsbDeviceHandleWin::Request : public base::win::ObjectWatcher::Delegate {
 public:
  Request(HANDLE handle, int interface_number)
      : handle_(handle),
        interface_number_(interface_number),
        event_(CreateEvent(nullptr, false, false, nullptr)) {
    memset(&overlapped_, 0, sizeof(overlapped_));
    overlapped_.hEvent = event_.Get();
  }

  ~Request() override = default;

  // Starts watching for completion of the overlapped event.
  void MaybeStartWatching(
      BOOL success,
      DWORD last_error,
      base::OnceCallback<void(Request*, DWORD, size_t)> callback) {
    callback_ = std::move(callback);
    if (success) {
      OnObjectSignaled(event_.Get());
    } else {
      if (last_error == ERROR_IO_PENDING)
        watcher_.StartWatchingOnce(event_.Get(), this);
      else
        std::move(callback_).Run(this, last_error, 0);
    }
  }

  void Abort() {
    watcher_.StopWatching();
    std::move(callback_).Run(this, ERROR_REQUEST_ABORTED, 0);
  }

  OVERLAPPED* overlapped() { return &overlapped_; }
  int interface_number() const { return interface_number_; }

  // base::win::ObjectWatcher::Delegate
  void OnObjectSignaled(HANDLE object) override {
    DCHECK_EQ(object, event_.Get());
    DWORD size;
    BOOL result;
    if (interface_number_ == -1)
      result = GetOverlappedResult(handle_, &overlapped_, &size, true);
    else
      result = WinUsb_GetOverlappedResult(handle_, &overlapped_, &size, true);
    DWORD last_error = GetLastError();

    if (result)
      std::move(callback_).Run(this, ERROR_SUCCESS, size);
    else
      std::move(callback_).Run(this, last_error, 0);
  }

 private:
  HANDLE handle_;
  // If -1 then |handle_| is a HANDLE and not a
  // WINUSB_INTERFACE_HANDLE.
  int interface_number_;
  OVERLAPPED overlapped_;
  base::win::ScopedHandle event_;
  base::win::ObjectWatcher watcher_;
  base::OnceCallback<void(Request*, DWORD, size_t)> callback_;

  DISALLOW_COPY_AND_ASSIGN(Request);
};

UsbDeviceHandleWin::Interface::Interface() = default;

UsbDeviceHandleWin::Interface::~Interface() = default;

scoped_refptr<UsbDevice> UsbDeviceHandleWin::GetDevice() const {
  return device_;
}

void UsbDeviceHandleWin::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_)
    return;

  device_->HandleClosed(this);
  device_ = nullptr;

  if (hub_handle_.IsValid()) {
    CancelIo(hub_handle_.Get());
    hub_handle_.Close();
  }

  for (auto& map_entry : interfaces_) {
    Interface* interface = &map_entry.second;
    if (interface->function_handle.IsValid())
      CancelIo(interface->function_handle.Get());

    if (interface->claimed) {
      interface->claimed = false;
      ReleaseInterfaceReference(interface);
    }
  }

  // Aborting requests may run or destroy callbacks holding the last reference
  // to this object so hold a reference for the rest of this method.
  scoped_refptr<UsbDeviceHandleWin> self(this);
  while (!requests_.empty())
    requests_.begin()->second->Abort();
}

void UsbDeviceHandleWin::SetConfiguration(int configuration_value,
                                          ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Setting device configuration is not supported on Windows.
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), false));
}

void UsbDeviceHandleWin::ClaimInterface(int interface_number,
                                        ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  auto interface_it = interfaces_.find(interface_number);
  if (interface_it == interfaces_.end()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }
  Interface* interface = &interface_it->second;

  OpenInterfaceHandle(
      interface,
      base::BindOnce(&UsbDeviceHandleWin::OnInterfaceClaimed,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void UsbDeviceHandleWin::ReleaseInterface(int interface_number,
                                          ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  auto interface_it = interfaces_.find(interface_number);
  if (interface_it == interfaces_.end()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }
  Interface* interface = &interface_it->second;

  if (!interface->claimed) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  interface->claimed = false;
  ReleaseInterfaceReference(interface);

  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), true));
}

void UsbDeviceHandleWin::SetInterfaceAlternateSetting(int interface_number,
                                                      int alternate_setting,
                                                      ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  auto interface_it = interfaces_.find(interface_number);
  if (interface_it == interfaces_.end()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }
  Interface& interface = interface_it->second;

  if (!interface.claimed) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  bool found_alternate = false;
  for (const auto& alternate : interface.info->alternates) {
    if (alternate->alternate_setting == alternate_setting) {
      found_alternate = true;
      break;
    }
  }

  if (!found_alternate) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // Prevent |interface.handle| from being released while the blocking call
  // is in progress.
  DCHECK(interface.handle.IsValid());
  interface.reference_count++;

  // Use a strong reference to |this| rather than a weak pointer to prevent
  // |interface.handle| from being freed because |this| was destroyed.
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&SetCurrentAlternateSettingBlocking,
                     interface.handle.Get(), alternate_setting),
      base::BindOnce(&UsbDeviceHandleWin::OnSetAlternateInterfaceSetting, this,
                     interface_number, alternate_setting, std::move(callback)));
}

void UsbDeviceHandleWin::ResetDevice(ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Resetting the device is not supported on Windows.
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), false));
}

void UsbDeviceHandleWin::ClearHalt(mojom::UsbTransferDirection direction,
                                   uint8_t endpoint_number,
                                   ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  uint8_t endpoint_address =
      ConvertEndpointNumberToAddress(endpoint_number, direction);

  auto endpoint_it = endpoints_.find(endpoint_address);
  if (endpoint_it == endpoints_.end()) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  auto interface_it =
      interfaces_.find(endpoint_it->second.interface->interface_number);
  DCHECK(interface_it != interfaces_.end());
  Interface& interface = interface_it->second;
  if (!interface.claimed) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), false));
    return;
  }

  // Prevent |interface.handle| from being released while the blocking call
  // is in progress.
  DCHECK(interface.handle.IsValid());
  interface.reference_count++;

  // Use a strong reference to |this| rather than a weak pointer to prevent
  // |interface.handle| from being freed because |this| was destroyed.
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&ResetPipeBlocking, interface.handle.Get(),
                     endpoint_address),
      base::BindOnce(&UsbDeviceHandleWin::OnClearHalt, this,
                     interface.info->interface_number, std::move(callback)));
}

void UsbDeviceHandleWin::ControlTransfer(
    UsbTransferDirection direction,
    UsbControlTransferType request_type,
    UsbControlTransferRecipient recipient,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!device_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  UsbTransferStatus::DISCONNECT, nullptr, 0));
    return;
  }

  if (hub_handle_.IsValid()) {
    if (direction == UsbTransferDirection::INBOUND &&
        request_type == UsbControlTransferType::STANDARD &&
        recipient == UsbControlTransferRecipient::DEVICE &&
        request == USB_REQUEST_GET_DESCRIPTOR) {
      if ((value >> 8) == USB_DEVICE_DESCRIPTOR_TYPE) {
        auto* node_connection_info = new USB_NODE_CONNECTION_INFORMATION_EX;
        node_connection_info->ConnectionIndex = device_->port_number();

        Request* request = MakeRequest(/*interface=*/nullptr);
        BOOL result = DeviceIoControl(
            hub_handle_.Get(), IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
            node_connection_info, sizeof(*node_connection_info),
            node_connection_info, sizeof(*node_connection_info), nullptr,
            request->overlapped());
        DWORD last_error = GetLastError();
        request->MaybeStartWatching(
            result, last_error,
            base::BindOnce(&UsbDeviceHandleWin::GotNodeConnectionInformation,
                           weak_factory_.GetWeakPtr(), std::move(callback),
                           base::Owned(node_connection_info), buffer));
        return;
      } else if (((value >> 8) == USB_CONFIGURATION_DESCRIPTOR_TYPE) ||
                 ((value >> 8) == USB_STRING_DESCRIPTOR_TYPE) ||
                 ((value >> 8) == USB_BOS_DESCRIPTOR_TYPE)) {
        size_t size = sizeof(USB_DESCRIPTOR_REQUEST) + buffer->size();
        auto request_buffer = base::MakeRefCounted<base::RefCountedBytes>(size);
        USB_DESCRIPTOR_REQUEST* descriptor_request =
            request_buffer->front_as<USB_DESCRIPTOR_REQUEST>();
        descriptor_request->ConnectionIndex = device_->port_number();
        descriptor_request->SetupPacket.bmRequest = BMREQUEST_DEVICE_TO_HOST;
        descriptor_request->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
        descriptor_request->SetupPacket.wValue = value;
        descriptor_request->SetupPacket.wIndex = index;
        descriptor_request->SetupPacket.wLength = buffer->size();

        Request* request = MakeRequest(/*interface=*/nullptr);
        BOOL result = DeviceIoControl(
            hub_handle_.Get(), IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
            request_buffer->front(), size, request_buffer->front(), size,
            nullptr, request->overlapped());
        DWORD last_error = GetLastError();
        request->MaybeStartWatching(
            result, last_error,
            base::BindOnce(&UsbDeviceHandleWin::GotDescriptorFromNodeConnection,
                           weak_factory_.GetWeakPtr(), std::move(callback),
                           request_buffer, buffer));
        return;
      }
    }

    // Unsupported transfer for hub.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       nullptr, 0));
    return;
  }

  // Submit a normal control transfer.
  OpenInterfaceForControlTransfer(
      recipient, index,
      base::BindOnce(&UsbDeviceHandleWin::OnInterfaceOpenedForControlTransfer,
                     weak_factory_.GetWeakPtr(), direction, request_type,
                     recipient, request, value, index, std::move(buffer),
                     timeout, std::move(callback)));
}

void UsbDeviceHandleWin::IsochronousTransferIn(
    uint8_t endpoint_number,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Isochronous is not yet supported on Windows.
  ReportIsochronousError(packet_lengths, std::move(callback),
                         UsbTransferStatus::TRANSFER_ERROR);
}

void UsbDeviceHandleWin::IsochronousTransferOut(
    uint8_t endpoint_number,
    scoped_refptr<base::RefCountedBytes> buffer,
    const std::vector<uint32_t>& packet_lengths,
    unsigned int timeout,
    IsochronousTransferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Isochronous is not yet supported on Windows.
  ReportIsochronousError(packet_lengths, std::move(callback),
                         UsbTransferStatus::TRANSFER_ERROR);
}

void UsbDeviceHandleWin::GenericTransfer(
    UsbTransferDirection direction,
    uint8_t endpoint_number,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  uint8_t endpoint_address =
      ConvertEndpointNumberToAddress(endpoint_number, direction);

  auto endpoint_it = endpoints_.find(endpoint_address);
  if (endpoint_it == endpoints_.end()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       nullptr, 0));
    return;
  }

  auto interface_it =
      interfaces_.find(endpoint_it->second.interface->interface_number);
  DCHECK(interface_it != interfaces_.end());
  Interface* interface = &interface_it->second;
  if (!interface->claimed) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       nullptr, 0));
    return;
  }

  DCHECK(interface->handle.IsValid());
  Request* request = MakeRequest(interface);
  BOOL result;
  if (direction == UsbTransferDirection::INBOUND) {
    result = WinUsb_ReadPipe(interface->handle.Get(), endpoint_address,
                             buffer->front(), buffer->size(), nullptr,
                             request->overlapped());
  } else {
    result = WinUsb_WritePipe(interface->handle.Get(), endpoint_address,
                              buffer->front(), buffer->size(), nullptr,
                              request->overlapped());
  }
  DWORD last_error = GetLastError();
  request->MaybeStartWatching(
      result, last_error,
      base::BindOnce(&UsbDeviceHandleWin::TransferComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(buffer)));
}

const mojom::UsbInterfaceInfo* UsbDeviceHandleWin::FindInterfaceByEndpoint(
    uint8_t endpoint_address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = endpoints_.find(endpoint_address);
  if (it != endpoints_.end())
    return it->second.interface;
  return nullptr;
}

UsbDeviceHandleWin::UsbDeviceHandleWin(scoped_refptr<UsbDeviceWin> device)
    : device_(std::move(device)),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      blocking_task_runner_(UsbService::CreateBlockingTaskRunner()) {
  // Windows only supports configuration 1, which therefore must be active.
  DCHECK(device_->GetActiveConfiguration());

  for (const auto& interface : device_->GetActiveConfiguration()->interfaces) {
    for (const auto& alternate : interface->alternates) {
      if (alternate->alternate_setting != 0)
        continue;

      Interface& interface_info = interfaces_[interface->interface_number];
      interface_info.info = interface.get();
      RegisterEndpoints(interface.get(), *alternate);

      if (interface->interface_number == interface->first_interface) {
        auto it = device_->functions().find(interface->interface_number);
        if (it != device_->functions().end()) {
          interface_info.function_driver = it->second.driver;
          interface_info.function_path = it->second.path;
        }
      }
    }
  }
}

UsbDeviceHandleWin::UsbDeviceHandleWin(scoped_refptr<UsbDeviceWin> device,
                                       base::win::ScopedHandle handle)
    : device_(std::move(device)),
      hub_handle_(std::move(handle)),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      blocking_task_runner_(UsbService::CreateBlockingTaskRunner()) {}

UsbDeviceHandleWin::~UsbDeviceHandleWin() {
#if DCHECK_IS_ON()
  DCHECK(!hub_handle_.IsValid());
  for (auto& map_entry : interfaces_) {
    const Interface& interface = map_entry.second;
    DCHECK_EQ(interface.reference_count, 0);
    DCHECK(!interface.handle.IsValid());
    DCHECK(!interface.function_handle.IsValid());
  }
#endif
}

void UsbDeviceHandleWin::UpdateFunction(int interface_number,
                                        const base::string16& function_driver,
                                        const base::string16& function_path) {
  auto it = interfaces_.find(interface_number);
  if (it == interfaces_.end())
    return;
  Interface* interface = &it->second;

  interface->function_driver = function_driver;
  interface->function_path = function_path;

  // Move callback lists onto the stack to avoid re-entrancy concerns.
  auto callbacks = std::move(interface->ready_callbacks);

  if (base::EqualsCaseInsensitiveASCII(function_driver, kWinUsbDriverName) &&
      !function_path.empty()) {
    // This path can also be used for requests for endpoint 0.
    callbacks.insert(callbacks.end(),
                     std::make_move_iterator(ep0_ready_callbacks_.begin()),
                     std::make_move_iterator(ep0_ready_callbacks_.end()));
    ep0_ready_callbacks_.clear();
  } else if (!ep0_ready_callbacks_.empty()) {
    // If all functions have been enumerated without finding one with the
    // WinUSB driver. Give up waiting and report an error.
    if (AllFunctionsEnumerated()) {
      callbacks.insert(callbacks.end(),
                       std::make_move_iterator(ep0_ready_callbacks_.begin()),
                       std::make_move_iterator(ep0_ready_callbacks_.end()));
      ep0_ready_callbacks_.clear();
    }
  }

  for (auto& callback : callbacks)
    std::move(callback).Run(interface);
}

void UsbDeviceHandleWin::OpenInterfaceHandle(Interface* interface,
                                             OpenInterfaceCallback callback) {
  if (interface->handle.IsValid()) {
    std::move(callback).Run(interface);
    return;
  }

  if (interface->info->first_interface != interface->info->interface_number) {
    auto first_interface_it =
        interfaces_.find(interface->info->first_interface);
    DCHECK(first_interface_it != interfaces_.end());
    Interface* first_interface = &first_interface_it->second;

    OpenInterfaceHandle(
        first_interface,
        base::BindOnce(&UsbDeviceHandleWin::OnFirstInterfaceOpened,
                       weak_factory_.GetWeakPtr(),
                       interface->info->interface_number, std::move(callback)));
    return;
  }

  if (interface->function_driver.empty()) {
    interface->ready_callbacks.push_back(
        base::BindOnce(&UsbDeviceHandleWin::OnFunctionAvailable,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  OnFunctionAvailable(std::move(callback), interface);
}

void UsbDeviceHandleWin::OnFunctionAvailable(OpenInterfaceCallback callback,
                                             Interface* interface) {
  base::ScopedClosureRunner run_callback(
      base::BindOnce(std::move(callback), interface));

  if (interface->handle.IsValid())
    return;

  if (!base::EqualsCaseInsensitiveASCII(interface->function_driver,
                                        kWinUsbDriverName)) {
    USB_LOG(ERROR) << "Interface "
                   << static_cast<int>(interface->info->interface_number)
                   << " uses driver \"" << interface->function_driver
                   << "\" instead of WinUSB.";
    return;
  }

  if (interface->function_path.empty()) {
    USB_LOG(ERROR) << "Interface "
                   << static_cast<int>(interface->info->interface_number)
                   << " has no device path.";
    return;
  }

  DCHECK(!interface->function_handle.IsValid());
  interface->function_handle.Set(CreateFile(
      interface->function_path.c_str(), GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE, /*lpSecurityAttributes=*/nullptr,
      OPEN_EXISTING, FILE_FLAG_OVERLAPPED, /*hTemplateFile=*/nullptr));
  if (!interface->function_handle.IsValid()) {
    USB_PLOG(ERROR) << "Failed to open " << interface->function_path;
    return;
  }

  WINUSB_INTERFACE_HANDLE handle;
  if (!WinUsb_Initialize(interface->function_handle.Get(), &handle)) {
    USB_PLOG(ERROR) << "Failed to initialize WinUSB handle";
    return;
  }
  interface->handle.Set(handle);
}

void UsbDeviceHandleWin::OnFirstInterfaceOpened(int interface_number,
                                                OpenInterfaceCallback callback,
                                                Interface* first_interface) {
  auto interface_it = interfaces_.find(interface_number);
  DCHECK(interface_it != interfaces_.end());
  Interface* interface = &interface_it->second;
  DCHECK_NE(interface->info->first_interface,
            interface->info->interface_number);
  DCHECK_EQ(interface->info->first_interface,
            first_interface->info->interface_number);

  base::ScopedClosureRunner run_callback(
      base::BindOnce(std::move(callback), interface));

  if (!first_interface->handle.IsValid())
    return;

  first_interface->reference_count++;

  int index =
      interface->info->interface_number - interface->info->first_interface - 1;
  WINUSB_INTERFACE_HANDLE handle;
  if (WinUsb_GetAssociatedInterface(first_interface->handle.Get(), index,
                                    &handle)) {
    interface->handle.Set(handle);
  } else {
    USB_PLOG(ERROR) << "Failed to get associated interface " << index
                    << " from interface "
                    << static_cast<int>(interface->info->first_interface);
    ReleaseInterfaceReference(first_interface);
  }
}

void UsbDeviceHandleWin::OnInterfaceClaimed(ResultCallback callback,
                                            Interface* interface) {
  if (interface->handle.IsValid()) {
    interface->claimed = true;
    interface->reference_count++;
  }

  std::move(callback).Run(interface->claimed);
}

void UsbDeviceHandleWin::OnSetAlternateInterfaceSetting(int interface_number,
                                                        int alternate_setting,
                                                        ResultCallback callback,
                                                        bool result) {
  auto it = interfaces_.find(interface_number);
  DCHECK(it != interfaces_.end());
  Interface& interface = it->second;

  if (!result) {
    ReleaseInterfaceReference(&interface);
    std::move(callback).Run(false);
    return;
  }

  // Unregister endpoints from the previously selected alternate setting.
  for (const auto& alternate : interface.info->alternates) {
    if (alternate->alternate_setting == interface.alternate_setting) {
      UnregisterEndpoints(*alternate);
      break;
    }
  }

  interface.alternate_setting = alternate_setting;

  // Unregister endpoints from the currently selected alternate setting.
  for (const auto& alternate : interface.info->alternates) {
    if (alternate->alternate_setting == interface.alternate_setting) {
      RegisterEndpoints(interface.info, *alternate);
      break;
    }
  }

  ReleaseInterfaceReference(&interface);
  std::move(callback).Run(true);
}

void UsbDeviceHandleWin::RegisterEndpoints(
    const mojom::UsbInterfaceInfo* interface,
    const mojom::UsbAlternateInterfaceInfo& alternate) {
  for (const auto& endpoint : alternate.endpoints) {
    Endpoint& endpoint_info =
        endpoints_[ConvertEndpointNumberToAddress(*endpoint)];
    endpoint_info.interface = interface;
    endpoint_info.type = endpoint->type;
  }
}

void UsbDeviceHandleWin::UnregisterEndpoints(
    const mojom::UsbAlternateInterfaceInfo& alternate) {
  for (const auto& endpoint : alternate.endpoints)
    endpoints_.erase(ConvertEndpointNumberToAddress(*endpoint));
}

void UsbDeviceHandleWin::OnClearHalt(int interface_number,
                                     ResultCallback callback,
                                     bool result) {
  auto it = interfaces_.find(interface_number);
  DCHECK(it != interfaces_.end());
  ReleaseInterfaceReference(&it->second);

  std::move(callback).Run(result);
}

void UsbDeviceHandleWin::OpenInterfaceForControlTransfer(
    UsbControlTransferRecipient recipient,
    uint16_t index,
    OpenInterfaceCallback callback) {
  switch (recipient) {
    case UsbControlTransferRecipient::DEVICE:
    case UsbControlTransferRecipient::OTHER: {
      // For control transfers targeting the whole device any interface with
      // the WinUSB driver loaded can be used.
      for (auto& map_entry : interfaces_) {
        const auto& interface = map_entry.second;
        if (base::EqualsCaseInsensitiveASCII(interface.function_driver,
                                             kWinUsbDriverName) &&
            !interface.function_path.empty()) {
          OpenInterfaceHandle(&map_entry.second, std::move(callback));
          return;
        }
      }

      if (AllFunctionsEnumerated()) {
        std::move(callback).Run(/*interface=*/nullptr);
        return;
      }

      ep0_ready_callbacks_.push_back(
          base::BindOnce(&UsbDeviceHandleWin::OnFunctionAvailableForEp0,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
      break;
    }

    case UsbControlTransferRecipient::ENDPOINT: {
      // By convention the lower bits of the wIndex field indicate the target
      // endpoint.
      auto endpoint_it = endpoints_.find(index & 0xff);
      if (endpoint_it == endpoints_.end()) {
        std::move(callback).Run(/*interface=*/nullptr);
        return;
      }

      index = endpoint_it->second.interface->interface_number;
      FALLTHROUGH;
    }

    case UsbControlTransferRecipient::INTERFACE: {
      // By convention the lower bits of the wIndex field indicate the target
      // interface.
      auto interface_it = interfaces_.find(index & 0xff);
      if (interface_it == interfaces_.end()) {
        std::move(callback).Run(/*interface=*/nullptr);
        return;
      }

      OpenInterfaceHandle(&interface_it->second, std::move(callback));
      break;
    }
  }
}

void UsbDeviceHandleWin::OnFunctionAvailableForEp0(
    OpenInterfaceCallback callback,
    Interface* interface) {
  OpenInterfaceHandle(interface, std::move(callback));
}

void UsbDeviceHandleWin::OnInterfaceOpenedForControlTransfer(
    UsbTransferDirection direction,
    UsbControlTransferType request_type,
    UsbControlTransferRecipient recipient,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    scoped_refptr<base::RefCountedBytes> buffer,
    unsigned int timeout,
    TransferCallback callback,
    Interface* interface) {
  if (!interface || interface->function_path.empty()) {
    USB_LOG(ERROR) << "Interface handle not available for control transfer.";
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       /*buffer=*/nullptr, /*size=*/0));
    return;
  }

  if (!interface->handle.IsValid()) {
    // OpenInterfaceHandle() already logged an error.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UsbTransferStatus::TRANSFER_ERROR,
                       /*buffer=*/nullptr, /*size=*/0));
    return;
  }

  WINUSB_SETUP_PACKET setup = {0};
  setup.RequestType = BuildRequestFlags(direction, request_type, recipient);
  setup.Request = request;
  setup.Value = value;
  setup.Index = index;
  setup.Length = buffer->size();

  Request* control_request = MakeRequest(interface);
  BOOL result = WinUsb_ControlTransfer(
      interface->handle.Get(), setup, buffer->front(), buffer->size(),
      /*LengthTransferred=*/nullptr, control_request->overlapped());
  DWORD last_error = GetLastError();
  control_request->MaybeStartWatching(
      result, last_error,
      base::BindOnce(&UsbDeviceHandleWin::TransferComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback), buffer));
}

UsbDeviceHandleWin::Request* UsbDeviceHandleWin::MakeRequest(
    Interface* interface) {
  // The HANDLE used to get the overlapped result must be the
  // WINUSB_INTERFACE_HANDLE of the first interface in the function.
  //
  // https://docs.microsoft.com/en-us/windows/win32/api/winusb/nf-winusb-winusb_getoverlappedresult
  HANDLE handle;
  if (!interface) {
    handle = hub_handle_.Get();
  } else {
    if (interface->info->interface_number != interface->info->first_interface) {
      auto it = interfaces_.find(interface->info->first_interface);
      DCHECK(it != interfaces_.end());
      interface = &it->second;
    }

    handle = interface->handle.Get();
    interface->reference_count++;
  }

  auto request = std::make_unique<Request>(
      handle, interface ? interface->info->interface_number : -1);
  Request* request_ptr = request.get();
  requests_[request_ptr] = std::move(request);
  return request_ptr;
}

std::unique_ptr<UsbDeviceHandleWin::Request> UsbDeviceHandleWin::UnlinkRequest(
    UsbDeviceHandleWin::Request* request_ptr) {
  auto it = requests_.find(request_ptr);
  DCHECK(it != requests_.end());
  std::unique_ptr<Request> request = std::move(it->second);
  requests_.erase(it);
  return request;
}

void UsbDeviceHandleWin::GotNodeConnectionInformation(
    TransferCallback callback,
    void* node_connection_info_ptr,
    scoped_refptr<base::RefCountedBytes> buffer,
    Request* request_ptr,
    DWORD win32_result,
    size_t bytes_transferred) {
  USB_NODE_CONNECTION_INFORMATION_EX* node_connection_info =
      static_cast<USB_NODE_CONNECTION_INFORMATION_EX*>(
          node_connection_info_ptr);
  std::unique_ptr<Request> request = UnlinkRequest(request_ptr);

  if (win32_result != ERROR_SUCCESS) {
    SetLastError(win32_result);
    USB_PLOG(ERROR) << "Failed to get node connection information";
    std::move(callback).Run(UsbTransferStatus::TRANSFER_ERROR, nullptr, 0);
    return;
  }

  DCHECK_EQ(bytes_transferred, sizeof(USB_NODE_CONNECTION_INFORMATION_EX));
  bytes_transferred = std::min(sizeof(USB_DEVICE_DESCRIPTOR), buffer->size());
  memcpy(buffer->front(), &node_connection_info->DeviceDescriptor,
         bytes_transferred);
  std::move(callback).Run(UsbTransferStatus::COMPLETED, buffer,
                          bytes_transferred);
}

void UsbDeviceHandleWin::GotDescriptorFromNodeConnection(
    TransferCallback callback,
    scoped_refptr<base::RefCountedBytes> request_buffer,
    scoped_refptr<base::RefCountedBytes> original_buffer,
    Request* request_ptr,
    DWORD win32_result,
    size_t bytes_transferred) {
  std::unique_ptr<Request> request = UnlinkRequest(request_ptr);

  if (win32_result != ERROR_SUCCESS) {
    SetLastError(win32_result);
    USB_PLOG(ERROR) << "Failed to read descriptor from node connection";
    std::move(callback).Run(UsbTransferStatus::TRANSFER_ERROR, nullptr, 0);
    return;
  }

  DCHECK_GE(bytes_transferred, sizeof(USB_DESCRIPTOR_REQUEST));
  bytes_transferred -= sizeof(USB_DESCRIPTOR_REQUEST);
  DCHECK_LE(bytes_transferred, original_buffer->size());
  memcpy(original_buffer->front(),
         request_buffer->front() + sizeof(USB_DESCRIPTOR_REQUEST),
         bytes_transferred);
  std::move(callback).Run(UsbTransferStatus::COMPLETED, original_buffer,
                          bytes_transferred);
}

void UsbDeviceHandleWin::TransferComplete(
    TransferCallback callback,
    scoped_refptr<base::RefCountedBytes> buffer,
    Request* request_ptr,
    DWORD win32_result,
    size_t bytes_transferred) {
  std::unique_ptr<Request> request = UnlinkRequest(request_ptr);
  UsbTransferStatus status = UsbTransferStatus::COMPLETED;

  if (win32_result != ERROR_SUCCESS) {
    SetLastError(win32_result);
    USB_PLOG(ERROR) << "Transfer failed";

    buffer = nullptr;
    bytes_transferred = 0;
    status = UsbTransferStatus::TRANSFER_ERROR;
  }

  DCHECK_NE(request->interface_number(), -1);
  auto it = interfaces_.find(request->interface_number());
  DCHECK(it != interfaces_.end());
  ReleaseInterfaceReference(&it->second);

  std::move(callback).Run(status, std::move(buffer), bytes_transferred);
}

void UsbDeviceHandleWin::ReportIsochronousError(
    const std::vector<uint32_t>& packet_lengths,
    IsochronousTransferCallback callback,
    UsbTransferStatus status) {
  std::vector<mojom::UsbIsochronousPacketPtr> packets(packet_lengths.size());
  for (size_t i = 0; i < packet_lengths.size(); ++i) {
    packets[i] = mojom::UsbIsochronousPacket::New();
    packets[i]->length = packet_lengths[i];
    packets[i]->transferred_length = 0;
    packets[i]->status = status;
  }
  task_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(callback), nullptr,
                                                   std::move(packets)));
}

bool UsbDeviceHandleWin::AllFunctionsEnumerated() const {
  for (const auto& map_entry : interfaces_) {
    const Interface& interface = map_entry.second;

    // Iterate over functions, rather than interfaces.
    if (interface.info->first_interface != interface.info->interface_number)
      continue;

    if (interface.function_driver.empty())
      return false;
  }

  return true;
}

void UsbDeviceHandleWin::ReleaseInterfaceReference(Interface* interface) {
  DCHECK_GT(interface->reference_count, 0);
  interface->reference_count--;
  if (interface->reference_count > 0)
    return;

  if (interface->handle.IsValid()) {
    interface->handle.Close();
    interface->alternate_setting = 0;
  }

  if (interface->function_handle.IsValid())
    interface->function_handle.Close();

  if (interface->info->interface_number != interface->info->first_interface) {
    auto it = interfaces_.find(interface->info->first_interface);
    DCHECK(it != interfaces_.end());
    ReleaseInterfaceReference(&it->second);
  }
}

}  // namespace device
