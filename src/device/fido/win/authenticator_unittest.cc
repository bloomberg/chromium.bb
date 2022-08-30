// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/authenticator.h"

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

using GetCredentialCallbackReceiver =
    test::TestCallbackReceiver<std::vector<DiscoverableCredentialMetadata>,
                               bool>;

const std::vector<uint8_t> kCredentialId = {1, 2, 3, 4};
constexpr char kRpId[] = "project-altdeus.example.com";
const std::vector<uint8_t> kUserId = {5, 6, 7, 8};
constexpr char kUserName[] = "unit-aarc-noa";
constexpr char kUserDisplayName[] = "Noa";

class WinAuthenticatorTest : public testing::Test {
 public:
  void SetUp() override {
    fake_webauthn_api_ = std::make_unique<FakeWinWebAuthnApi>();
    fake_webauthn_api_->set_supports_silent_discovery(true);
    authenticator_ = std::make_unique<WinWebAuthnApiAuthenticator>(
        /*current_window=*/nullptr, fake_webauthn_api_.get());
  }

 protected:
  std::unique_ptr<FidoAuthenticator> authenticator_;
  std::unique_ptr<FakeWinWebAuthnApi> fake_webauthn_api_;
  base::test::TaskEnvironment task_environment;
};

// Tests getting credential information for an empty allow-list request that has
// valid credentials on a Windows version that supports silent discovery.
TEST_F(WinAuthenticatorTest,
       GetCredentialInformationForRequest_HasCredentials) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName,
                                     /*icon_url=*/absl::nullopt);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialCallbackReceiver callback;
  authenticator_->GetCredentialInformationForRequest(request,
                                                     callback.callback());
  callback.WaitForCallback();

  DiscoverableCredentialMetadata expected =
      DiscoverableCredentialMetadata(kCredentialId, user);
  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{expected});
  EXPECT_TRUE(std::get<1>(*callback.result()));
}

// Tests getting credential information for an empty allow-list request that
// does not have valid credentials on a Windows version that supports silent
// discovery.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_NoCredentials) {
  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialCallbackReceiver callback;
  authenticator_->GetCredentialInformationForRequest(request,
                                                     callback.callback());
  callback.WaitForCallback();

  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_TRUE(std::get<1>(*callback.result()));
}

// Tests the authenticator handling of an unexpected error from the Windows API.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_UnknownError) {
  fake_webauthn_api_->set_hresult(ERROR_NOT_SUPPORTED);
  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialCallbackReceiver callback;
  authenticator_->GetCredentialInformationForRequest(request,
                                                     callback.callback());
  callback.WaitForCallback();

  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_TRUE(std::get<1>(*callback.result()));
}

// Tests the authenticator handling of attempting to get credential information
// for a version of the Windows API that does not support silent discovery.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_Unsupported) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName,
                                     /*icon_url=*/absl::nullopt);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);
  fake_webauthn_api_->set_supports_silent_discovery(false);

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialCallbackReceiver callback;
  authenticator_->GetCredentialInformationForRequest(request,
                                                     callback.callback());
  callback.WaitForCallback();

  DiscoverableCredentialMetadata expected =
      DiscoverableCredentialMetadata(kCredentialId, user);
  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_TRUE(std::get<1>(*callback.result()));
}

// Tests that for non empty allow-list requests, the authenticator returns an
// empty credential list.
TEST_F(WinAuthenticatorTest,
       GetCredentialInformationForRequest_NonEmptyAllowList) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName,
                                     /*icon_url=*/absl::nullopt);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  request.allow_list.emplace_back(CredentialType::kPublicKey, kCredentialId);
  GetCredentialCallbackReceiver callback;
  authenticator_->GetCredentialInformationForRequest(request,
                                                     callback.callback());
  callback.WaitForCallback();

  DiscoverableCredentialMetadata expected =
      DiscoverableCredentialMetadata(kCredentialId, user);
  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_TRUE(std::get<1>(*callback.result()));
}

}  // namespace
}  // namespace device
