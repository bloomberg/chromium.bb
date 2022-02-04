// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the chromeos.quickUnlockPrivate extension API.

#include "chrome/browser/extensions/api/quick_unlock_private/quick_unlock_private_api.h"

#include <memory>

#include "ash/components/login/auth/fake_extended_authenticator.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_regular.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_function_dispatcher.h"

namespace extensions {
namespace {

namespace quick_unlock_private = api::quick_unlock_private;
using CredentialCheck = quick_unlock_private::CredentialCheck;
using CredentialProblem = quick_unlock_private::CredentialProblem;
using CredentialRequirements = quick_unlock_private::CredentialRequirements;
using QuickUnlockMode = quick_unlock_private::QuickUnlockMode;
using QuickUnlockModeList = std::vector<QuickUnlockMode>;
using CredentialList = std::vector<std::string>;

// The type of the test. Either based on Prefs or Cryptohome
enum class TestType { kPrefs, kCryptohome };

const base::StringPiece TestTypeStr(TestType type) {
  switch (type) {
    case TestType::kPrefs:
      return "PrefBased";
    case TestType::kCryptohome:
      return "CryptohomeBased";
  }
}

constexpr char kTestUserEmail[] = "testuser@gmail.com";
constexpr char kTestUserGaiaId[] = "9876543210";
constexpr char kTestUserEmailHash[] = "testuser@gmail.com-hash";
constexpr char kInvalidToken[] = "invalid";
constexpr char kValidPassword[] = "valid";
constexpr char kInvalidPassword[] = "invalid";

class FakeEasyUnlockService : public ash::EasyUnlockServiceRegular {
 public:
  FakeEasyUnlockService(
      Profile* profile,
      chromeos::device_sync::FakeDeviceSyncClient* fake_device_sync_client,
      chromeos::secure_channel::FakeSecureChannelClient*
          fake_secure_channel_client,
      chromeos::multidevice_setup::FakeMultiDeviceSetupClient*
          fake_multidevice_setup_client)
      : ash::EasyUnlockServiceRegular(profile,
                                      fake_secure_channel_client,
                                      fake_device_sync_client,
                                      fake_multidevice_setup_client) {}

  FakeEasyUnlockService(const FakeEasyUnlockService&) = delete;
  FakeEasyUnlockService& operator=(const FakeEasyUnlockService&) = delete;

  ~FakeEasyUnlockService() override {}

  // ash::EasyUnlockServiceRegular:
  void InitializeInternal() override {}
  void ShutdownInternal() override {}
};

std::unique_ptr<KeyedService> CreateEasyUnlockServiceForTest(
    content::BrowserContext* context) {
  static base::NoDestructor<chromeos::device_sync::FakeDeviceSyncClient>
      fake_device_sync_client;
  static base::NoDestructor<chromeos::secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client;
  static base::NoDestructor<
      chromeos::multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client;

  return std::make_unique<FakeEasyUnlockService>(
      Profile::FromBrowserContext(context), fake_device_sync_client.get(),
      fake_secure_channel_client.get(), fake_multidevice_setup_client.get());
}

ash::ExtendedAuthenticator* CreateFakeAuthenticator(
    ash::AuthStatusConsumer* auth_status_consumer) {
  const AccountId account_id =
      AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
  ash::UserContext expected_context(user_manager::USER_TYPE_REGULAR,
                                    account_id);
  expected_context.SetKey(ash::Key(kValidPassword));

  auto* authenticator = new ash::FakeExtendedAuthenticator(auth_status_consumer,
                                                           expected_context);
  return authenticator;
}

void FailIfCalled(const QuickUnlockModeList& modes) {
  FAIL();
}

enum ExpectedPinState {
  PIN_GOOD = 1 << 0,
  PIN_TOO_SHORT = 1 << 1,
  PIN_TOO_LONG = 1 << 2,
  PIN_WEAK_ERROR = 1 << 3,
  PIN_WEAK_WARNING = 1 << 4,
  PIN_CONTAINS_NONDIGIT = 1 << 5
};

}  // namespace

class QuickUnlockPrivateUnitTest
    : public ExtensionApiUnittest,
      public ::testing::WithParamInterface<std::tuple<TestType, bool>> {
 public:
  static std::string ParamInfoToString(
      testing::TestParamInfo<QuickUnlockPrivateUnitTest::ParamType> info) {
    return base::StrCat(
        {TestTypeStr(std::get<0>(info.param)),
         std::get<1>(info.param) ? "AutosubmitEnabled" : "AutosubmitDisabled"});
  }

  QuickUnlockPrivateUnitTest() = default;

  QuickUnlockPrivateUnitTest(const QuickUnlockPrivateUnitTest&) = delete;
  QuickUnlockPrivateUnitTest& operator=(const QuickUnlockPrivateUnitTest&) =
      delete;

  ~QuickUnlockPrivateUnitTest() override = default;

 protected:
  void SetUp() override {
    // Enable/disable PIN auto submit
    auto param = GetParam();
    feature_list_.InitWithFeatureState(ash::features::kQuickUnlockPinAutosubmit,
                                       std::get<1>(param));

    ash::CryptohomeMiscClient::InitializeFake();
    ash::UserDataAuthClient::InitializeFake();
    if (std::get<0>(param) == TestType::kCryptohome) {
      auto* cryptohome_client = ash::FakeUserDataAuthClient::Get();
      cryptohome_client->set_supports_low_entropy_credentials(true);
      cryptohome_client->set_enable_auth_check(true);
    }
    ash::SystemSaltGetter::Initialize();

    fake_user_manager_ = new ash::FakeChromeUserManager();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));

    ExtensionApiUnittest::SetUp();

    ash::SystemSaltGetter::Get()->SetRawSaltForTesting(
        {1, 2, 3, 4, 5, 6, 7, 8});
    fake_user_manager_->CreateLocalState();

