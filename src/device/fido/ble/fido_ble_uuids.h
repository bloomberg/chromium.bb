// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_BLE_FIDO_BLE_UUIDS_H_
#define DEVICE_FIDO_BLE_FIDO_BLE_UUIDS_H_

#include "base/component_export.h"

namespace device {

// FIDO GATT Service's UUIDs as defined by the standard:
// https://fidoalliance.org/specs/fido-v2.0-rd-20161004/fido-client-to-authenticator-protocol-v2.0-rd-20161004.html#gatt-service-description
//
// For details on how the short UUIDs for FIDO Service (0xFFFD) and FIDO Service
// Revision (0x2A28) were converted to the long canonical ones, see
// https://www.bluetooth.com/specifications/assigned-numbers/service-discovery
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kFidoServiceUUID[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kFidoControlPointUUID[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kFidoStatusUUID[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kFidoControlPointLengthUUID[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kFidoServiceRevisionUUID[];
COMPONENT_EXPORT(DEVICE_FIDO)
extern const char kFidoServiceRevisionBitfieldUUID[];
// TODO(hongjunchoi): Add URL to the specification once CaBLE protocol is
// standardized.
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCableAdvertisementUUID16[];
COMPONENT_EXPORT(DEVICE_FIDO) extern const char kCableAdvertisementUUID128[];

}  // namespace device

#endif  // DEVICE_FIDO_BLE_FIDO_BLE_UUIDS_H_
