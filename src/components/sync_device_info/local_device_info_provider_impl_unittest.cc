// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/local_device_info_provider_impl.h"

#include "base/test/mock_callback.h"
#include "components/sync/driver/sync_util.h"
#include "components/version_info/version_string.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

const char kLocalDeviceGuid[] = "foo";
const char kLocalDeviceSessionName[] = "bar";

using testing::NotNull;
using testing::Return;

class LocalDeviceInfoProviderImplTest : public testing::Test {
 public:
  LocalDeviceInfoProviderImplTest() {}
  ~LocalDeviceInfoProviderImplTest() override {}

  void SetUp() override {
    provider_ = std::make_unique<LocalDeviceInfoProviderImpl>(
        version_info::Channel::UNKNOWN,
        version_info::GetVersionStringWithModifier("UNKNOWN"),
        signin_scoped_device_id_callback_.Get(),
        send_tab_to_self_receiving_enabled_callback_.Get());
  }

  void TearDown() override { provider_.reset(); }

 protected:
  void InitializeProvider() { InitializeProvider(kLocalDeviceGuid); }

  void InitializeProvider(const std::string& guid) {
    provider_->Initialize(guid, kLocalDeviceSessionName);
  }

  testing::NiceMock<base::MockCallback<
      LocalDeviceInfoProviderImpl::SigninScopedDeviceIdCallback>>
      signin_scoped_device_id_callback_;
  testing::NiceMock<base::MockCallback<
      LocalDeviceInfoProviderImpl::SendTabToSelfReceivingEnabledCallback>>
      send_tab_to_self_receiving_enabled_callback_;
  std::unique_ptr<LocalDeviceInfoProviderImpl> provider_;
};

TEST_F(LocalDeviceInfoProviderImplTest, GetLocalDeviceInfo) {
  ASSERT_EQ(nullptr, provider_->GetLocalDeviceInfo());

  InitializeProvider();

  const DeviceInfo* local_device_info = provider_->GetLocalDeviceInfo();
  ASSERT_NE(nullptr, local_device_info);
  EXPECT_EQ(std::string(kLocalDeviceGuid), local_device_info->guid());
  EXPECT_EQ(kLocalDeviceSessionName, local_device_info->client_name());
  EXPECT_EQ(MakeUserAgentForSync(provider_->GetChannel()),
            local_device_info->sync_user_agent());

  provider_->Clear();
  ASSERT_EQ(nullptr, provider_->GetLocalDeviceInfo());
}

TEST_F(LocalDeviceInfoProviderImplTest, GetSigninScopedDeviceId) {
  const std::string kSigninScopedDeviceId = "device_id";

  EXPECT_CALL(signin_scoped_device_id_callback_, Run())
      .WillOnce(Return(kSigninScopedDeviceId));

  InitializeProvider();

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_EQ(kSigninScopedDeviceId,
            provider_->GetLocalDeviceInfo()->signin_scoped_device_id());
}

TEST_F(LocalDeviceInfoProviderImplTest, SendTabToSelfReceivingEnabled) {
  ON_CALL(send_tab_to_self_receiving_enabled_callback_, Run())
      .WillByDefault(Return(true));

  InitializeProvider();

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_TRUE(
      provider_->GetLocalDeviceInfo()->send_tab_to_self_receiving_enabled());

  ON_CALL(send_tab_to_self_receiving_enabled_callback_, Run())
      .WillByDefault(Return(false));

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_FALSE(
      provider_->GetLocalDeviceInfo()->send_tab_to_self_receiving_enabled());
}

}  // namespace
}  // namespace syncer