    // Rebuild quick unlock state.
    ash::quick_unlock::EnabledForTesting(true);
    ash::quick_unlock::PinBackend::ResetForTesting();

    base::RunLoop().RunUntilIdle();

    modes_changed_handler_ = base::DoNothing();

    // Ensure that quick unlock is turned off.
    RunSetModes(QuickUnlockModeList{}, CredentialList{});
  }

  TestingProfile* CreateProfile() override {
    auto pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(pref_service->registry());
    test_pref_service_ = pref_service.get();

    TestingProfile* profile = profile_manager()->CreateTestingProfile(
        kTestUserEmail, std::move(pref_service), u"Test profile",
        1 /* avatar_id */, std::string() /* supervised_user_id */,
        GetTestingFactories());

    // Setup a primary user.
    auto test_account =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    fake_user_manager_->AddUser(test_account);
    fake_user_manager_->UserLoggedIn(test_account, kTestUserEmailHash, false,
                                     false);
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        fake_user_manager_->GetPrimaryUser(), profile);

    // Generate an auth token.
    auth_token_user_context_.SetAccountId(test_account);
    auth_token_user_context_.SetUserIDHash(kTestUserEmailHash);
    token_ = ash::quick_unlock::QuickUnlockFactory::GetForProfile(profile)
                 ->CreateAuthToken(auth_token_user_context_);
    base::RunLoop().RunUntilIdle();

    return profile;
  }

  void TearDown() override {
    ash::quick_unlock::EnabledForTesting(false);
    ash::quick_unlock::DisablePinByPolicyForTesting(false);

    base::RunLoop().RunUntilIdle();

    ExtensionApiUnittest::TearDown();

    fake_user_manager_ = nullptr;
    scoped_user_manager_.reset();

    ash::SystemSaltGetter::Shutdown();
    ash::UserDataAuthClient::Shutdown();
    ash::CryptohomeMiscClient::Shutdown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{ash::EasyUnlockServiceFactory::GetInstance(),
             base::BindRepeating(&CreateEasyUnlockServiceForTest)}};
  }

  // If a mode change event is raised, fail the test.
  void FailIfModesChanged() {
    modes_changed_handler_ = base::BindRepeating(&FailIfCalled);
  }

  // If a mode change event is raised, expect the given |modes|.
  void ExpectModesChanged(const QuickUnlockModeList& modes) {
    modes_changed_handler_ =
        base::BindRepeating(&QuickUnlockPrivateUnitTest::ExpectModeList,
                            base::Unretained(this), modes);
    expect_modes_changed_ = true;
  }

  // Wrapper for chrome.quickUnlockPrivate.getAuthToken. Expects the function
  // to succeed and returns the result.
  std::unique_ptr<quick_unlock_private::TokenInfo> GetAuthToken(
      const std::string& password) {
    // Setup a fake authenticator to avoid calling cryptohome methods.
    auto* func = new QuickUnlockPrivateGetAuthTokenFunction();
    func->SetAuthenticatorAllocatorForTesting(
        base::BindRepeating(&CreateFakeAuthenticator));

    auto params = std::make_unique<base::ListValue>();
    params->Append(base::Value(password));
    std::unique_ptr<base::Value> result = RunFunction(func, std::move(params));
    EXPECT_TRUE(result);
    auto token_info = quick_unlock_private::TokenInfo::FromValue(*result);
    EXPECT_TRUE(token_info);
    return token_info;
  }

  // Wrapper for chrome.quickUnlockPrivate.getAuthToken with an invalid
  // password. Expects the function to fail and returns the error.
  std::string RunAuthTokenWithInvalidPassword() {
    // Setup a fake authenticator to avoid calling cryptohome methods.
    auto* func = new QuickUnlockPrivateGetAuthTokenFunction();
    func->SetAuthenticatorAllocatorForTesting(
        base::BindRepeating(&CreateFakeAuthenticator));

    auto params = std::make_unique<base::ListValue>();
    params->Append(base::Value(kInvalidPassword));
    return RunFunctionAndReturnError(func, std::move(params));
  }

  // Wrapper for chrome.quickUnlockPrivate.setLockScreenEnabled.
  void SetLockScreenEnabled(const std::string& token, bool enabled) {
    auto params = std::make_unique<base::ListValue>();
    params->Append(token);
    params->Append(enabled);
    RunFunction(new QuickUnlockPrivateSetLockScreenEnabledFunction(),
                std::move(params));
  }

  // Wrapper for chrome.quickUnlockPrivate.setLockScreenEnabled.
  std::string SetLockScreenEnabledWithInvalidToken(bool enabled) {
    auto params = std::make_unique<base::ListValue>();
    params->Append(kInvalidToken);
    params->Append(enabled);
    return RunFunctionAndReturnError(
        new QuickUnlockPrivateSetLockScreenEnabledFunction(),
        std::move(params));
  }

  // Wrapper for chrome.quickUnlockPrivate.getAvailableModes.
  QuickUnlockModeList GetAvailableModes() {
    // Run the function.
    std::unique_ptr<base::Value> result =
        RunFunction(new QuickUnlockPrivateGetAvailableModesFunction(),
                    std::make_unique<base::ListValue>());

    // Extract the results.
    QuickUnlockModeList modes;

    EXPECT_TRUE(result->is_list());
    for (const base::Value& value : result->GetList()) {
      EXPECT_TRUE(value.is_string());
      modes.push_back(
          quick_unlock_private::ParseQuickUnlockMode(value.GetString()));
    }

    return modes;
  }

  // Wrapper for chrome.quickUnlockPrivate.getActiveModes.
  QuickUnlockModeList GetActiveModes() {
    std::unique_ptr<base::Value> result =
        RunFunction(new QuickUnlockPrivateGetActiveModesFunction(),
                    std::make_unique<base::ListValue>());

    QuickUnlockModeList modes;

    EXPECT_TRUE(result->is_list());
    for (const base::Value& value : result->GetList()) {
      EXPECT_TRUE(value.is_string());
      modes.push_back(
          quick_unlock_private::ParseQuickUnlockMode(value.GetString()));
    }

    return modes;
  }

  bool HasFlag(int outcome, int flag) { return (outcome & flag) != 0; }

  // Helper function for checking whether |IsCredentialUsableUsingPin| will
  // return the right message given a pin.
  void CheckPin(int expected_outcome, const std::string& pin) {
    CredentialCheck result = CheckCredentialUsingPin(pin);
    const std::vector<CredentialProblem> errors(result.errors);
    const std::vector<CredentialProblem> warnings(result.warnings);

    // A pin is considered good if it emits no errors or warnings.
    EXPECT_EQ(HasFlag(expected_outcome, PIN_GOOD),
              errors.empty() && warnings.empty());
    EXPECT_EQ(HasFlag(expected_outcome, PIN_TOO_SHORT),
              base::Contains(errors,
                             CredentialProblem::CREDENTIAL_PROBLEM_TOO_SHORT));
    EXPECT_EQ(
        HasFlag(expected_outcome, PIN_TOO_LONG),
        base::Contains(errors, CredentialProblem::CREDENTIAL_PROBLEM_TOO_LONG));
    EXPECT_EQ(HasFlag(expected_outcome, PIN_WEAK_WARNING),
              base::Contains(warnings,
                             CredentialProblem::CREDENTIAL_PROBLEM_TOO_WEAK));
    EXPECT_EQ(
        HasFlag(expected_outcome, PIN_WEAK_ERROR),
        base::Contains(errors, CredentialProblem::CREDENTIAL_PROBLEM_TOO_WEAK));
    EXPECT_EQ(
        HasFlag(expected_outcome, PIN_CONTAINS_NONDIGIT),
        base::Contains(
            errors, CredentialProblem::CREDENTIAL_PROBLEM_CONTAINS_NONDIGIT));
  }

  CredentialCheck CheckCredentialUsingPin(const std::string& pin) {
    auto params = std::make_unique<base::ListValue>();
    params->Append(ToString(QuickUnlockMode::QUICK_UNLOCK_MODE_PIN));
    params->Append(pin);

    std::unique_ptr<base::Value> result = RunFunction(
        new QuickUnlockPrivateCheckCredentialFunction(), std::move(params));

    CredentialCheck function_result;
    EXPECT_TRUE(CredentialCheck::Populate(*result, &function_result));
    return function_result;
  }

  void CheckGetCredentialRequirements(int expected_pin_min_length,
                                      int expected_pin_max_length) {
    auto params = std::make_unique<base::ListValue>();
    params->Append(ToString(QuickUnlockMode::QUICK_UNLOCK_MODE_PIN));

    std::unique_ptr<base::Value> result =
        RunFunction(new QuickUnlockPrivateGetCredentialRequirementsFunction(),
                    std::move(params));

    CredentialRequirements function_result;
    EXPECT_TRUE(CredentialRequirements::Populate(*result, &function_result));

    EXPECT_EQ(function_result.min_length, expected_pin_min_length);
    EXPECT_EQ(function_result.max_length, expected_pin_max_length);
  }

  std::unique_ptr<base::ListValue> GetSetModesParams(
      const std::string& token,
      const QuickUnlockModeList& modes,
      const CredentialList& passwords) {
    auto params = std::make_unique<base::ListValue>();
    params->Append(token);

    auto serialized_modes = std::make_unique<base::ListValue>();
    for (QuickUnlockMode mode : modes)
      serialized_modes->Append(quick_unlock_private::ToString(mode));
    params->Append(std::move(serialized_modes));

    auto serialized_passwords = std::make_unique<base::ListValue>();
    for (const std::string& password : passwords)
      serialized_passwords->Append(password);
    params->Append(std::move(serialized_passwords));

    return params;
  }

  // Runs chrome.quickUnlockPrivate.setModes using a valid token. Expects the
  // function to succeed.
  void RunSetModes(const QuickUnlockModeList& modes,
                   const CredentialList& passwords) {
    std::unique_ptr<base::ListValue> params =
        GetSetModesParams(token_, modes, passwords);
    auto* func = new QuickUnlockPrivateSetModesFunction();

    // Stub out event handling since we are not setting up an event router.
    func->SetModesChangedEventHandlerForTesting(modes_changed_handler_);

    // Run the function. Expect a non null result.
    RunFunction(func, std::move(params));

    // Verify that the mode change event handler was run if it was registered.
    // ExpectModesChanged will set expect_modes_changed_ to true and the event
    // handler will set it to false; so if the handler never runs,
    // expect_modes_changed_ will still be true.
    EXPECT_FALSE(expect_modes_changed_) << "Mode change event was not raised";
  }

  // Runs chrome.quickUnlockPrivate.setModes using an invalid token. Expects the
  // function to fail and returns the error.
  std::string RunSetModesWithInvalidToken() {
    std::unique_ptr<base::ListValue> params = GetSetModesParams(
        kInvalidToken, {QuickUnlockMode::QUICK_UNLOCK_MODE_PIN}, {"111111"});
    auto* func = new QuickUnlockPrivateSetModesFunction();

    // Stub out event handling since we are not setting up an event router.
    func->SetModesChangedEventHandlerForTesting(modes_changed_handler_);

    // Run function, expecting it to fail.
    return RunFunctionAndReturnError(func, std::move(params));
  }

  std::string SetModesWithError(const std::string& args) {
    auto* func = new QuickUnlockPrivateSetModesFunction();
    func->SetModesChangedEventHandlerForTesting(base::DoNothing());

    return api_test_utils::RunFunctionAndReturnError(func, args, profile());
  }

  std::string token() { return token_; }

  // Returns if the pin is set in the backend.
  bool IsPinSetInBackend() {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);

    bool called = false;
    bool is_set = false;
    ash::quick_unlock::PinBackend::GetInstance()->IsSet(
        account_id, base::BindOnce(
                        [](bool* out_called, bool* out_is_set, bool is_set) {
                          *out_called = true;
                          *out_is_set = is_set;
                        },
                        &called, &is_set));
    base::RunLoop().RunUntilIdle();
    CHECK(called);
    return is_set;
  }

  // Checks whether there is a user value set for the PIN auto submit
  // preference.
  bool HasUserValueForPinAutosubmitPref() {
    const bool has_user_val =
        test_pref_service_->GetUserPrefValue(
            ::prefs::kPinUnlockAutosubmitEnabled) != nullptr;
    return has_user_val;
  }

  bool GetAutosubmitPrefVal() {
    return test_pref_service_->GetBoolean(::prefs::kPinUnlockAutosubmitEnabled);
  }

  int GetExposedPinLength() {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    return user_manager::known_user::GetUserPinLength(account_id);
  }

  void ClearExposedPinLength() {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    user_manager::known_user::SetUserPinLength(account_id, 0);
  }

  bool IsBackfillNeeded() {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    return user_manager::known_user::PinAutosubmitIsBackfillNeeded(account_id);
  }

  void SetBackfillNotNeeded() {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    user_manager::known_user::PinAutosubmitSetBackfillNotNeeded(account_id);
  }

  void SetBackfillNeededForTests() {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    user_manager::known_user::PinAutosubmitSetBackfillNeededForTests(
        account_id);
  }

  void OnUpdateUserPods() {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    ash::quick_unlock::PinBackend::GetInstance()->GetExposedPinLength(
        account_id);
  }

  void SetPin(const std::string& pin) {
    RunSetModes(QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN},
                {pin});
  }

  void SetPinForBackfillTests(const std::string& pin) {
    // No PIN set. By default IsBackfillNeeded should return true.
    ASSERT_EQ(IsBackfillNeeded(), true);

    // Set PIN. Backfill must be marked as 'not needed' internally by the API.
    SetPin(pin);
    ASSERT_EQ(IsBackfillNeeded(), false);
    ASSERT_EQ(HasUserValueForPinAutosubmitPref(), false);

    // Set 'backfill needed' and clear the exposed length to simulate a PIN that
    // was set before the PIN auto submit feature existed.
    SetBackfillNeededForTests();
    ClearExposedPinLength();
    ASSERT_EQ(IsBackfillNeeded(), true);
    ASSERT_EQ(GetExposedPinLength(), 0);
  }

  void ClearPin() { RunSetModes(QuickUnlockModeList{}, CredentialList{}); }

  // Run an authentication attempt with the plain-text |password|.
  bool TryAuthenticate(const std::string& password) {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    bool called = false;
    bool success = false;
    base::RunLoop loop;
    ash::quick_unlock::PinBackend::GetInstance()->TryAuthenticate(
        account_id, ash::Key(password),
        base::BindLambdaForTesting([&](bool auth_success) {
          called = true;
          success = auth_success;
          loop.Quit();
        }));
    loop.Run();
    return success;
  }

  bool SetPinAutosubmitEnabled(const std::string& pin, const bool enabled) {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    bool called = false;
    bool success = false;
    base::RunLoop loop;
    ash::quick_unlock::PinBackend::GetInstance()->SetPinAutoSubmitEnabled(
        account_id, pin, enabled,
        base::BindLambdaForTesting([&](bool autosubmit_success) {
          called = true;
          success = autosubmit_success;
          loop.Quit();
        }));
    loop.Run();
    return success;
  }

  bool IsAutosubmitFeatureEnabled() { return std::get<1>(GetParam()); }

  base::test::ScopedFeatureList feature_list_;
  sync_preferences::TestingPrefServiceSyncable* test_pref_service_;

 private:
  // Runs the given |func| with the given |params|.
  std::unique_ptr<base::Value> RunFunction(
      scoped_refptr<ExtensionFunction> func,
      std::unique_ptr<base::ListValue> params) {
    base::RunLoop().RunUntilIdle();
    std::unique_ptr<base::Value> result =
        api_test_utils::RunFunctionWithDelegateAndReturnSingleResult(
            func, std::move(params),
            std::make_unique<ExtensionFunctionDispatcher>(profile()),
            api_test_utils::NONE);
    base::RunLoop().RunUntilIdle();
    return result;
  }

  // Runs |func| with |params|. Expects and returns an error result.
  std::string RunFunctionAndReturnError(
      scoped_refptr<ExtensionFunction> func,
      std::unique_ptr<base::ListValue> params) {
    base::RunLoop().RunUntilIdle();
    auto dispatcher = std::make_unique<ExtensionFunctionDispatcher>(profile());
    api_test_utils::RunFunction(func.get(), std::move(params),
                                std::move(dispatcher), api_test_utils::NONE);
    EXPECT_TRUE(func->GetResultList()->GetList().empty());
    base::RunLoop().RunUntilIdle();
    return func->GetError();
  }

  // Verifies a mode change event is raised and that |expected| is now the
  // active set of quick unlock modes.
  void ExpectModeList(const QuickUnlockModeList& expected,
                      const QuickUnlockModeList& actual) {
    EXPECT_EQ(expected, actual);
    expect_modes_changed_ = false;
  }

  ash::FakeChromeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  QuickUnlockPrivateSetModesFunction::ModesChangedEventHandler
      modes_changed_handler_;
  bool expect_modes_changed_ = false;
  ash::UserContext auth_token_user_context_;
  std::string token_;
};

