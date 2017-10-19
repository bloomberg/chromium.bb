// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBConfiguration_h
#define USBConfiguration_h

#include "device/usb/public/interfaces/device.mojom-blink.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class USBDevice;
class USBInterface;

class USBConfiguration : public GarbageCollected<USBConfiguration>,
                         public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static USBConfiguration* Create(const USBDevice*, size_t configuration_index);
  static USBConfiguration* Create(const USBDevice*,
                                  size_t configuration_value,
                                  ExceptionState&);

  USBConfiguration(const USBDevice*, size_t configuration_index);

  const USBDevice* Device() const;
  size_t Index() const;
  const device::mojom::blink::UsbConfigurationInfo& Info() const;

  uint8_t configurationValue() const { return Info().configuration_value; }
  String configurationName() const { return Info().configuration_name; }
  HeapVector<Member<USBInterface>> interfaces() const;

  void Trace(blink::Visitor*);

 private:
  Member<const USBDevice> device_;
  const size_t configuration_index_;
};

}  // namespace blink

#endif  // USBConfiguration_h
