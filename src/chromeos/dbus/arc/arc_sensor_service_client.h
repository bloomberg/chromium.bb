// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ARC_ARC_SENSOR_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_ARC_ARC_SENSOR_SERVICE_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "dbus/bus.h"
#include "dbus/message.h"

namespace chromeos {

// ArcSensorServiceClient is used to communicate with arc-sensor-service.
class COMPONENT_EXPORT(CHROMEOS_DBUS_ARC) ArcSensorServiceClient {
 public:
  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static ArcSensorServiceClient* Get();

  // Bootstraps the Mojo connection between chrome and arc-sensor-service.
  virtual void BootstrapMojoConnection(int fd,
                                       const std::string& token,
                                       VoidDBusMethodCallback callback) = 0;

 protected:
  // Initialize() should be used instead.
  ArcSensorServiceClient();
  virtual ~ArcSensorServiceClient();
  ArcSensorServiceClient(const ArcSensorServiceClient&) = delete;
  ArcSensorServiceClient& operator=(const ArcSensorServiceClient&) = delete;
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
using ::chromeos::ArcSensorServiceClient;
}  // namespace ash

#endif  // CHROMEOS_DBUS_ARC_ARC_SENSOR_SERVICE_CLIENT_H_
