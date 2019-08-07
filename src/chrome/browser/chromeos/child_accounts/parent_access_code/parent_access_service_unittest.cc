// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/parent_access_code/parent_access_service.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/parent_access_code/policy_config_source.h"
#include "chrome/browser/chromeos/child_accounts/parent_access_code/test_utils.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_service_manager_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace parent_access {

// Stores information about results of the access code validation.
struct CodeValidationResults {
  // Number of successful access code validations.
  int success_count = 0;

  // Number of attempts whenaccess code validation failed.
  int failure_count = 0;
};

// Config source implementation used for tests.
class TestConfigSource : public ConfigSource {
 public:
  TestConfigSource() = default;
  ~TestConfigSource() override = default;

  ConfigSet GetConfigSet() override { return ConfigSet(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestConfigSource);
};

// ParentAccessServiceDelegate implementation used for tests.
class TestParentAccessServiceDelegate : public ParentAccessService::Delegate {
 public:
  TestParentAccessServiceDelegate() = default;
  ~TestParentAccessServiceDelegate() override = default;

  void OnAccessCodeValidation(bool result) override {
    result ? ++validation_results_.success_count
           : ++validation_results_.failure_count;
  }

  CodeValidationResults validation_results_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestParentAccessServiceDelegate);
};

// Tests ParentAccessService.
class ParentAccessServiceTest : public testing::Test {
 public:
  // Parent access code validation callback.
  void OnParentAccessCodeCallback(bool result) {
    result ? ++validation_results_.success_count
           : ++validation_results_.failure_count;
  }

 protected:
  ParentAccessServiceTest() = default;
  ~ParentAccessServiceTest() override = default;

  // testing::Test:
  void SetUp() override {
    DBusThreadManager::Initialize();

    auto config_source = std::make_unique<TestConfigSource>();
    parent_access_service_ =
        std::make_unique<parent_access::ParentAccessService>(
            std::move(config_source));

    test_delegate_ = std::make_unique<TestParentAccessServiceDelegate>();
    parent_access_service_->SetDelegate(test_delegate_.get());

    parent_access_service_->SetClockForTesting(&test_clock_);

    ASSERT_NO_FATAL_FAILURE(GetTestAccessCodeValues(&test_values_));
  }

  void TearDown() override {
    parent_access_service_->SetDelegate(nullptr);
    DBusThreadManager::Shutdown();
  }

  // Performs |code| validation on |parent_access_service_|.
  void ValidateAccessCode(const std::string& code) {
    parent_access_service_->ValidateParentAccessCode(
        code,
        base::BindOnce(&ParentAccessServiceTest::OnParentAccessCodeCallback,
                       base::Unretained(this)));
  }

  // Checks if ParentAccessServiceDelegate and ValidateParentAccessCodeCallback
  // were called as intended. Expects |success_count| of successful access code
  // validations and |failure_count| of failed validation attempts.
  void ExpectResults(int success_count, int failure_count) {
    EXPECT_EQ(success_count, test_delegate_->validation_results_.success_count);
    EXPECT_EQ(failure_count, test_delegate_->validation_results_.failure_count);
    EXPECT_EQ(success_count, validation_results_.success_count);
    EXPECT_EQ(failure_count, validation_results_.failure_count);
  }

  // ParentAccessService depends on LoginScreenClient and therefore requires
  // objects in the following block to be initialized early (order matters).
  content::TestBrowserThreadBundle thread_bundle_;
  content::TestServiceManagerContext context_;
  session_manager::SessionManager session_manager_;
  LoginScreenClient login_screen_client_;

  base::SimpleTestClock test_clock_;
  AccessCodeValues test_values_;
  CodeValidationResults validation_results_;

  std::unique_ptr<TestParentAccessServiceDelegate> test_delegate_;
  std::unique_ptr<ParentAccessService> parent_access_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ParentAccessServiceTest);
};

TEST_F(ParentAccessServiceTest, NoConfigAvailable) {
  auto test_value = test_values_.begin();
  test_clock_.SetNow(test_value->first);

  ValidateAccessCode(test_value->second);

  ExpectResults(0, 1);
}

