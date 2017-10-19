// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBInterface_h
#define USBInterface_h

#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Heap.h"

namespace blink {

class ExceptionState;
class USBAlternateInterface;
class USBConfiguration;
class USBDevice;

class USBInterface : public GarbageCollected<USBInterface>,
                     public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBInterface* Create(const USBConfiguration*, size_t interface_index);
  static USBInterface* Create(const USBConfiguration*,
                              size_t interface_number,
                              ExceptionState&);

  USBInterface(const USBDevice*,
               size_t configuration_index,
               size_t interface_index);

  const device::mojom::blink::UsbInterfaceInfo& Info() const;

  uint8_t interfaceNumber() const { return Info().interface_number; }
  USBAlternateInterface* alternate() const;
  HeapVector<Member<USBAlternateInterface>> alternates() const;
  bool claimed() const;

  void Trace(blink::Visitor*);

 private:
  Member<const USBDevice> device_;
  const size_t configuration_index_;
  const size_t interface_index_;
};

}  // namespace blink

#endif  // USBInterface_h