// Verifies that GetAuthTokenValid succeeds when a valid password is provided.
TEST_P(QuickUnlockPrivateUnitTest, GetAuthTokenValid) {
  std::unique_ptr<quick_unlock_private::TokenInfo> token_info =
      GetAuthToken(kValidPassword);

  ash::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      ash::quick_unlock::QuickUnlockFactory::GetForProfile(profile());
  EXPECT_EQ(token_info->token,
            quick_unlock_storage->GetAuthToken()->Identifier());
  EXPECT_EQ(token_info->lifetime_seconds,
            ash::quick_unlock::AuthToken::kTokenExpirationSeconds);
}

// Verifies that GetAuthTokenValid fails when an invalid password is provided.
TEST_P(QuickUnlockPrivateUnitTest, GetAuthTokenInvalid) {
  std::string error = RunAuthTokenWithInvalidPassword();
  EXPECT_FALSE(error.empty());
}

// Verifies that setting lock screen enabled modifies the setting.
TEST_P(QuickUnlockPrivateUnitTest, SetLockScreenEnabled) {
  PrefService* pref_service = profile()->GetPrefs();
  bool lock_screen_enabled =
      pref_service->GetBoolean(ash::prefs::kEnableAutoScreenLock);

  SetLockScreenEnabled(token(), !lock_screen_enabled);

  EXPECT_EQ(!lock_screen_enabled,
            pref_service->GetBoolean(ash::prefs::kEnableAutoScreenLock));
}

