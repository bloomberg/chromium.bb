// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothAdvertisingData.h"

namespace blink {

namespace {
// TODO(ortuno): RSSI Unknown and Tx Power Unknown should have different
// values. Add kUnknownTxPower when implemented: http://crbug.com/551572
const int kUnknownPower = 127;
} // namespace

BluetoothAdvertisingData* BluetoothAdvertisingData::create(int8_t txPower, int8_t rssi)
{
    return new BluetoothAdvertisingData(txPower, rssi);
}

int8_t BluetoothAdvertisingData::txPower(bool& isNull)
{
    if (m_txPower == kUnknownPower) {
        isNull = true;
    }
    return m_txPower;
}

int8_t BluetoothAdvertisingData::rssi(bool& isNull)
{
    if (m_rssi == kUnknownPower) {
        isNull = true;
    }
    return m_rssi;
}

BluetoothAdvertisingData::BluetoothAdvertisingData(int8_t txPower, int8_t rssi)
    : m_txPower(txPower)
    , m_rssi(rssi)
{
}

} // namespace blink
