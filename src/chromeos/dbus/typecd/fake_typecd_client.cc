// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/typecd/fake_typecd_client.h"
#include "third_party/cros_system_api/dbus/typecd/dbus-constants.h"

namespace chromeos {

FakeTypecdClient::FakeTypecdClient() = default;
FakeTypecdClient::~FakeTypecdClient() = default;

void FakeTypecdClient::EmitThunderboltDeviceConnectedSignal(
    bool is_thunderbolt_only) {
  NotifyOnThunderboltDeviceConnected(is_thunderbolt_only);
}

void FakeTypecdClient::EmitCableWarningSignal(typecd::CableWarningType type) {
  NotifyOnCableWarning(type);
}

}  // namespace chromeos