// Verifies that setting lock screen enabled fails to modify the setting with
// an invalid token.
TEST_P(QuickUnlockPrivateUnitTest, SetLockScreenEnabledFailsWithInvalidToken) {
  PrefService* pref_service = profile()->GetPrefs();
  bool lock_screen_enabled =
      pref_service->GetBoolean(ash::prefs::kEnableAutoScreenLock);

  std::string error =
      SetLockScreenEnabledWithInvalidToken(!lock_screen_enabled);
  EXPECT_FALSE(error.empty());

  EXPECT_EQ(lock_screen_enabled,
            pref_service->GetBoolean(ash::prefs::kEnableAutoScreenLock));
}

// Verifies that this returns PIN for GetAvailableModes, unless it is blocked by
// policy.
TEST_P(QuickUnlockPrivateUnitTest, GetAvailableModes) {
  EXPECT_EQ(GetAvailableModes(),
            QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});

  ash::quick_unlock::DisablePinByPolicyForTesting(true);
  EXPECT_TRUE(GetAvailableModes().empty());
}

// Verfies that trying to set modes with a valid PIN failes when PIN is blocked
// by policy.
TEST_P(QuickUnlockPrivateUnitTest, SetModesForPinFailsWhenPinDisabledByPolicy) {
  ash::quick_unlock::DisablePinByPolicyForTesting(true);
  EXPECT_FALSE(SetModesWithError("[\"valid\", [\"PIN\"], [\"111\"]]").empty());
}

