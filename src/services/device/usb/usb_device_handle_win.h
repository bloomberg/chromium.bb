// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_WIN_H_
#define SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_WIN_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/win/scoped_handle.h"
#include "services/device/usb/scoped_winusb_handle.h"
#include "services/device/usb/usb_device_handle.h"

namespace base {
class RefCountedBytes;
class SequencedTaskRunner;
}  // namespace base

namespace device {

class UsbDeviceWin;

// UsbDeviceHandle class provides basic I/O related functionalities.
class UsbDeviceHandleWin : public UsbDeviceHandle {
 public:
  scoped_refptr<UsbDevice> GetDevice() const override;
  void Close() override;
  void SetConfiguration(int configuration_value,
                        ResultCallback callback) override;
  void ClaimInterface(int interface_number, ResultCallback callback) override;
  void ReleaseInterface(int interface_number, ResultCallback callback) override;
  void SetInterfaceAlternateSetting(int interface_number,
                                    int alternate_setting,
                                    ResultCallback callback) override;
  void ResetDevice(ResultCallback callback) override;
  void ClearHalt(mojom::UsbTransferDirection direction,
                 uint8_t endpoint_number,
                 ResultCallback callback) override;

  void ControlTransfer(mojom::UsbTransferDirection direction,
                       mojom::UsbControlTransferType request_type,
                       mojom::UsbControlTransferRecipient recipient,
                       uint8_t request,
                       uint16_t value,
                       uint16_t index,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       TransferCallback callback) override;

  void IsochronousTransferIn(uint8_t endpoint,
                             const std::vector<uint32_t>& packet_lengths,
                             unsigned int timeout,
                             IsochronousTransferCallback callback) override;

  void IsochronousTransferOut(uint8_t endpoint,
                              scoped_refptr<base::RefCountedBytes> buffer,
                              const std::vector<uint32_t>& packet_lengths,
                              unsigned int timeout,
                              IsochronousTransferCallback callback) override;

  void GenericTransfer(mojom::UsbTransferDirection direction,
                       uint8_t endpoint_number,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       unsigned int timeout,
                       TransferCallback callback) override;
  const mojom::UsbInterfaceInfo* FindInterfaceByEndpoint(
      uint8_t endpoint_address) override;

 protected:
  friend class UsbDeviceWin;

  // Constructor used to build a connection to the device.
  UsbDeviceHandleWin(scoped_refptr<UsbDeviceWin> device);

  // Constructor used to build a connection to the device's parent hub.
  UsbDeviceHandleWin(scoped_refptr<UsbDeviceWin> device,
                     base::win::ScopedHandle handle);

  ~UsbDeviceHandleWin() override;

  void UpdateFunction(int interface_number,
                      const base::string16& function_driver,
                      const base::string16& function_path);

 private:
  struct Interface;
  class Request;

  using OpenInterfaceCallback = base::OnceCallback<void(Interface*)>;

  struct Interface {
    Interface();
    ~Interface();

    const mojom::UsbInterfaceInfo* info;

    // In a composite device each function has its own driver and path to open.
    base::string16 function_driver;
    base::string16 function_path;
    base::win::ScopedHandle function_handle;

    ScopedWinUsbHandle handle;
    bool claimed = false;

    // The currently selected alternative interface setting. This is assumed to
    // be the first alternate when the device is opened.
    uint8_t alternate_setting = 0;

    // The count of outstanding requests, including associated interfaces
    // claimed which require keeping the handles owned by this object open.
    int reference_count = 0;

    // Closures to execute when |function_path| has been populated.
    std::vector<OpenInterfaceCallback> ready_callbacks;

    DISALLOW_COPY_AND_ASSIGN(Interface);
  };

  struct Endpoint {
    const mojom::UsbInterfaceInfo* interface;
    mojom::UsbTransferType type;
  };

  void OpenInterfaceHandle(Interface* interface,
                           OpenInterfaceCallback callback);
  void OnFunctionAvailable(OpenInterfaceCallback callback,
                           Interface* interface);
  void OnFirstInterfaceOpened(int interface_number,
                              OpenInterfaceCallback callback,
                              Interface* first_interface);
  void OnInterfaceClaimed(ResultCallback callback, Interface* interface);
  void OnSetAlternateInterfaceSetting(int interface_number,
                                      int alternate_setting,
                                      ResultCallback callback,
                                      bool result);
  void RegisterEndpoints(const mojom::UsbInterfaceInfo* interface,
                         const mojom::UsbAlternateInterfaceInfo& alternate);
  void UnregisterEndpoints(const mojom::UsbAlternateInterfaceInfo& alternate);
  void OnClearHalt(int interface_number, ResultCallback callback, bool result);
  void OpenInterfaceForControlTransfer(
      mojom::UsbControlTransferRecipient recipient,
      uint16_t index,
      OpenInterfaceCallback callback);
  void OnFunctionAvailableForEp0(OpenInterfaceCallback callback,
                                 Interface* interface);
  void OnInterfaceOpenedForControlTransfer(
      mojom::UsbTransferDirection direction,
      mojom::UsbControlTransferType request_type,
      mojom::UsbControlTransferRecipient recipient,
      uint8_t request,
      uint16_t value,
      uint16_t index,
      scoped_refptr<base::RefCountedBytes> buffer,
      unsigned int timeout,
      TransferCallback callback,
      Interface* interface);
  Request* MakeRequest(Interface* interface);
  std::unique_ptr<Request> UnlinkRequest(Request* request);
  void GotNodeConnectionInformation(TransferCallback callback,
                                    void* node_connection_info,
                                    scoped_refptr<base::RefCountedBytes> buffer,
                                    Request* request_ptr,
                                    DWORD win32_result,
                                    size_t bytes_transferred);
  void GotDescriptorFromNodeConnection(
      TransferCallback callback,
      scoped_refptr<base::RefCountedBytes> request_buffer,
      scoped_refptr<base::RefCountedBytes> original_buffer,
      Request* request_ptr,
      DWORD win32_result,
      size_t bytes_transferred);
  void TransferComplete(TransferCallback callback,
                        scoped_refptr<base::RefCountedBytes> buffer,
                        Request* request_ptr,
                        DWORD win32_result,
                        size_t bytes_transferred);
  void ReportIsochronousError(const std::vector<uint32_t>& packet_lengths,
                              IsochronousTransferCallback callback,
                              mojom::UsbTransferStatus status);
  bool AllFunctionsEnumerated() const;
  void ReleaseInterfaceReference(Interface* interface);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<UsbDeviceWin> device_;

  // |hub_handle_| or all the handles for claimed interfaces in |interfaces_|
  // must outlive their associated |requests_| because individual Request
  // objects hold on to the raw handles for the purpose of calling
  // GetOverlappedResult().
  base::win::ScopedHandle hub_handle_;

  std::map<uint8_t, Interface> interfaces_;
  std::map<uint8_t, Endpoint> endpoints_;
  std::map<Request*, std::unique_ptr<Request>> requests_;

  // Control transfers which are waiting for a function handle to be ready.
  std::vector<OpenInterfaceCallback> ep0_ready_callbacks_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::WeakPtrFactory<UsbDeviceHandleWin> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceHandleWin);
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_DEVICE_HANDLE_WIN_H_
