// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_name_util.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/network/test_cellular_esim_profile_handler.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kTestEuiccPath[] = "euicc_path";
const char kTestIccid[] = "iccid";
const char kTestProfileName[] = "test_profile_name";
const char kTestEidName[] = "eid";
const char kTestEthName[] = "test_eth_name";
const char kTestNameFromShill[] = "shill_network_name";

const char kTestESimCellularServicePath[] = "/service/cellular1";
const char kTestEthServicePath[] = "/service/eth0";

}  // namespace

class NetworkNameUtilTest : public testing::Test {
 public:
  NetworkNameUtilTest() {}
  ~NetworkNameUtilTest() override = default;

  // testing::Test:
  void SetUp() override {
    cellular_inhibitor_ = std::make_unique<CellularInhibitor>();
    cellular_esim_profile_handler_ =
        std::make_unique<TestCellularESimProfileHandler>();

    network_state_test_helper_.hermes_manager_test()->AddEuicc(
        dbus::ObjectPath(kTestEuiccPath), kTestEidName, /*is_active=*/true,
        /*physical_slot=*/0);
    cellular_esim_profile_handler_->Init(
        network_state_test_helper_.network_state_handler(),
        cellular_inhibitor_.get());
    base::RunLoop().RunUntilIdle();
  }

  void AddESimProfile(hermes::profile::State state,
                      const std::string& service_path) {
    network_state_test_helper_.hermes_euicc_test()->AddCarrierProfile(
        dbus::ObjectPath(service_path), dbus::ObjectPath(kTestEuiccPath),
        kTestIccid, kTestProfileName, "service_provider", "activation_code",
        service_path, state, hermes::profile::ProfileClass::kOperational,
        HermesEuiccClient::TestInterface::AddCarrierProfileBehavior::
            kAddProfileWithService);
    base::RunLoop().RunUntilIdle();
  }

  void AddEthernet() {
    ShillServiceClient::TestInterface* service_test =
        network_state_test_helper_.service_test();
    service_test->AddService(kTestEthServicePath, "test_guid1", kTestEthName,
                             shill::kTypeEthernet, shill::kStateIdle, true);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  NetworkStateTestHelper network_state_test_helper_{
      false /* use_default_devices_and_services */};
  std::unique_ptr<CellularInhibitor> cellular_inhibitor_;
  std::unique_ptr<TestCellularESimProfileHandler>
      cellular_esim_profile_handler_;
};

TEST_F(NetworkNameUtilTest, EsimNetworkGetNetworkName) {
  AddESimProfile(hermes::profile::State::kActive, kTestESimCellularServicePath);

  const chromeos::NetworkState* network =
      network_state_test_helper_.network_state_handler()->GetNetworkState(
          kTestESimCellularServicePath);

  std::string name = network_name_util::GetNetworkName(
      cellular_esim_profile_handler_.get(), network);

  EXPECT_EQ(name, kTestProfileName);
}

TEST_F(NetworkNameUtilTest, EthernetNetworkGetNetworkName) {
  AddEthernet();
  const chromeos::NetworkState* network =
      network_state_test_helper_.network_state_handler()->GetNetworkState(
          kTestEthServicePath);

  std::string name = network_name_util::GetNetworkName(
      cellular_esim_profile_handler_.get(), network);

  EXPECT_EQ(name, kTestEthName);
}

TEST_F(NetworkNameUtilTest, NameComesFromHermes) {
  AddESimProfile(hermes::profile::State::kActive, kTestESimCellularServicePath);

  // Change the network's name in Shill. Now, Hermes and Shill have different
  // names associated with the profile.
  network_state_test_helper_.SetServiceProperty(
      kTestESimCellularServicePath, shill::kNameProperty,
      base::Value(kTestNameFromShill));
  base::RunLoop().RunUntilIdle();

  const chromeos::NetworkState* network =
      network_state_test_helper_.network_state_handler()->GetNetworkState(
          kTestESimCellularServicePath);

  std::string name = network_name_util::GetNetworkName(
      cellular_esim_profile_handler_.get(), network);

  EXPECT_EQ(name, kTestProfileName);
}

}  // namespace chromeos
