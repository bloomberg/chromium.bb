// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_service.h"

#include <memory>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/attestation/mock_tpm_challenge_key.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/attestation/attestation_ca.pb.h"
#include "chromeos/dbus/attestation/attestation_client.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace enterprise_connectors {

namespace {

// A sample VerifiedAccess v2 challenge rerepsented as a JSON string.
constexpr char kJsonChallenge[] =
    "{"
    "\"challenge\": "
    "\"CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIELlPXqh8+"
    "rZJ2VIqwPXtPFrr653QdRrIzHFwqP+"
    "b3L8GJTcufirLxKAAkindNwTfwYUcbCFDjiW3kXdmDPE0wC0J6b5ZI6X6vOVcSMXTpK7nxsAGK"
    "zFV+i80LCnfwUZn7Ne1bHzloAqBdpLOu53vQ63hKRk6MRPhc9jYVDsvqXfQ7s+"
    "FUA5r3lxdoluxwAUMFqcP4VgnMvKzKTPYbnnB+xj5h5BZqjQToXJYoP4VC3/"
    "ID+YHNsCWy5o7+G5jnq0ak3zeqWfo1+lCibMPsCM+"
    "2g7nCZIwvwWlfoKwv3aKvOVMBcJxPAIxH1w+hH+"
    "NWxqRi6qgZm84q0ylm0ybs6TFjdgLvSViAIp0Z9p/An/"
    "u3W4CMboCswxIxNYRCGrIIVPElE3Yb4QS65mKrg=\""
    "}";

constexpr char kFakeResponse[] = "fake_response";

constexpr char kDeviceId[] = "device-id";
constexpr char kObfuscatedCustomerId[] = "customer-id";

absl::optional<std::string> ParseValueFromResponse(
    const std::string& response) {
  absl::optional<base::Value> data = base::JSONReader::Read(
      response, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  // If json is malformed or it doesn't include the needed field return
  // an empty string.
  if (!data || !data.value().FindPath("challengeResponse"))
    return absl::nullopt;

  std::string decoded_response_value;
  if (!base::Base64Decode(
          data.value().FindPath("challengeResponse")->GetString(),
          &decoded_response_value)) {
    return absl::nullopt;
  }

  return decoded_response_value;
}

ash::attestation::MockTpmChallengeKey* InjectMockChallengeKey() {
  auto mock_challenge_key =
      std::make_unique<ash::attestation::MockTpmChallengeKey>();
  ash::attestation::MockTpmChallengeKey* challenge_key_ptr =
      mock_challenge_key.get();
  ash::attestation::TpmChallengeKeyFactory::SetForTesting(
      std::move(mock_challenge_key));
  return challenge_key_ptr;
}

}  // namespace

class AshAttestationServiceTest : public testing::Test {
 protected:
  AshAttestationServiceTest() = default;

  void SetUp() override {
    testing::Test::SetUp();

    chromeos::AttestationClient::InitializeFake();

    mock_challenge_key_ = InjectMockChallengeKey();
    attestation_service_ =
        std::make_unique<AshAttestationService>(&test_profile_);
  }

  std::unique_ptr<attestation::DeviceTrustSignals> CreateSignals() {
    auto signals = std::make_unique<attestation::DeviceTrustSignals>();
    signals->set_device_id(kDeviceId);
    signals->set_obfuscated_customer_id(kObfuscatedCustomerId);
    return signals;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<AshAttestationService> attestation_service_;

  TestingProfile test_profile_;
  ash::attestation::MockTpmChallengeKey* mock_challenge_key_;
};

TEST_F(AshAttestationServiceTest, BuildChallengeResponse_Success) {
  auto signals = CreateSignals();

  base::RunLoop run_loop;
  auto callback =
      base::BindLambdaForTesting([&](const std::string& challenge_response) {
        ASSERT_FALSE(challenge_response.empty());
        auto parsed_value = ParseValueFromResponse(challenge_response);
        ASSERT_TRUE(parsed_value.has_value());
        EXPECT_EQ(kFakeResponse, parsed_value.value());
        run_loop.Quit();
      });

  auto protoChallenge = JsonChallengeToProtobufChallenge(kJsonChallenge);
  EXPECT_CALL(
      *mock_challenge_key_,
      BuildResponse(chromeos::attestation::AttestationKeyType::KEY_DEVICE,
                    /*profile=*/&test_profile_, /*callback=*/_,
                    /*challenge=*/protoChallenge,
                    /*register_key=*/false,
                    /*key_name_for_spkac=*/std::string(),
                    /*signals=*/_))
      .WillOnce(RunOnceCallback<2>(
          ash::attestation::TpmChallengeKeyResult::MakeChallengeResponse(
              kFakeResponse)));

  attestation_service_->BuildChallengeResponseForVAChallenge(
      kJsonChallenge, std::move(signals), std::move(callback));
  run_loop.Run();
}

}  // namespace enterprise_connectors
