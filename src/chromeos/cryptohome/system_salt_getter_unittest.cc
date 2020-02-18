// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/system_salt_getter.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

// Used as a GetSystemSaltCallback.
void CopySystemSalt(std::string* out_system_salt,
                    const std::string& system_salt) {
  *out_system_salt = system_salt;
}

class SystemSaltGetterTest : public testing::Test {
 protected:
  SystemSaltGetterTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    CryptohomeClient::InitializeFake();

    EXPECT_FALSE(SystemSaltGetter::IsInitialized());
    SystemSaltGetter::Initialize();
    ASSERT_TRUE(SystemSaltGetter::IsInitialized());
    ASSERT_TRUE(SystemSaltGetter::Get());
  }

  void TearDown() override {
    SystemSaltGetter::Shutdown();
    CryptohomeClient::Shutdown();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(SystemSaltGetterTest, GetSystemSalt) {
  // Try to get system salt before the service becomes available.
  FakeCryptohomeClient::Get()->SetServiceIsAvailable(false);
  std::string system_salt;
  SystemSaltGetter::Get()->GetSystemSalt(
      base::BindOnce(&CopySystemSalt, &system_salt));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(system_salt.empty());  // System salt is not returned yet.

  // Service becomes available.
  FakeCryptohomeClient::Get()->SetServiceIsAvailable(true);
  base::RunLoop().RunUntilIdle();
  const std::string expected_system_salt =
      SystemSaltGetter::ConvertRawSaltToHexString(
          FakeCryptohomeClient::GetStubSystemSalt());
  EXPECT_EQ(expected_system_salt, system_salt);  // System salt is returned.
}

}  // namespace
}  // namespace chromeos
