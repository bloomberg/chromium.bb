// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBAlternateInterface_h
#define USBAlternateInterface_h

#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Heap.h"

namespace blink {

class ExceptionState;
class USBEndpoint;
class USBInterface;

class USBAlternateInterface : public GarbageCollected<USBAlternateInterface>,
                              public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBAlternateInterface* Create(const USBInterface*,
                                       size_t alternate_index);
  static USBAlternateInterface* Create(const USBInterface*,
                                       size_t alternate_setting,
                                       ExceptionState&);

  USBAlternateInterface(const USBInterface*, size_t alternate_index);

  const device::mojom::blink::UsbAlternateInterfaceInfo& Info() const;

  uint8_t alternateSetting() const { return Info().alternate_setting; }
  uint8_t interfaceClass() const { return Info().class_code; }
  uint8_t interfaceSubclass() const { return Info().subclass_code; }
  uint8_t interfaceProtocol() const { return Info().protocol_code; }
  String interfaceName() const { return Info().interface_name; }
  HeapVector<Member<USBEndpoint>> endpoints() const;

  void Trace(blink::Visitor*);

 private:
  Member<const USBInterface> interface_;
  const size_t alternate_index_;
};

}  // namespace blink

#endif  // USBAlternateInterface_h
