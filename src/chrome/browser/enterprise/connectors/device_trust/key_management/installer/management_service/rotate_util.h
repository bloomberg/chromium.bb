// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_ROTATE_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_ROTATE_UTIL_H_

#include <memory>

#include "base/command_line.h"
#include "components/version_info/channel.h"

namespace enterprise_connectors {

class KeyRotationManager;

// Rotates the device trust signing key and saves it to a global location
// on the machine accessible to all install modes of the browser (i.e.,
// stable and all three side-by-side modes). Returns the result of the
// rotation. `key_rotation_manager` is the rotation manager for the
// current platform. The `command_line` is the command_line of the current
// process and the `channel` is the build channel (i.e stable, dev, etc).
bool RotateDeviceTrustKey(
    std::unique_ptr<KeyRotationManager> key_rotation_manager,
    const base::CommandLine* command_line,
    version_info::Channel channel);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_ROTATE_UTIL_H_
