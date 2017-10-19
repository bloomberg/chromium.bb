// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBEndpoint_h
#define USBEndpoint_h

#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Heap.h"

namespace blink {

class ExceptionState;
class USBAlternateInterface;

class USBEndpoint : public GarbageCollected<USBEndpoint>,
                    public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBEndpoint* Create(const USBAlternateInterface*,
                             size_t endpoint_index);
  static USBEndpoint* Create(const USBAlternateInterface*,
                             size_t endpoint_number,
                             const String& direction,
                             ExceptionState&);

  USBEndpoint(const USBAlternateInterface*, size_t endpoint_index);

  const device::mojom::blink::UsbEndpointInfo& Info() const;

  uint8_t endpointNumber() const { return Info().endpoint_number; }
  String direction() const;
  String type() const;
  unsigned packetSize() const { return Info().packet_size; }

  void Trace(blink::Visitor*);

 private:
  Member<const USBAlternateInterface> alternate_;
  const size_t endpoint_index_;
};

}  // namespace blink

#endif  // USBEndpoint_h
