// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NEARBY_PUBLIC_CPP_NEARBY_CLIENT_UUIDS_H_
#define CHROMEOS_SERVICES_NEARBY_PUBLIC_CPP_NEARBY_CLIENT_UUIDS_H_

#include <vector>

#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace chromeos {
namespace nearby {

// Returns a list of Bluetooth Service UUIDs corresponding to current clients
// of Nearby Connections (e.g., Nearby Share). Callers can use this function or
// IsNearbyClientUuid() to verify that outgoing or incoming Bluetooth
// connections are initiated by said recognized Nearby Connections clients.
const std::vector<device::BluetoothUUID>& GetNearbyClientUuids();

// Helper function to check if |uuid| is present in list returned by
// GetNearbyClientUuids().
bool IsNearbyClientUuid(const device::BluetoothUUID& uuid);

}  // namespace nearby
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash {
namespace nearby {
using ::chromeos::nearby::GetNearbyClientUuids;
}
}  // namespace ash

#endif  // CHROMEOS_SERVICES_NEARBY_PUBLIC_CPP_NEARBY_CLIENT_UUIDS_H_