TEST_F(ParentAccessServiceTest, NoValidConfigAvailable) {
  ConfigSource::ConfigSet configs;
  configs.future_config = GetInvalidTestConfig();
  configs.current_config = GetInvalidTestConfig();
  configs.old_configs.push_back(GetInvalidTestConfig());
  parent_access_service_->OnConfigChanged(configs);

  const AccessCodeValues::iterator test_value = test_values_.begin();
  test_clock_.SetNow(test_value->first);

  ValidateAccessCode(test_value->second);

  ExpectResults(0, 1);
}

TEST_F(ParentAccessServiceTest, ValidationWithFutureConfig) {
  ConfigSource::ConfigSet configs;
  configs.future_config = GetDefaultTestConfig();
  configs.current_config = GetInvalidTestConfig();
  configs.old_configs.push_back(GetInvalidTestConfig());
  parent_access_service_->OnConfigChanged(configs);

  const AccessCodeValues::iterator test_value = test_values_.begin();
  test_clock_.SetNow(test_value->first);

  ValidateAccessCode(test_value->second);

  ExpectResults(1, 0);
}

TEST_F(ParentAccessServiceTest, ValidationWithCurrentConfig) {
  ConfigSource::ConfigSet configs;
  configs.future_config = GetInvalidTestConfig();
  configs.current_config = GetDefaultTestConfig();
  configs.old_configs.push_back(GetInvalidTestConfig());
  parent_access_service_->OnConfigChanged(configs);

  const AccessCodeValues::iterator test_value = test_values_.begin();
  test_clock_.SetNow(test_value->first);

  ValidateAccessCode(test_value->second);

  ExpectResults(1, 0);
}

TEST_F(ParentAccessServiceTest, ValidationWithOldConfig) {
  ConfigSource::ConfigSet configs;
  configs.future_config = GetInvalidTestConfig();
  configs.current_config = GetInvalidTestConfig();
  configs.old_configs.push_back(GetInvalidTestConfig());
  configs.old_configs.push_back(GetDefaultTestConfig());
  parent_access_service_->OnConfigChanged(configs);

  const AccessCodeValues::iterator test_value = test_values_.begin();
  test_clock_.SetNow(test_value->first);

  ValidateAccessCode(test_value->second);

  ExpectResults(1, 0);
}

TEST_F(ParentAccessServiceTest, MultipleValidationAttempts) {
  AccessCodeValues::iterator test_value = test_values_.begin();
  test_clock_.SetNow(test_value->first);

  // No config - validation should fail.
  ValidateAccessCode(test_value->second);

  ConfigSource::ConfigSet configs;
  configs.current_config = GetDefaultTestConfig();
  parent_access_service_->OnConfigChanged(configs);

  // Valid config - validation should pass.
  for (auto& value : test_values_) {
    test_clock_.SetNow(value.first);
    ValidateAccessCode(value.second);
  }

  configs.current_config = GetInvalidTestConfig();
  parent_access_service_->OnConfigChanged(configs);

  // Invalid config - validation should fail.
  test_clock_.SetNow(test_value->first);
  ValidateAccessCode(test_value->second);

  ExpectResults(test_values_.size(), 2);
}

TEST_F(ParentAccessServiceTest, NoDelegate) {
  parent_access_service_->SetDelegate(nullptr);

  ConfigSource::ConfigSet configs;
  configs.current_config = GetDefaultTestConfig();
  parent_access_service_->OnConfigChanged(configs);

  AccessCodeValues::iterator test_value = test_values_.begin();
  test_clock_.SetNow(test_value->first);

  ValidateAccessCode(test_value->second);

  EXPECT_EQ(0, test_delegate_->validation_results_.success_count);
  EXPECT_EQ(0, test_delegate_->validation_results_.failure_count);
  EXPECT_EQ(1, validation_results_.success_count);
  EXPECT_EQ(0, validation_results_.failure_count);
}

}  // namespace parent_access
}  // namespace chromeos