// Verifies that SetModes succeeds with a valid token.
TEST_P(QuickUnlockPrivateUnitTest, SetModes) {
  // Verify there is no active mode.
  EXPECT_EQ(GetActiveModes(), QuickUnlockModeList{});

  RunSetModes(QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN},
              {"111111"});
  EXPECT_EQ(GetActiveModes(),
            QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});
}

// Verifies that an invalid password cannot be used to update the mode list.
TEST_P(QuickUnlockPrivateUnitTest, SetModesFailsWithInvalidPassword) {
  // Verify there is no active mode.
  EXPECT_EQ(GetActiveModes(), QuickUnlockModeList{});

  // Try to enable PIN, but use an invalid password. Verify that no event is
  // raised and GetActiveModes still returns an empty set.
  FailIfModesChanged();
  std::string error = RunSetModesWithInvalidToken();
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(GetActiveModes(), QuickUnlockModeList{});
}

// Verifies that the quickUnlockPrivate.onActiveModesChanged is only raised when
// the active set of modes changes.
TEST_P(QuickUnlockPrivateUnitTest, ModeChangeEventOnlyRaisedWhenModesChange) {
  // Make sure quick unlock is turned off, and then verify that turning it off
  // again does not trigger an event.
  RunSetModes(QuickUnlockModeList{}, CredentialList{});
  FailIfModesChanged();
  RunSetModes(QuickUnlockModeList{}, CredentialList{});

  // Turn on PIN unlock, and then verify turning it on again and also changing
  // the password does not trigger an event.
  ExpectModesChanged(
      QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});
  RunSetModes(QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN},
              {"111111"});
  FailIfModesChanged();
  RunSetModes(QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN},
              {"222222"});
  RunSetModes(QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN},
              {""});
}

// Ensures that quick unlock can be enabled and disabled by checking the result
// of quickUnlockPrivate.GetActiveModes and PinStoragePrefs::IsPinSet.
TEST_P(QuickUnlockPrivateUnitTest, SetModesAndGetActiveModes) {
  // Update mode to PIN raises an event and updates GetActiveModes.
  ExpectModesChanged(
      QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});
  RunSetModes(QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN},
              {"111111"});
  EXPECT_EQ(GetActiveModes(),
            QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN});
  EXPECT_TRUE(IsPinSetInBackend());

  // SetModes can be used to turn off a quick unlock mode.
  ExpectModesChanged(QuickUnlockModeList{});
  RunSetModes(QuickUnlockModeList{}, CredentialList{});
  EXPECT_EQ(GetActiveModes(), QuickUnlockModeList{});
  EXPECT_FALSE(IsPinSetInBackend());
}

// Verifies that enabling PIN quick unlock actually talks to the PIN subsystem.
TEST_P(QuickUnlockPrivateUnitTest, VerifyAuthenticationAgainstPIN) {
  RunSetModes(QuickUnlockModeList{}, CredentialList{});
  EXPECT_FALSE(IsPinSetInBackend());

  RunSetModes(QuickUnlockModeList{QuickUnlockMode::QUICK_UNLOCK_MODE_PIN},
              {"111111"});
  EXPECT_TRUE(IsPinSetInBackend());

  EXPECT_FALSE(TryAuthenticate("000000"));
  EXPECT_TRUE(TryAuthenticate("111111"));
  EXPECT_FALSE(TryAuthenticate("000000"));
}

