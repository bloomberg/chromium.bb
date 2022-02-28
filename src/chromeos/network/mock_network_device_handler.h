// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_MOCK_NETWORK_DEVICE_HANDLER_H_
#define CHROMEOS_NETWORK_MOCK_NETWORK_DEVICE_HANDLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "net/base/ip_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) MockNetworkDeviceHandler
    : public NetworkDeviceHandler {
 public:
  MockNetworkDeviceHandler();

  MockNetworkDeviceHandler(const MockNetworkDeviceHandler&) = delete;
  MockNetworkDeviceHandler& operator=(const MockNetworkDeviceHandler&) = delete;

  virtual ~MockNetworkDeviceHandler();

  MOCK_CONST_METHOD2(GetDeviceProperties,
                     void(const std::string& device_path,
                          network_handler::ResultCallback callback));

  MOCK_METHOD5(SetDeviceProperty,
               void(const std::string& device_path,
                    const std::string& property_name,
                    const base::Value& value,
                    base::OnceClosure callback,
                    network_handler::ErrorCallback error_callback));

  MOCK_METHOD4(RegisterCellularNetwork,
               void(const std::string& device_path,
                    const std::string& network_id,
                    base::OnceClosure callback,
                    network_handler::ErrorCallback error_callback));

  MOCK_METHOD5(RequirePin,
               void(const std::string& device_path,
                    bool require_pin,
                    const std::string& pin,
                    base::OnceClosure callback,
                    network_handler::ErrorCallback error_callback));

  MOCK_METHOD4(EnterPin,
               void(const std::string& device_path,
                    const std::string& pin,
                    base::OnceClosure callback,
                    network_handler::ErrorCallback error_callback));

  MOCK_METHOD5(UnblockPin,
               void(const std::string& device_path,
                    const std::string& puk,
                    const std::string& new_pin,
                    base::OnceClosure callback,
                    network_handler::ErrorCallback error_callback));

  MOCK_METHOD5(ChangePin,
               void(const std::string& device_path,
                    const std::string& old_pin,
                    const std::string& new_pin,
                    base::OnceClosure callback,
                    network_handler::ErrorCallback error_callback));

  MOCK_METHOD1(SetCellularPolicyAllowRoaming, void(bool policy_allow_roaming));

  MOCK_METHOD1(SetMACAddressRandomizationEnabled, void(bool enabled));

  MOCK_METHOD1(SetUsbEthernetMacAddressSource,
               void(const std::string& enabled));
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_MOCK_NETWORK_DEVICE_HANDLER_H_
