// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBDevice_h
#define USBDevice_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/BitVector.h"
#include "platform/wtf/Vector.h"

namespace blink {

class ScriptPromiseResolver;
class ScriptState;
class USBConfiguration;
class USBControlTransferParameters;

class USBDevice : public GarbageCollectedFinalized<USBDevice>,
                  public ContextLifecycleObserver,
                  public ScriptWrappable {
  USING_GARBAGE_COLLECTED_MIXIN(USBDevice);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBDevice* Create(device::mojom::blink::UsbDeviceInfoPtr device_info,
                           device::mojom::blink::UsbDevicePtr device,
                           ExecutionContext* context) {
    return new USBDevice(std::move(device_info), std::move(device), context);
  }

  explicit USBDevice(device::mojom::blink::UsbDeviceInfoPtr,
                     device::mojom::blink::UsbDevicePtr,
                     ExecutionContext*);
  virtual ~USBDevice();

  const device::mojom::blink::UsbDeviceInfo& Info() const {
    return *device_info_;
  }
  bool IsInterfaceClaimed(size_t configuration_index,
                          size_t interface_index) const;
  size_t SelectedAlternateInterface(size_t interface_index) const;

  // USBDevice.idl
  uint8_t usbVersionMajor() const { return Info().usb_version_major; }
  uint8_t usbVersionMinor() const { return Info().usb_version_minor; }
  uint8_t usbVersionSubminor() const { return Info().usb_version_subminor; }
  uint8_t deviceClass() const { return Info().class_code; }
  uint8_t deviceSubclass() const { return Info().subclass_code; }
  uint8_t deviceProtocol() const { return Info().protocol_code; }
  uint16_t vendorId() const { return Info().vendor_id; }
  uint16_t productId() const { return Info().product_id; }
  uint8_t deviceVersionMajor() const { return Info().device_version_major; }
  uint8_t deviceVersionMinor() const { return Info().device_version_minor; }
  uint8_t deviceVersionSubminor() const {
    return Info().device_version_subminor;
  }
  String manufacturerName() const { return Info().manufacturer_name; }
  String productName() const { return Info().product_name; }
  String serialNumber() const { return Info().serial_number; }
  USBConfiguration* configuration() const;
  HeapVector<Member<USBConfiguration>> configurations() const;
  bool opened() const { return opened_; }

  ScriptPromise open(ScriptState*);
  ScriptPromise close(ScriptState*);
  ScriptPromise selectConfiguration(ScriptState*, uint8_t configuration_value);
  ScriptPromise claimInterface(ScriptState*, uint8_t interface_number);
  ScriptPromise releaseInterface(ScriptState*, uint8_t interface_number);
  ScriptPromise selectAlternateInterface(ScriptState*,
                                         uint8_t interface_number,
                                         uint8_t alternate_setting);
  ScriptPromise controlTransferIn(ScriptState*,
                                  const USBControlTransferParameters& setup,
                                  unsigned length);
  ScriptPromise controlTransferOut(ScriptState*,
                                   const USBControlTransferParameters& setup);
  ScriptPromise controlTransferOut(ScriptState*,
                                   const USBControlTransferParameters& setup,
                                   const ArrayBufferOrArrayBufferView& data);
  ScriptPromise clearHalt(ScriptState*,
                          String direction,
                          uint8_t endpoint_number);
  ScriptPromise transferIn(ScriptState*,
                           uint8_t endpoint_number,
                           unsigned length);
  ScriptPromise transferOut(ScriptState*,
                            uint8_t endpoint_number,
                            const ArrayBufferOrArrayBufferView& data);
  ScriptPromise isochronousTransferIn(ScriptState*,
                                      uint8_t endpoint_number,
                                      Vector<unsigned> packet_lengths);
  ScriptPromise isochronousTransferOut(ScriptState*,
                                       uint8_t endpoint_number,
                                       const ArrayBufferOrArrayBufferView& data,
                                       Vector<unsigned> packet_lengths);
  ScriptPromise reset(ScriptState*);

  // ContextLifecycleObserver interface.
  void ContextDestroyed(ExecutionContext*) override;

  void Trace(blink::Visitor*);

 private:
  int FindConfigurationIndex(uint8_t configuration_value) const;
  int FindInterfaceIndex(uint8_t interface_number) const;
  int FindAlternateIndex(size_t interface_index,
                         uint8_t alternate_setting) const;
  bool EnsureNoDeviceOrInterfaceChangeInProgress(ScriptPromiseResolver*) const;
  bool EnsureDeviceConfigured(ScriptPromiseResolver*) const;
  bool EnsureInterfaceClaimed(uint8_t interface_number,
                              ScriptPromiseResolver*) const;
  bool EnsureEndpointAvailable(bool in_transfer,
                               uint8_t endpoint_number,
                               ScriptPromiseResolver*) const;
  bool AnyInterfaceChangeInProgress() const;
  device::mojom::blink::UsbControlTransferParamsPtr
  ConvertControlTransferParameters(const USBControlTransferParameters&,
                                   ScriptPromiseResolver*) const;
  void SetEndpointsForInterface(size_t interface_index, bool set);

  void AsyncOpen(ScriptPromiseResolver*,
                 device::mojom::blink::UsbOpenDeviceError);
  void AsyncClose(ScriptPromiseResolver*);
  void OnDeviceOpenedOrClosed(bool);
  void AsyncSelectConfiguration(size_t configuration_index,
                                ScriptPromiseResolver*,
                                bool success);
  void OnConfigurationSelected(bool success, size_t configuration_index);
  void AsyncClaimInterface(size_t interface_index,
                           ScriptPromiseResolver*,
                           bool success);
  void AsyncReleaseInterface(size_t interface_index,
                             ScriptPromiseResolver*,
                             bool success);
  void OnInterfaceClaimedOrUnclaimed(bool claimed, size_t interface_index);
  void AsyncSelectAlternateInterface(size_t interface_index,
                                     size_t alternate_index,
                                     ScriptPromiseResolver*,
                                     bool success);
  void AsyncControlTransferIn(ScriptPromiseResolver*,
                              device::mojom::blink::UsbTransferStatus,
                              const Vector<uint8_t>&);
  void AsyncControlTransferOut(unsigned,
                               ScriptPromiseResolver*,
                               device::mojom::blink::UsbTransferStatus);
  void AsyncClearHalt(ScriptPromiseResolver*, bool success);
  void AsyncTransferIn(ScriptPromiseResolver*,
                       device::mojom::blink::UsbTransferStatus,
                       const Vector<uint8_t>&);
  void AsyncTransferOut(unsigned,
                        ScriptPromiseResolver*,
                        device::mojom::blink::UsbTransferStatus);
  void AsyncIsochronousTransferIn(
      ScriptPromiseResolver*,
      const Vector<uint8_t>&,
      Vector<device::mojom::blink::UsbIsochronousPacketPtr>);
  void AsyncIsochronousTransferOut(
      ScriptPromiseResolver*,
      Vector<device::mojom::blink::UsbIsochronousPacketPtr>);
  void AsyncReset(ScriptPromiseResolver*, bool success);

  void OnConnectionError();
  bool MarkRequestComplete(ScriptPromiseResolver*);

  device::mojom::blink::UsbDeviceInfoPtr device_info_;
  device::mojom::blink::UsbDevicePtr device_;
  HeapHashSet<Member<ScriptPromiseResolver>> device_requests_;
  bool opened_;
  bool device_state_change_in_progress_;
  int configuration_index_;
  WTF::BitVector claimed_interfaces_;
  WTF::BitVector interface_state_change_in_progress_;
  WTF::Vector<size_t> selected_alternates_;
  WTF::BitVector in_endpoints_;
  WTF::BitVector out_endpoints_;
};

}  // namespace blink

#endif  // USBDevice_h
