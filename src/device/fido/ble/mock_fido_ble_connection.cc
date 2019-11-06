// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/mock_fido_ble_connection.h"

#include <utility>

namespace device {

MockFidoBleConnection::MockFidoBleConnection(BluetoothAdapter* adapter,
                                             std::string device_address)
    : FidoBleConnection(adapter, std::move(device_address)) {}

MockFidoBleConnection::~MockFidoBleConnection() = default;

void MockFidoBleConnection::Connect(ConnectionCallback callback) {
  ConnectPtr(&callback);
}

void MockFidoBleConnection::ReadControlPointLength(
    ControlPointLengthCallback callback) {
  ReadControlPointLengthPtr(&callback);
}

void MockFidoBleConnection::WriteControlPoint(const std::vector<uint8_t>& data,
                                              WriteCallback callback) {
  WriteControlPointPtr(data, &callback);
}

}  // namespace device
