// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_ADVERTISING_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_ADVERTISING_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

class BluetoothDevice;
class BluetoothAdvertisingEventInit;
class BluetoothManufacturerDataMap;
class BluetoothServiceDataMap;
class StringOrUnsignedLong;

class BluetoothAdvertisingEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static BluetoothAdvertisingEvent* Create(
      const AtomicString& event_type,
      const BluetoothAdvertisingEventInit* initializer) {
    return MakeGarbageCollected<BluetoothAdvertisingEvent>(event_type,
                                                           initializer);
  }

  static BluetoothAdvertisingEvent* Create(
      const AtomicString& event_type,
      BluetoothDevice* device,
      const String& name,
      const HeapVector<StringOrUnsignedLong>& uuids,
      base::Optional<short> appearance,
      base::Optional<int8_t> txPower,
      base::Optional<int8_t> rssi,
      BluetoothManufacturerDataMap* manufacturer_data_map,
      BluetoothServiceDataMap* service_data_map) {
    return MakeGarbageCollected<BluetoothAdvertisingEvent>(
        event_type, device, name, uuids, appearance, txPower, rssi,
        manufacturer_data_map, service_data_map);
  }

  BluetoothAdvertisingEvent(const AtomicString& event_type,
                            const BluetoothAdvertisingEventInit* initializer);

  BluetoothAdvertisingEvent(const AtomicString& event_type,
                            BluetoothDevice* device,
                            const String& name,
                            const HeapVector<StringOrUnsignedLong>& uuids,
                            base::Optional<short> appearance,
                            base::Optional<int8_t> txPower,
                            base::Optional<int8_t> rssi,
                            BluetoothManufacturerDataMap* manufacturer_data_map,
                            BluetoothServiceDataMap* service_data_map);

  ~BluetoothAdvertisingEvent() override;

  void Trace(blink::Visitor*) override;

  const AtomicString& InterfaceName() const override;

  BluetoothDevice* device() const;
  const String& name() const;
  const HeapVector<StringOrUnsignedLong>& uuids() const;
  short appearance(bool& is_null) const;
  int8_t txPower(bool& is_null) const;
  int8_t rssi(bool& is_null) const;
  BluetoothManufacturerDataMap* manufacturerData() const;
  BluetoothServiceDataMap* serviceData() const;

 private:
  Member<BluetoothDevice> device_;
  String name_;
  HeapVector<StringOrUnsignedLong> uuids_;
  base::Optional<short> appearance_;
  base::Optional<int8_t> txPower_;
  base::Optional<int8_t> rssi_;
  const Member<BluetoothManufacturerDataMap> manufacturer_data_map_;
  const Member<BluetoothServiceDataMap> service_data_map_;
};

}  // namespace blink

#endif
