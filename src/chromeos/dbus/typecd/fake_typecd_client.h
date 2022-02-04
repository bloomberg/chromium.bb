// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_TYPECD_FAKE_TYPECD_CLIENT_H_
#define CHROMEOS_DBUS_TYPECD_FAKE_TYPECD_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/dbus/typecd/typecd_client.h"
#include "third_party/cros_system_api/dbus/typecd/dbus-constants.h"

namespace chromeos {

class COMPONENT_EXPORT(TYPECD) FakeTypecdClient : public TypecdClient {
 public:
  FakeTypecdClient();
  FakeTypecdClient(const FakeTypecdClient&) = delete;
  FakeTypecdClient& operator=(const FakeTypecdClient&) = delete;
  ~FakeTypecdClient() override;

  // This is a simple fake to notify observers of a simulated D-Bus received
  // signal.
  void EmitThunderboltDeviceConnectedSignal(bool is_thunderbolt_only);
  void EmitCableWarningSignal(typecd::CableWarningType type);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_TYPECD_FAKE_TYPECD_CLIENT_H_
