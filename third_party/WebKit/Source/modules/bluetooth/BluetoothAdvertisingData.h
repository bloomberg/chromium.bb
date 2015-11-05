// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BluetoothAdvertisingData_h
#define BluetoothAdvertisingData_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

// Represents the data a BLE device is advertising.
class BluetoothAdvertisingData final
    : public GarbageCollected<BluetoothAdvertisingData>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static BluetoothAdvertisingData* create(int8_t txPower, int8_t rssi);

    // IDL exposed interface:
    int8_t txPower(bool& isNull);
    int8_t rssi(bool& isNull);

    // Interface required by garbage collection.
    DEFINE_INLINE_TRACE() { }

private:
    explicit BluetoothAdvertisingData(int8_t txPower, int8_t rssi);

    int8_t m_txPower;
    int8_t m_rssi;
};

} // namespace blink

#endif // BluetoothAdvertisingData_h
