// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_DEVICE_MANAGEMENT_DM_MESSAGE_H_
#define CHROME_UPDATER_DEVICE_MANAGEMENT_DM_MESSAGE_H_

#include <string>

#include "base/containers/flat_map.h"

namespace updater {

class CachedPolicyInfo;

// DM policy map: policy_type --> serialized policy data of PolicyFetchResponse.
using DMPolicyMap = base::flat_map<std::string, std::string>;

// Returns the serialized data from a DeviceManagementRequest, which wraps
// a RegisterBrowserRequest, to register the current device.
std::string GetRegisterBrowserRequestData(const std::string& machine_name,
                                          const std::string& os_platform,
                                          const std::string& os_version);

// Returns the serialized data from a DeviceManagementRequest, which wraps
// a PolicyFetchRequest, to fetch policies for the given type.
std::string GetPolicyFetchRequestData(const std::string& policy_type,
                                      const CachedPolicyInfo& policy_info);

// Parses the DeviceManagementResponse for a device registration request, and
// returns the DM token. Returns empty string if parsing failed or the response
// is unexpected.
std::string ParseDeviceRegistrationResponse(const std::string& response_data);

// Parses the DeviceManagementResponse for a policy fetch request, and returns
// DMPolicyMap. |policy_info|, |expected_dm_token|, |expected_device_id| are
// used to verify the response and check whether the response is intended for
// current device.
DMPolicyMap ParsePolicyFetchResponse(const std::string& response_data,
                                     const CachedPolicyInfo& policy_info,
                                     const std::string& expected_dm_token,
                                     const std::string& expected_device_id);
}  // namespace updater

#endif  // CHROME_UPDATER_DEVICE_MANAGEMENT_DM_MESSAGE_H_