// Verifies that the number of modes and the number of passwords given must be
// the same.
TEST_P(QuickUnlockPrivateUnitTest, ThrowErrorOnMismatchedParameterCount) {
  EXPECT_FALSE(SetModesWithError("[\"valid\", [\"PIN\"], []]").empty());
  EXPECT_FALSE(SetModesWithError("[\"valid\", [], [\"11\"]]").empty());
}

// Validates PIN error checking in conjuction with policy-related prefs.
TEST_P(QuickUnlockPrivateUnitTest, CheckCredentialProblemReporting) {
  PrefService* pref_service = profile()->GetPrefs();

  // Verify the pin checks work with the default preferences which are minimum
  // length of 6, maximum length of 0 (no maximum) and no easy to guess check.
  CheckPin(PIN_GOOD, "111112");
  CheckPin(PIN_GOOD, "1111112");
  CheckPin(PIN_GOOD, "1111111111111112");
  CheckPin(PIN_WEAK_WARNING, "111111");
  CheckPin(PIN_TOO_SHORT, "1");
  CheckPin(PIN_TOO_SHORT, "11");
  CheckPin(PIN_TOO_SHORT | PIN_WEAK_WARNING, "111");
  CheckPin(PIN_TOO_SHORT | PIN_CONTAINS_NONDIGIT, "a");
  CheckPin(PIN_CONTAINS_NONDIGIT, "aaaaab");
  CheckPin(PIN_CONTAINS_NONDIGIT | PIN_WEAK_WARNING, "aaaaaa");
  CheckPin(PIN_CONTAINS_NONDIGIT | PIN_WEAK_WARNING, "abcdef");

  // Verify that now if the minimum length is set to 3, PINs of length 3 are
  // accepted.
  pref_service->SetInteger(ash::prefs::kPinUnlockMinimumLength, 3);
  CheckPin(PIN_WEAK_WARNING, "111");

  // Verify setting a nonzero maximum length that is less than the minimum
  // length results in the pin only accepting PINs of length minimum length.
  pref_service->SetInteger(ash::prefs::kPinUnlockMaximumLength, 2);
  pref_service->SetInteger(ash::prefs::kPinUnlockMinimumLength, 4);
  CheckPin(PIN_GOOD, "1112");
  CheckPin(PIN_TOO_SHORT, "112");
  CheckPin(PIN_TOO_LONG, "11112");

  // Verify that now if the maximum length is set to 5, PINs longer than 5 are
  // considered too long and cannot be used.
  pref_service->SetInteger(ash::prefs::kPinUnlockMaximumLength, 5);
  CheckPin(PIN_TOO_LONG | PIN_WEAK_WARNING, "111111");
  CheckPin(PIN_TOO_LONG | PIN_WEAK_WARNING, "1111111");

  // Verify that if both the minimum length and maximum length is set to 4, only
  // 4 digit PINs can be used.
  pref_service->SetInteger(ash::prefs::kPinUnlockMinimumLength, 4);
  pref_service->SetInteger(ash::prefs::kPinUnlockMaximumLength, 4);
  CheckPin(PIN_TOO_SHORT, "122");
  CheckPin(PIN_TOO_LONG, "12222");
  CheckPin(PIN_GOOD, "1222");

  // Set the PINs minimum/maximum lengths back to their defaults.
  pref_service->SetInteger(ash::prefs::kPinUnlockMinimumLength, 4);
  pref_service->SetInteger(ash::prefs::kPinUnlockMaximumLength, 0);

  // Verify that PINs that are weak are flagged as such. See
  // IsPinDifficultEnough in quick_unlock_private_api.cc for the description of
  // a weak pin.
  pref_service->SetBoolean(ash::prefs::kPinUnlockWeakPinsAllowed, false);
  // Good.
  CheckPin(PIN_GOOD, "1112");
  CheckPin(PIN_GOOD, "7890");
  CheckPin(PIN_GOOD, "0987");
  // Same digits.
  CheckPin(PIN_WEAK_ERROR, "1111");
  // Increasing.
  CheckPin(PIN_WEAK_ERROR, "0123");
  CheckPin(PIN_WEAK_ERROR, "3456789");
  // Decreasing.
  CheckPin(PIN_WEAK_ERROR, "3210");
  CheckPin(PIN_WEAK_ERROR, "987654");
  // Too common.
  CheckPin(PIN_WEAK_ERROR, "1212");

  // Verify that if a PIN has more than one error, both are returned.
  CheckPin(PIN_TOO_SHORT | PIN_WEAK_ERROR, "111");
  CheckPin(PIN_TOO_SHORT | PIN_WEAK_ERROR, "234");
}

TEST_P(QuickUnlockPrivateUnitTest, GetCredentialRequirements) {
  PrefService* pref_service = profile()->GetPrefs();

  // Verify that trying out PINs under the minimum/maximum lengths will send the
  // minimum/maximum lengths as additional information for display purposes.
  pref_service->SetInteger(ash::prefs::kPinUnlockMinimumLength, 6);
  pref_service->SetInteger(ash::prefs::kPinUnlockMaximumLength, 8);
  CheckGetCredentialRequirements(6, 8);

  // Verify that by setting a maximum length to be nonzero and smaller than the
  // minimum length, the resulting maxium length will be equal to the minimum
  // length pref.
  pref_service->SetInteger(ash::prefs::kPinUnlockMaximumLength, 4);
  CheckGetCredentialRequirements(6, 6);

  // Verify that the values received from policy are sanitized.
  pref_service->SetInteger(ash::prefs::kPinUnlockMinimumLength, -3);
  pref_service->SetInteger(ash::prefs::kPinUnlockMaximumLength, -3);
  CheckGetCredentialRequirements(1, 0);
}

