// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/has_secure_wifi_connection_routine.h"

#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {
namespace network_diagnostics {

namespace {

const char* kInsecureSecurity = shill::kSecurityWep;
const char* kSecureSecurity = shill::kSecurityPsk;

}  // namespace

class HasSecureWiFiConnectionRoutineTest : public ::testing::Test {
 public:
  HasSecureWiFiConnectionRoutineTest() {
    has_secure_wifi_connection_routine_ =
        std::make_unique<HasSecureWiFiConnectionRoutine>();
  }

  void CompareVerdict(mojom::RoutineVerdict expected_verdict,
                      const std::vector<mojom::HasSecureWiFiConnectionProblem>&
                          expected_problems,
                      mojom::RoutineVerdict actual_verdict,
                      const std::vector<mojom::HasSecureWiFiConnectionProblem>&
                          actual_problems) {
    EXPECT_EQ(expected_verdict, actual_verdict);
    EXPECT_EQ(expected_problems, actual_problems);
  }

  void SetUpWiFi(const char* state, std::string security) {
    DCHECK(wifi_path_.empty());
    // By default, NetworkStateTestHelper already adds a WiFi device, so, we
    // do not need to add one here. All that remains to be done is configuring
    // the WiFi service.
    wifi_path_ = ConfigureService(
        R"({"GUID": "wifi_guid", "Type": "wifi", "State": "idle"})");
    SetServiceProperty(wifi_path_, shill::kStateProperty, base::Value(state));
    SetServiceProperty(wifi_path_, shill::kSecurityClassProperty,
                       base::Value(security));
    base::RunLoop().RunUntilIdle();
  }

  chromeos::NetworkStateTestHelper& network_state_helper() {
    return cros_network_config_test_helper_.network_state_helper();
  }
  HasSecureWiFiConnectionRoutine* has_secure_wifi_connection_routine() {
    return has_secure_wifi_connection_routine_.get();
  }

 protected:
  base::WeakPtr<HasSecureWiFiConnectionRoutineTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::string ConfigureService(const std::string& shill_json_string) {
    return network_state_helper().ConfigureService(shill_json_string);
  }
  void SetServiceProperty(const std::string& service_path,
                          const std::string& key,
                          const base::Value& value) {
    network_state_helper().SetServiceProperty(service_path, key, value);
  }
  const std::string& wifi_path() const { return wifi_path_; }

  content::BrowserTaskEnvironment task_environment_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_;
  std::unique_ptr<HasSecureWiFiConnectionRoutine>
      has_secure_wifi_connection_routine_;
  std::string wifi_path_;
  base::WeakPtrFactory<HasSecureWiFiConnectionRoutineTest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HasSecureWiFiConnectionRoutineTest);
};

TEST_F(HasSecureWiFiConnectionRoutineTest, TestSecureWiFiConnection) {
  SetUpWiFi(shill::kStateOnline, kSecureSecurity);
  std::vector<mojom::HasSecureWiFiConnectionProblem> expected_problems = {};
  has_secure_wifi_connection_routine()->RunTest(base::BindOnce(
      &HasSecureWiFiConnectionRoutineTest::CompareVerdict, weak_ptr(),
      mojom::RoutineVerdict::kNoProblem, expected_problems));
  base::RunLoop().RunUntilIdle();
}

TEST_F(HasSecureWiFiConnectionRoutineTest, TestInsecureWiFiConnection) {
  SetUpWiFi(shill::kStateOnline, kInsecureSecurity);
  std::vector<mojom::HasSecureWiFiConnectionProblem> expected_problems = {
      mojom::HasSecureWiFiConnectionProblem::kSecurityTypeWepPsk};
  has_secure_wifi_connection_routine()->RunTest(base::BindOnce(
      &HasSecureWiFiConnectionRoutineTest::CompareVerdict, weak_ptr(),
      mojom::RoutineVerdict::kProblem, expected_problems));
  base::RunLoop().RunUntilIdle();
}

TEST_F(HasSecureWiFiConnectionRoutineTest, TestWiFiNotConnected) {
  SetUpWiFi(shill::kStateOffline, kSecureSecurity);
  std::vector<mojom::HasSecureWiFiConnectionProblem> expected_problems = {};
  has_secure_wifi_connection_routine()->RunTest(base::BindOnce(
      &HasSecureWiFiConnectionRoutineTest::CompareVerdict, weak_ptr(),
      mojom::RoutineVerdict::kNotRun, expected_problems));
  base::RunLoop().RunUntilIdle();
}

}  // namespace network_diagnostics
}  // namespace chromeos
