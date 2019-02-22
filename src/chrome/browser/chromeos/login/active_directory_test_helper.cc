// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/active_directory_test_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/dbus/auth_policy_client.h"
#include "chromeos/dbus/authpolicy/active_directory_info.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart_client.h"
#include "chromeos/dbus/util/tpm_util.h"
#include "chromeos/login/auth/authpolicy_login_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace active_directory_test_helper {

namespace {

constexpr char kAdMachineName[] = "ad-machine-name";
constexpr char kDmToken[] = "dm-token";

}  // namespace

void PrepareLogin(const std::string& user_principal_name) {
  std::vector<std::string> user_and_domain = base::SplitString(
      user_principal_name, "@", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, user_and_domain.size());

  // Start the D-Bus service.
  chromeos::DBusThreadManager::Get()
      ->GetUpstartClient()
      ->StartAuthPolicyService();

  // Join the AD domain.
  chromeos::AuthPolicyLoginHelper helper;
  {
    base::RunLoop loop;
    helper.set_dm_token(kDmToken);
    helper.JoinAdDomain(
        kAdMachineName, "" /* distinguished_name */,
        authpolicy::KerberosEncryptionTypes::ENC_TYPES_STRONG,
        user_principal_name, "" /* password */,
        base::BindOnce(
            [](base::OnceClosure closure, const std::string& expected_domain,
               authpolicy::ErrorType error, const std::string& domain) {
              EXPECT_EQ(authpolicy::ERROR_NONE, error);
              EXPECT_EQ(expected_domain, domain);
              std::move(closure).Run();
            },
            loop.QuitClosure(), user_and_domain[1]));
    loop.Run();
  }

  // Lock the device to AD mode. Paths need to be set here, so install attribs
  // can be saved to disk.
  OverridePaths();
  ASSERT_TRUE(
      tpm_util::LockDeviceActiveDirectoryForTesting(user_and_domain[1]));

  // Fetch device policy.
  {
    base::RunLoop run_loop;
    chromeos::DBusThreadManager::Get()
        ->GetAuthPolicyClient()
        ->RefreshDevicePolicy(base::BindOnce(
            [](base::OnceClosure quit_closure, authpolicy::ErrorType error) {
              EXPECT_EQ(authpolicy::ERROR_NONE, error);
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure()));
    run_loop.Run();
  }
}

void OverridePaths() {
  base::FilePath user_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  RegisterStubPathOverrides(user_data_dir);
}

}  // namespace active_directory_test_helper

}  // namespace chromeos