// Enabling a PIN will by default enable auto submit, unless it is
// recommended/forced by policy to be disabled.
TEST_P(QuickUnlockPrivateUnitTest, PinAutosubmitLongestPossiblePin) {
  const bool feature_enabled = IsAutosubmitFeatureEnabled();
  SetPin("123456789012");
  EXPECT_TRUE(IsPinSetInBackend());
  EXPECT_EQ(GetAutosubmitPrefVal(), feature_enabled);
  EXPECT_EQ(GetExposedPinLength(), feature_enabled ? 12 : 0);
}

// When recommended to be disabled, PIN auto submit will not be enabled when
// setting a PIN.
TEST_P(QuickUnlockPrivateUnitTest, PinAutosubmitRecommendedDisabled) {
  test_pref_service_->SetRecommendedPref(::prefs::kPinUnlockAutosubmitEnabled,
                                         std::make_unique<base::Value>(false));

  SetPin("123456");
  EXPECT_TRUE(IsPinSetInBackend());
  EXPECT_FALSE(GetAutosubmitPrefVal());
  EXPECT_EQ(GetExposedPinLength(), 0);
}

// When forced to be disabled, PIN auto submit will not be enabled when
// setting a PIN.
TEST_P(QuickUnlockPrivateUnitTest, PinAutosubmitForcedDisabled) {
  test_pref_service_->SetManagedPref(::prefs::kPinUnlockAutosubmitEnabled,
                                     std::make_unique<base::Value>(false));

  SetPin("123456");
  EXPECT_TRUE(IsPinSetInBackend());
  EXPECT_FALSE(GetAutosubmitPrefVal());
  EXPECT_EQ(GetExposedPinLength(), 0);

  // Not possible to enable auto submit through Settings. The dialog
  // that makes this call cannot even be opened because its a mandatory pref.
  EXPECT_FALSE(SetPinAutosubmitEnabled("123456", true /*enabled*/));
  EXPECT_FALSE(GetAutosubmitPrefVal());
  EXPECT_EQ(GetExposedPinLength(), 0);
}

// Setting a PIN that is longer than 12 digits does not enable auto submit.
TEST_P(QuickUnlockPrivateUnitTest, PinAutosubmitLongPinIsNotExposed) {
  SetPin("1234567890123");  // 13 digits
  EXPECT_TRUE(IsPinSetInBackend());
  EXPECT_FALSE(GetAutosubmitPrefVal());
  EXPECT_EQ(GetExposedPinLength(), 0);
}

// When auto submit is enabled, it remains enabled when the PIN is changed
// and the exposed length is updated.
TEST_P(QuickUnlockPrivateUnitTest, PinAutosubmitOnSetAndUpdate) {
  const bool feature_enabled = IsAutosubmitFeatureEnabled();

  SetPin("123456");
  EXPECT_TRUE(IsPinSetInBackend());
  EXPECT_EQ(GetAutosubmitPrefVal(), feature_enabled);
  EXPECT_EQ(GetExposedPinLength(), feature_enabled ? 6 : 0);

  SetPin("12345678");
  EXPECT_EQ(GetAutosubmitPrefVal(), feature_enabled);
  EXPECT_EQ(GetExposedPinLength(), feature_enabled ? 8 : 0);
}

// When auto submit is disabled, it remains disabled when the PIN is changed
// and the exposed length remains zero.
TEST_P(QuickUnlockPrivateUnitTest, PinAutosubmitBehaviorWhenDisabled) {
  const bool feature_enabled = IsAutosubmitFeatureEnabled();

  SetPin("123456");
  EXPECT_TRUE(IsPinSetInBackend());
  EXPECT_EQ(GetAutosubmitPrefVal(), feature_enabled);
  EXPECT_EQ(GetExposedPinLength(), feature_enabled ? 6 : 0);

  // Disable auto submit
  EXPECT_EQ(SetPinAutosubmitEnabled("", false /*enabled*/), feature_enabled);
  EXPECT_TRUE(IsPinSetInBackend());
  EXPECT_EQ(HasUserValueForPinAutosubmitPref(), feature_enabled);
  EXPECT_FALSE(GetAutosubmitPrefVal());
  EXPECT_EQ(GetExposedPinLength(), 0);

  // Change to a different PIN
  SetPin("12345678");
  EXPECT_FALSE(GetAutosubmitPrefVal());
  EXPECT_EQ(GetExposedPinLength(), 0);
  EXPECT_EQ(HasUserValueForPinAutosubmitPref(), feature_enabled);
}

// Disabling PIN removes the user set value for auto submit and clears
// the exposed length.
TEST_P(QuickUnlockPrivateUnitTest, PinAutosubmitOnPinDisabled) {
  const bool feature_enabled = IsAutosubmitFeatureEnabled();

  SetPin("123456");
  EXPECT_TRUE(IsPinSetInBackend());
  EXPECT_EQ(GetAutosubmitPrefVal(), feature_enabled);
  EXPECT_EQ(GetExposedPinLength(), feature_enabled ? 6 : 0);

  // Disable PIN
  ClearPin();
  EXPECT_FALSE(IsPinSetInBackend());
  EXPECT_FALSE(HasUserValueForPinAutosubmitPref());
  EXPECT_EQ(GetExposedPinLength(), 0);
}

