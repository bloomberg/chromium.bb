// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothCharacteristicProperties_h
#define BluetoothCharacteristicProperties_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

// Each BluetoothRemoteGATTCharacteristic exposes its characteristic properties
// through a BluetoothCharacteristicProperties object. These properties express
// what operations are valid on the characteristic.
class BluetoothCharacteristicProperties final
    : public GarbageCollected<BluetoothCharacteristicProperties>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BluetoothCharacteristicProperties* Create(uint32_t properties);

  // IDL exposed interface:
  bool broadcast() const;
  bool read() const;
  bool writeWithoutResponse() const;
  bool write() const;
  bool notify() const;
  bool indicate() const;
  bool authenticatedSignedWrites() const;
  bool reliableWrite() const;
  bool writableAuxiliaries() const;

  void Trace(blink::Visitor* visitor) {}

 private:
  explicit BluetoothCharacteristicProperties(uint32_t properties);

  enum Property {
    kNone = 0,
    kBroadcast = 1 << 0,
    kRead = 1 << 1,
    kWriteWithoutResponse = 1 << 2,
    kWrite = 1 << 3,
    kNotify = 1 << 4,
    kIndicate = 1 << 5,
    kAuthenticatedSignedWrites = 1 << 6,
    kExtendedProperties = 1 << 7,  // Not used in class.
    kReliableWrite = 1 << 8,
    kWritableAuxiliaries = 1 << 9,
  };

  uint32_t properties;
};

}  // namespace blink

#endif  // GamepadButton_h
