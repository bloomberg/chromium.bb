// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_BUILDER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_BUILDER_H_

#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace policy {

typedef TypedPolicyBuilder<enterprise_management::ChromeDeviceSettingsProto>
    DevicePolicyBuilder;

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_POLICY_BUILDER_H_