// If the user has no control over the preference, the pin length is collected
// upon a successful authentication attempt.
TEST_P(QuickUnlockPrivateUnitTest, PinAutosubmitCollectLengthOnAuthSuccess) {
  const bool feature_enabled = IsAutosubmitFeatureEnabled();

  // Start with MANDATORY FALSE to prevent auto enabling when setting a PIN.
  test_pref_service_->SetManagedPref(::prefs::kPinUnlockAutosubmitEnabled,
                                     std::make_unique<base::Value>(false));
  SetPin("123456");
  EXPECT_TRUE(IsPinSetInBackend());
  EXPECT_FALSE(GetAutosubmitPrefVal());
  EXPECT_EQ(GetExposedPinLength(), 0);

  // Autosubmit disabled, length unknown. Change to MANDATORY TRUE
  test_pref_service_->SetManagedPref(::prefs::kPinUnlockAutosubmitEnabled,
                                     std::make_unique<base::Value>(true));
  EXPECT_TRUE(GetAutosubmitPrefVal());
  EXPECT_EQ(GetExposedPinLength(), 0);

  // Try to authenticate with the wrong pin. Length won't be exposed.
  EXPECT_FALSE(TryAuthenticate("1234567"));
  EXPECT_EQ(GetExposedPinLength(), 0);

  // Authenticate with the correct pin. Length is exposed.
  EXPECT_TRUE(TryAuthenticate("123456"));
  EXPECT_EQ(GetExposedPinLength(), feature_enabled ? 6 : 0);
}

// If the user had PIN auto submit enabled and it was forced disabled via
// policy, the exposed length will be removed when the user pods are updated.
TEST_P(QuickUnlockPrivateUnitTest, PinAutosubmitClearLengthOnUiUpdate) {
  const bool feature_enabled = IsAutosubmitFeatureEnabled();

  SetPin("123456");
  EXPECT_TRUE(IsPinSetInBackend());
  EXPECT_EQ(GetAutosubmitPrefVal(), feature_enabled);
  EXPECT_EQ(GetExposedPinLength(), feature_enabled ? 6 : 0);

  // Switch to MANDATORY FALSE.
  test_pref_service_->SetManagedPref(::prefs::kPinUnlockAutosubmitEnabled,
                                     std::make_unique<base::Value>(false));

  // Called during user pod update.
  OnUpdateUserPods();

  // Exposed length must have been cleared.
  EXPECT_TRUE(IsPinSetInBackend());
  EXPECT_FALSE(GetAutosubmitPrefVal());
  EXPECT_EQ(GetExposedPinLength(), 0);
}

// Checks that the feature flag correctly prevents all actions.
TEST_P(QuickUnlockPrivateUnitTest, PinAutosubmitFeatureGuard) {
  const bool feature_enabled = IsAutosubmitFeatureEnabled();
  EXPECT_EQ(ash::features::IsPinAutosubmitFeatureEnabled(), feature_enabled);
}

// Tests that the backfill operation sets a user value for the auto submit pref
// for users who have set a PIN in a version of Chrome OS that did not support
// auto submit.
TEST_P(QuickUnlockPrivateUnitTest, PinAutosubmitBackfillDefaultPinLength) {
  base::HistogramTester histogram_tester;
  const bool feature_enabled = IsAutosubmitFeatureEnabled();
  if (!feature_enabled)
    return;

  SetPinForBackfillTests("123456");

  // A successful authentication attempt will backfill the user value.
  EXPECT_TRUE(TryAuthenticate("123456"));
  EXPECT_EQ(GetExposedPinLength(), 6);
  EXPECT_TRUE(HasUserValueForPinAutosubmitPref());
  EXPECT_TRUE(GetAutosubmitPrefVal());
  EXPECT_FALSE(IsBackfillNeeded());

  histogram_tester.ExpectUniqueSample(
      "Ash.Login.PinAutosubmit.Backfill",
      ash::quick_unlock::PinBackend::BackfillEvent::kEnabled, 1);
}

// No backfill operation if the PIN is longer than 6 digits.
TEST_P(QuickUnlockPrivateUnitTest, PinAutosubmitBackfillNonDefaultPinLength) {
  base::HistogramTester histogram_tester;
  const bool feature_enabled = IsAutosubmitFeatureEnabled();
  if (!feature_enabled)
    return;

  SetPinForBackfillTests("1234567");

  // A successful authentication attempt will backfill the user value to false.
  EXPECT_TRUE(TryAuthenticate("1234567"));
  EXPECT_EQ(GetExposedPinLength(), 0);
  EXPECT_TRUE(HasUserValueForPinAutosubmitPref());
  EXPECT_FALSE(GetAutosubmitPrefVal());
  EXPECT_FALSE(IsBackfillNeeded());

  histogram_tester.ExpectUniqueSample(
      "Ash.Login.PinAutosubmit.Backfill",
      ash::quick_unlock::PinBackend::BackfillEvent::kDisabledDueToPinLength, 1);
}

// Tests that the backfill operation sets a user value for the auto submit pref
// to false for enterprise users even with a default length of 6.
TEST_P(QuickUnlockPrivateUnitTest, PinAutosubmitBackfillEnterprise) {
  base::HistogramTester histogram_tester;
  const bool feature_enabled = IsAutosubmitFeatureEnabled();
  if (!feature_enabled)
    return;

  // Enterprise users have auto submit disabled by default.
  test_pref_service_->SetManagedPref(::prefs::kPinUnlockAutosubmitEnabled,
                                     std::make_unique<base::Value>(false));

  SetPinForBackfillTests("123456");

  // A successful authentication attempt will backfill the user value to false.
  EXPECT_TRUE(TryAuthenticate("123456"));
  EXPECT_EQ(GetExposedPinLength(), 0);
  EXPECT_TRUE(HasUserValueForPinAutosubmitPref());
  EXPECT_FALSE(GetAutosubmitPrefVal());
  EXPECT_FALSE(IsBackfillNeeded());

  histogram_tester.ExpectUniqueSample(
      "Ash.Login.PinAutosubmit.Backfill",
      ash::quick_unlock::PinBackend::BackfillEvent::kDisabledDueToPolicy, 1);
}

INSTANTIATE_TEST_SUITE_P(
    StorageProviders,
    QuickUnlockPrivateUnitTest,
    testing::Combine(testing::Values(TestType::kPrefs, TestType::kCryptohome),
                     testing::Bool()), /*autosubmit*/
    QuickUnlockPrivateUnitTest::ParamInfoToString);

}  // namespace extensions
