// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_HERMES_HERMES_CLIENTS_H_
#define CHROMEOS_DBUS_HERMES_HERMES_CLIENTS_H_

#include "base/component_export.h"

namespace dbus {
class Bus;
}

namespace chromeos {

namespace hermes_clients {

// Initializes all Hermes dbus clients in the correct order.
COMPONENT_EXPORT(HERMES_CLIENT) void Initialize(dbus::Bus* system_bus);

// Initializes fake implementations of all Hermes dbus clients.
COMPONENT_EXPORT(HERMES_CLIENT) void InitializeFakes();

// Shutdown all Hermes dbus clients.
COMPONENT_EXPORT(HERMES_CLIENT) void Shutdown();

}  // namespace hermes_clients

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace hermes_clients {
using ::chromeos::hermes_clients::Initialize;
using ::chromeos::hermes_clients::Shutdown;
}  // namespace hermes_clients
}  // namespace ash

#endif  // CHROMEOS_DBUS_HERMES_HERMES_CLIENTS_H_
