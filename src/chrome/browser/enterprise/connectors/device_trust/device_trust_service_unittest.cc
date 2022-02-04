// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include <tuple>

#include "base/barrier_closure.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/mock_attestation_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/mock_signals_service.h"
#include "components/enterprise/common/proto/device_trust_report_event.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::NotNull;

namespace {

const base::Value origins[]{base::Value("example1.example.com"),
                            base::Value("example2.example.com")};
const base::Value more_origins[]{base::Value("example1.example.com"),
                                 base::Value("example2.example.com"),
                                 base::Value("example3.example.com")};

}  // namespace

namespace enterprise_connectors {

using test::MockAttestationService;
using test::MockSignalsService;
using AttestationCallback = DeviceTrustService::AttestationCallback;

class DeviceTrustServiceTest
    : public testing::Test,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  void SetUp() override {
    RegisterProfilePrefs(prefs_.registry());

    feature_list_.InitWithFeatureState(kDeviceTrustConnectorEnabled,
                                       is_flag_enabled());

    if (is_policy_enabled()) {
      EnableServicePolicy();
    } else {
      DisableServicePolicy();
    }
  }

  void EnableServicePolicy() {
    prefs_.SetUserPref(kContextAwareAccessSignalsAllowlistPref,
                       std::make_unique<base::ListValue>(origins));
  }

  void DisableServicePolicy() {
    prefs_.SetUserPref(kContextAwareAccessSignalsAllowlistPref,
                       std::make_unique<base::ListValue>());
  }

  const base::Value* GetPolicyUrls() {
    return prefs_.GetList(kContextAwareAccessSignalsAllowlistPref);
  }

  std::unique_ptr<DeviceTrustService> CreateService() {
    connector_ = std::make_unique<DeviceTrustConnectorService>(&prefs_);

    auto mock_attestation_service = std::make_unique<MockAttestationService>();
    mock_attestation_service_ = mock_attestation_service.get();

    auto mock_signals_service = std::make_unique<MockSignalsService>();
    mock_signals_service_ = mock_signals_service.get();

    return std::make_unique<DeviceTrustService>(
        std::move(mock_attestation_service), std::move(mock_signals_service),
        connector_.get());
  }

  bool is_attestation_flow_enabled() {
    return is_flag_enabled() && is_policy_enabled();
  }

  bool is_flag_enabled() { return std::get<0>(GetParam()); }
  bool is_policy_enabled() { return std::get<1>(GetParam()); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<DeviceTrustConnectorService> connector_;
  raw_ptr<MockAttestationService> mock_attestation_service_;
  raw_ptr<MockSignalsService> mock_signals_service_;
};

// Tests that IsEnabled returns true only when the feature flag is enabled and
// the policy has some URLs.
TEST_P(DeviceTrustServiceTest, IsEnabled) {
  auto device_trust_service = CreateService();
  EXPECT_EQ(is_attestation_flow_enabled(), device_trust_service->IsEnabled());
}

// Tests that the service kicks off the attestation flow properly.
TEST_P(DeviceTrustServiceTest, BuildChallengeResponse) {
  auto device_trust_service = CreateService();

  std::string fake_device_id = "fake_device_id";
  EXPECT_CALL(*mock_signals_service_, CollectSignals(_))
      .WillOnce(
          Invoke([&fake_device_id](
                     base::OnceCallback<void(std::unique_ptr<SignalsType>)>
                         signals_callback) {
            auto fake_signals = std::make_unique<SignalsType>();
            fake_signals->set_device_id(fake_device_id);
            std::move(signals_callback).Run(std::move(fake_signals));
          }));

  std::string fake_challenge = "fake_challenge";
  EXPECT_CALL(*mock_attestation_service_, BuildChallengeResponseForVAChallenge(
                                              fake_challenge, NotNull(), _))
      .WillOnce(Invoke([&fake_device_id](const std::string& challenge,
                                         std::unique_ptr<SignalsType> signals,
                                         AttestationCallback callback) {
        EXPECT_EQ(signals->device_id(), fake_device_id);
      }));

  device_trust_service->BuildChallengeResponse(
      fake_challenge,
      /*callback=*/base::BindOnce([](const std::string& response) {}));
}

INSTANTIATE_TEST_SUITE_P(All,
                         DeviceTrustServiceTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace enterprise_connectors
