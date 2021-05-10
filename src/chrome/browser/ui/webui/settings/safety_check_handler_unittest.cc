// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

#include <string>
#include <unordered_map>

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/test_passwords_private_delegate.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/ui/webui/help/test_version_updater.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safety_check/test_update_check_helper.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_impl_win.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/chromeos/devicetype_utils.h"
#endif

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

// Components for building event strings.
constexpr char kParent[] = "parent";
constexpr char kUpdates[] = "updates";
constexpr char kPasswords[] = "passwords";
constexpr char kSafeBrowsing[] = "safe-browsing";
constexpr char kExtensions[] = "extensions";
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kChromeCleaner[] = "chrome-cleaner";
#endif

namespace {
using Enabled = base::StrongAlias<class EnabledTag, bool>;
using UserCanDisable = base::StrongAlias<class UserCanDisableTag, bool>;

class TestingSafetyCheckHandler : public SafetyCheckHandler {
 public:
  using SafetyCheckHandler::AllowJavascript;
  using SafetyCheckHandler::DisallowJavascript;
  using SafetyCheckHandler::set_web_ui;
  using SafetyCheckHandler::SetTimestampDelegateForTesting;
  using SafetyCheckHandler::SetVersionUpdaterForTesting;

  TestingSafetyCheckHandler(
      std::unique_ptr<safety_check::UpdateCheckHelper> update_helper,
      std::unique_ptr<VersionUpdater> version_updater,
      password_manager::BulkLeakCheckService* leak_service,
      extensions::PasswordsPrivateDelegate* passwords_delegate,
      extensions::ExtensionPrefs* extension_prefs,
      extensions::ExtensionServiceInterface* extension_service,
      std::unique_ptr<TimestampDelegate> timestamp_delegate)
      : SafetyCheckHandler(std::move(update_helper),
                           std::move(version_updater),
                           leak_service,
                           passwords_delegate,
                           extension_prefs,
                           extension_service,
                           std::move(timestamp_delegate)) {}
};

class TestDestructionVersionUpdater : public TestVersionUpdater {
 public:
  ~TestDestructionVersionUpdater() override { destructor_invoked_ = true; }

  void CheckForUpdate(StatusCallback callback, PromoteCallback) override {}

  static bool GetDestructorInvoked() { return destructor_invoked_; }

 private:
  static bool destructor_invoked_;
};

class TestTimestampDelegate : public TimestampDelegate {
 public:
  base::Time GetSystemTime() override {
    // 1 second before midnight Dec 31st 2020, so that -(24h-1s) is still on the
    // same day. This test time is hard coded to prevent DST flakiness, see
    // crbug.com/1066576.
    return base::Time::FromDoubleT(1609459199).LocalMidnight() -
           base::TimeDelta::FromSeconds(1);
  }
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  base::Time FetchChromeCleanerScanCompletionTimestamp() override {
    // 2 seconds before midnight Dec 31st 2020.
    return base::Time::FromDoubleT(1609459199).LocalMidnight() -
           base::TimeDelta::FromSeconds(2);
  }
#endif
};

bool TestDestructionVersionUpdater::destructor_invoked_ = false;

class TestPasswordsDelegate : public extensions::TestPasswordsPrivateDelegate {
 public:
  TestPasswordsDelegate() { store_->Init(/*prefs=*/nullptr); }

  void TearDown() {
    store_->ShutdownOnUIThread();
    // Needs to be invoked in the test's TearDown() - before the destructor.
    base::RunLoop().RunUntilIdle();
  }

  void SetBulkLeakCheckService(
      password_manager::BulkLeakCheckService* leak_service) {
    leak_service_ = leak_service;
  }

  void SetNumCompromisedCredentials(int compromised_password_count) {
    compromised_password_count_ = compromised_password_count;
  }

  void SetNumWeakCredentials(int weak_password_count) {
    weak_password_count_ = weak_password_count;
  }

  void SetPasswordCheckState(
      extensions::api::passwords_private::PasswordCheckState state) {
    state_ = state;
  }

  void SetProgress(int done, int total) {
    done_ = done;
    total_ = total;
  }

  void InvokeOnCompromisedCredentialsChanged() {
    // Compromised credentials can be added only after password form to which
    // they corresponds exists.
    password_manager::PasswordForm form;
    form.signon_realm = std::string("test.com");
    form.url = GURL("test.com");
    // Credentials have to be unique, so the callback is always invoked.
    form.username_value = base::ASCIIToUTF16(
        "test" + base::NumberToString(test_credential_counter_++));
    form.password_value = base::ASCIIToUTF16("password");
    form.username_element = base::ASCIIToUTF16("username_element");
    store_->AddLogin(form);
    base::RunLoop().RunUntilIdle();

    store_->AddInsecureCredential(password_manager::CompromisedCredentials(
        form.signon_realm, form.username_value, base::Time(),
        password_manager::InsecureType::kLeaked,
        password_manager::IsMuted(false)));
    base::RunLoop().RunUntilIdle();
  }

  std::vector<extensions::api::passwords_private::InsecureCredential>
  GetCompromisedCredentials() override {
    std::vector<extensions::api::passwords_private::InsecureCredential>
        compromised(compromised_password_count_);
    for (int i = 0; i < compromised_password_count_; ++i) {
      compromised[i].username = "test" + base::NumberToString(i);
    }
    return compromised;
  }

  std::vector<extensions::api::passwords_private::InsecureCredential>
  GetWeakCredentials() override {
    std::vector<extensions::api::passwords_private::InsecureCredential> weak(
        weak_password_count_);
    for (int i = 0; i < weak_password_count_; ++i) {
      weak[i].username = "test" + base::NumberToString(i);
    }
    return weak;
  }

  extensions::api::passwords_private::PasswordCheckStatus
  GetPasswordCheckStatus() override {
    extensions::api::passwords_private::PasswordCheckStatus status;
    status.state = state_;
    if (total_ != 0) {
      status.already_processed = std::make_unique<int>(done_);
      status.remaining_in_queue = std::make_unique<int>(total_ - done_);
    }
    return status;
  }

  password_manager::InsecureCredentialsManager* GetInsecureCredentialsManager()
      override {
    return &credentials_manager_;
  }

 private:
  password_manager::BulkLeakCheckService* leak_service_ = nullptr;
  int compromised_password_count_ = 0;
  int weak_password_count_ = 0;
  int done_ = 0;
  int total_ = 0;
  int test_credential_counter_ = 0;
  extensions::api::passwords_private::PasswordCheckState state_ =
      extensions::api::passwords_private::PASSWORD_CHECK_STATE_IDLE;
  scoped_refptr<password_manager::TestPasswordStore> store_ =
      base::MakeRefCounted<password_manager::TestPasswordStore>();
  password_manager::SavedPasswordsPresenter presenter_{store_};
  password_manager::InsecureCredentialsManager credentials_manager_{&presenter_,
                                                                    store_};
};

class TestSafetyCheckExtensionService : public TestExtensionService {
 public:
  void AddExtensionState(const std::string& extension_id,
                         Enabled enabled,
                         UserCanDisable user_can_disable) {
    state_map_.emplace(extension_id, ExtensionState{enabled.value(),
                                                    user_can_disable.value()});
  }

  bool IsExtensionEnabled(const std::string& extension_id) const override {
    auto it = state_map_.find(extension_id);
    if (it == state_map_.end()) {
      return false;
    }
    return it->second.enabled;
  }

  bool UserCanDisableInstalledExtension(
      const std::string& extension_id) override {
    auto it = state_map_.find(extension_id);
    if (it == state_map_.end()) {
      return false;
    }
    return it->second.user_can_disable;
  }

 private:
  struct ExtensionState {
    bool enabled;
    bool user_can_disable;
  };

  std::unordered_map<std::string, ExtensionState> state_map_;
};

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
class TestChromeCleanerControllerDelegate
    : public safe_browsing::ChromeCleanerControllerDelegate {
 public:
  bool IsAllowedByPolicy() override { return false; }
};
#endif

}  // namespace

class SafetyCheckHandlerTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

  // Returns a |base::DictionaryValue| for safety check status update that
  // has the specified |component| and |new_state| if it exists; nullptr
  // otherwise.
  const base::DictionaryValue* GetSafetyCheckStatusChangedWithDataIfExists(
      const std::string& component,
      int new_state);

  std::string GenerateExtensionId(char char_to_repeat);

  void VerifyDisplayString(const base::DictionaryValue* event,
                           const base::string16& expected);
  void VerifyDisplayString(const base::DictionaryValue* event,
                           const std::string& expected);

  // Replaces any instances of browser name (e.g. Google Chrome, Chromium,
  // etc) with "browser" to make sure tests work both on Chromium and
  // Google Chrome.
  void ReplaceBrowserName(base::string16* s);

 protected:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  safety_check::TestUpdateCheckHelper* update_helper_ = nullptr;
  TestVersionUpdater* version_updater_ = nullptr;
  std::unique_ptr<password_manager::BulkLeakCheckService> test_leak_service_;
  TestPasswordsDelegate test_passwords_delegate_;
  extensions::ExtensionPrefs* test_extension_prefs_ = nullptr;
  TestSafetyCheckExtensionService test_extension_service_;
  content::TestWebUI test_web_ui_;
  std::unique_ptr<TestingSafetyCheckHandler> safety_check_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  TestChromeCleanerControllerDelegate test_chrome_cleaner_controller_delegate_;
#endif
};

void SafetyCheckHandlerTest::SetUp() {
  feature_list_.InitWithFeatures({features::kSafetyCheckWeakPasswords}, {});

  TestingProfile::Builder builder;
  profile_ = builder.Build();

  web_contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(profile_.get()));

  // The unique pointer to a TestVersionUpdater gets moved to
  // SafetyCheckHandler, but a raw pointer is retained here to change its
  // state.
  auto update_helper = std::make_unique<safety_check::TestUpdateCheckHelper>();
  update_helper_ = update_helper.get();
  auto version_updater = std::make_unique<TestVersionUpdater>();
  version_updater_ = version_updater.get();
  test_leak_service_ = std::make_unique<password_manager::BulkLeakCheckService>(
      nullptr, nullptr);
  test_passwords_delegate_.SetBulkLeakCheckService(test_leak_service_.get());
  test_web_ui_.set_web_contents(web_contents_.get());
  test_extension_prefs_ = extensions::ExtensionPrefs::Get(profile_.get());
  auto timestamp_delegate = std::make_unique<TestTimestampDelegate>();
  safety_check_ = std::make_unique<TestingSafetyCheckHandler>(
      std::move(update_helper), std::move(version_updater),
      test_leak_service_.get(), &test_passwords_delegate_,
      test_extension_prefs_, &test_extension_service_,
      std::move(timestamp_delegate));
  test_web_ui_.ClearTrackedCalls();
  safety_check_->set_web_ui(&test_web_ui_);
  safety_check_->AllowJavascript();

  browser_task_environment_.RunUntilIdle();
}

void SafetyCheckHandlerTest::TearDown() {
  test_passwords_delegate_.TearDown();
}

const base::DictionaryValue*
SafetyCheckHandlerTest::GetSafetyCheckStatusChangedWithDataIfExists(
    const std::string& component,
    int new_state) {
  // Return the latest update if multiple, so iterate from the end.
  const std::vector<std::unique_ptr<content::TestWebUI::CallData>>& call_data =
      test_web_ui_.call_data();
  for (int i = call_data.size() - 1; i >= 0; --i) {
    const content::TestWebUI::CallData& data = *(call_data[i]);
    if (data.function_name() != "cr.webUIListenerCallback") {
      continue;
    }
    std::string event;
    if ((!data.arg1()->GetAsString(&event)) ||
        event != "safety-check-" + component + "-status-changed") {
      continue;
    }
    const base::DictionaryValue* dictionary = nullptr;
    if (!data.arg2()->GetAsDictionary(&dictionary)) {
      continue;
    }
    int cur_new_state;
    if (dictionary->GetInteger("newState", &cur_new_state) &&
        cur_new_state == new_state) {
      return dictionary;
    }
  }
  return nullptr;
}

std::string SafetyCheckHandlerTest::GenerateExtensionId(char char_to_repeat) {
  return std::string(crx_file::id_util::kIdSize * 2, char_to_repeat);
}

void SafetyCheckHandlerTest::VerifyDisplayString(
    const base::DictionaryValue* event,
    const base::string16& expected) {
  base::string16 display;
  ASSERT_TRUE(event->GetString("displayString", &display));
  ReplaceBrowserName(&display);
  // Need to also replace any instances of Chrome and Chromium in the
  // expected string due to an edge case on ChromeOS, where a device name
  // is "Chrome", which gets replaced in the display string.
  base::string16 expected_replaced = expected;
  ReplaceBrowserName(&expected_replaced);
  EXPECT_EQ(expected_replaced, display);
}

void SafetyCheckHandlerTest::VerifyDisplayString(
    const base::DictionaryValue* event,
    const std::string& expected) {
  VerifyDisplayString(event, base::ASCIIToUTF16(expected));
}

void SafetyCheckHandlerTest::ReplaceBrowserName(base::string16* s) {
  base::ReplaceSubstringsAfterOffset(s, 0, base::ASCIIToUTF16("Google Chrome"),
                                     base::ASCIIToUTF16("Browser"));
  base::ReplaceSubstringsAfterOffset(s, 0, base::ASCIIToUTF16("Chrome"),
                                     base::ASCIIToUTF16("Browser"));
  base::ReplaceSubstringsAfterOffset(s, 0, base::ASCIIToUTF16("Chromium"),
                                     base::ASCIIToUTF16("Browser"));
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Checking) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::CHECKING);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kChecking));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, base::UTF8ToUTF16(""));
  // Checking state should not get recorded.
  histogram_tester_.ExpectTotalCount("Settings.SafetyCheck.UpdatesResult", 0);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Updated) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::UPDATED);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kUpdated));
  ASSERT_TRUE(event);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::string16 expected = base::ASCIIToUTF16("Your ") +
                            ui::GetChromeOSDeviceName() +
                            base::ASCIIToUTF16(" is up to date");
  VerifyDisplayString(event, expected);
#else
  VerifyDisplayString(event, "Browser is up to date");
#endif
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kUpdated, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Updating) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::UPDATING);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kUpdating));
  ASSERT_TRUE(event);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  VerifyDisplayString(event, "Updating your device");
#else
  VerifyDisplayString(event, "Updating Browser");
#endif
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kUpdating, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Relaunch) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::NEARLY_UPDATED);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kRelaunch));
  ASSERT_TRUE(event);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  VerifyDisplayString(
      event, "Nearly up to date! Restart your device to finish updating.");
#else
  VerifyDisplayString(event,
                      "Nearly up to date! Relaunch Browser to finish "
                      "updating. Incognito windows won't reopen.");
#endif
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kRelaunch, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Disabled) {
  const char* processor_variation = nullptr;
#if defined(OS_MAC)
  switch (base::mac::GetCPUType()) {
    case base::mac::CPUType::kIntel:
      processor_variation = " (x86_64)";
      break;
    case base::mac::CPUType::kTranslatedIntel:
      processor_variation = " (x86_64 translated)";
      break;
    case base::mac::CPUType::kArm:
      processor_variation = " (arm64)";
      break;
  }
#elif defined(ARCH_CPU_64_BITS)
  processor_variation = " (64-bit)";
#elif defined(ARCH_CPU_32_BITS)
  processor_variation = " (32-bit)";
#else
#error Update for a processor that is neither 32-bit nor 64-bit.
#endif  // OS_*

  version_updater_->SetReturnedStatus(VersionUpdater::Status::DISABLED);
  safety_check_->PerformSafetyCheck();
  // TODO(crbug/1072432): Since the UNKNOWN state is not present in JS in M83,
  // use FAILED_OFFLINE, which uses the same icon.
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailedOffline));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event, "Version " + version_info::GetVersionNumber() + " (" +
                 (version_info::IsOfficialBuild() ? "Official Build"
                                                  : "Developer Build") +
                 ") " + chrome::GetChannelName() + processor_variation);
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kUnknown, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_DisabledByAdmin) {
  version_updater_->SetReturnedStatus(
      VersionUpdater::Status::DISABLED_BY_ADMIN);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kDisabledByAdmin));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "Updates are managed by <a target=\"_blank\" "
      "href=\"https://support.google.com/chrome?p=your_administrator\">your "
      "administrator</a>");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kDisabledByAdmin, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_FailedOffline) {
  version_updater_->SetReturnedStatus(VersionUpdater::Status::FAILED_OFFLINE);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailedOffline));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check for updates. Try checking your "
                      "internet connection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kFailedOffline, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Failed_ConnectivityOnline) {
  update_helper_->SetConnectivity(true);
  version_updater_->SetReturnedStatus(VersionUpdater::Status::FAILED);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailed));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "Browser didn't update, something went wrong. <a target=\"_blank\" "
      "href=\"https://support.google.com/chrome?p=fix_chrome_updates\">Fix "
      "Browser update problems and failed updates.</a>");
  histogram_tester_.ExpectBucketCount("Settings.SafetyCheck.UpdatesResult",
                                      SafetyCheckHandler::UpdateStatus::kFailed,
                                      1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_Failed_ConnectivityOffline) {
  update_helper_->SetConnectivity(false);
  version_updater_->SetReturnedStatus(VersionUpdater::Status::FAILED);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kFailedOffline));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check for updates. Try checking your "
                      "internet connection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.UpdatesResult",
      SafetyCheckHandler::UpdateStatus::kFailedOffline, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckUpdates_DestroyedOnJavascriptDisallowed) {
  EXPECT_FALSE(TestDestructionVersionUpdater::GetDestructorInvoked());
  safety_check_->SetVersionUpdaterForTesting(
      std::make_unique<TestDestructionVersionUpdater>());
  safety_check_->PerformSafetyCheck();
  safety_check_->DisallowJavascript();
  EXPECT_TRUE(TestDestructionVersionUpdater::GetDestructorInvoked());
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_EnabledStandard) {
  TestingProfile::FromWebUI(&test_web_ui_)
      ->AsTestingProfile()
      ->GetTestingPrefService()
      ->SetManagedPref(prefs::kSafeBrowsingEnabled,
                       std::make_unique<base::Value>(true));
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(
              SafetyCheckHandler::SafeBrowsingStatus::kEnabledStandard));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "Standard Protection is on");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kEnabledStandard, 1);
}

TEST_F(SafetyCheckHandlerTest,
       CheckSafeBrowsing_EnabledStandardAvailableEnhanced) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::
                               kEnabledStandardAvailableEnhanced));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Standard protection is on. For even more security, use "
                      "enhanced protection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kEnabledStandardAvailableEnhanced,
      1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_EnabledEnhanced) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(
              SafetyCheckHandler::SafeBrowsingStatus::kEnabledEnhanced));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "Enhanced Protection is on");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kEnabledEnhanced, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_InconsistentEnhanced) {
  // Tests the corner case of SafeBrowsingEnhanced pref being on, while
  // SafeBrowsing enabled is off. This should be treated as SB off.
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::kDisabled));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event, "Safe Browsing is off. Browser recommends turning it on.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kDisabled, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_Disabled) {
  Profile::FromWebUI(&test_web_ui_)
      ->GetPrefs()
      ->SetBoolean(prefs::kSafeBrowsingEnabled, false);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::kDisabled));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event, "Safe Browsing is off. Browser recommends turning it on.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kDisabled, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_DisabledByAdmin) {
  TestingProfile::FromWebUI(&test_web_ui_)
      ->AsTestingProfile()
      ->GetTestingPrefService()
      ->SetManagedPref(prefs::kSafeBrowsingEnabled,
                       std::make_unique<base::Value>(false));
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(
              SafetyCheckHandler::SafeBrowsingStatus::kDisabledByAdmin));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "<a target=\"_blank\" "
      "href=\"https://support.google.com/chrome?p=your_administrator\">Your "
      "administrator</a> has turned off Safe Browsing");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kDisabledByAdmin, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckSafeBrowsing_DisabledByExtension) {
  TestingProfile::FromWebUI(&test_web_ui_)
      ->AsTestingProfile()
      ->GetTestingPrefService()
      ->SetExtensionPref(prefs::kSafeBrowsingEnabled,
                         std::make_unique<base::Value>(false));
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(
              SafetyCheckHandler::SafeBrowsingStatus::kDisabledByExtension));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "An extension has turned off Safe Browsing");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.SafeBrowsingResult",
      SafetyCheckHandler::SafeBrowsingStatus::kDisabledByExtension, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_ObserverRemovedAfterError) {
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, base::UTF8ToUTF16(""));
  histogram_tester_.ExpectTotalCount("Settings.SafetyCheck.PasswordsResult", 0);
  // Second, an "offline" state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kNetworkError);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kOffline));
  ASSERT_TRUE(event2);
  VerifyDisplayString(event2,
                      "Browser can't check your passwords. Try checking your "
                      "internet connection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kOffline, 1);
  // Another error, but since the previous state is terminal, the handler
  // should no longer be observing the BulkLeakCheckService state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kServiceError);
  const base::DictionaryValue* event3 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kOffline));
  ASSERT_TRUE(event3);
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kOffline, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_InterruptedAndRefreshed) {
  safety_check_->PerformSafetyCheck();
  // Password check running.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, base::UTF8ToUTF16(""));
  // The check gets interrupted and the page is refreshed.
  safety_check_->DisallowJavascript();
  safety_check_->AllowJavascript();
  // Need to set the |TestVersionUpdater| instance again to prevent
  // |PerformSafetyCheck()| from creating a real |VersionUpdater| instance.
  safety_check_->SetVersionUpdaterForTesting(
      std::make_unique<TestVersionUpdater>());
  // Another run of the safety check.
  safety_check_->PerformSafetyCheck();
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event2);
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kSignedOut);
  const base::DictionaryValue* event3 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSignedOut));
  ASSERT_TRUE(event3);
  VerifyDisplayString(event3,
                      "Browser can't check your passwords because you're not "
                      "signed in");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kSignedOut, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_StartedTwice) {
  safety_check_->PerformSafetyCheck();
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event);
  // Then, a network error.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kNetworkError);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kOffline));
  EXPECT_TRUE(event2);
  VerifyDisplayString(event2,
                      "Browser can't check your passwords. Try checking your "
                      "internet connection.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kOffline, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_ObserverNotifiedTwice) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(test_passwords_delegate_.StartPasswordCheckTriggered());
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(
          password_manager::BulkLeakCheckService::State::kServiceError);
  // Another notification about the same state change.
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(
          password_manager::BulkLeakCheckService::State::kServiceError);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kError));
  ASSERT_TRUE(event);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_Safe) {
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Second, a "safe" state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSafe));
  EXPECT_TRUE(event);
  VerifyDisplayString(event, "No compromised passwords found");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kSafe, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_StaleSafeThenCompromised) {
  constexpr int kCompromised = 7;
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  test_passwords_delegate_.SetPasswordCheckState(
      extensions::api::passwords_private::PASSWORD_CHECK_STATE_RUNNING);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Not a "safe" state, so send an |OnCredentialDone| with is_leaked=true.
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnCredentialDone(
          {base::ASCIIToUTF16("login"), base::ASCIIToUTF16("password")},
          password_manager::IsLeaked(true));
  // The service goes idle, but the disk still has a stale "safe" state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  test_passwords_delegate_.SetPasswordCheckState(
      extensions::api::passwords_private::PASSWORD_CHECK_STATE_IDLE);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSafe));
  EXPECT_TRUE(event);
  // An InsecureCredentialsManager callback fires once the compromised passwords
  // get written to disk.
  test_passwords_delegate_.SetNumCompromisedCredentials(kCompromised);
  test_passwords_delegate_.InvokeOnCompromisedCredentialsChanged();
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(
              SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  EXPECT_TRUE(event2);
  VerifyDisplayString(
      event2, base::NumberToString(kCompromised) + " compromised passwords");
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_SafeStateThenMoreEvents) {
  safety_check_->PerformSafetyCheck();
  // Running state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  test_passwords_delegate_.SetPasswordCheckState(
      extensions::api::passwords_private::PASSWORD_CHECK_STATE_RUNNING);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));

  // Previous safe state got loaded.
  test_passwords_delegate_.SetNumCompromisedCredentials(0);
  test_passwords_delegate_.InvokeOnCompromisedCredentialsChanged();
  // The event should get ignored, since the state is still running.
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSafe));
  EXPECT_FALSE(event);

  // The check is completed with another safe state.
  test_passwords_delegate_.SetPasswordCheckState(
      extensions::api::passwords_private::PASSWORD_CHECK_STATE_IDLE);
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  // This time the safe state should be reflected.
  event = GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords, static_cast<int>(SafetyCheckHandler::PasswordsStatus::kSafe));
  EXPECT_TRUE(event);

  // After some time, some compromises were discovered (unrelated to SC).
  constexpr int kCompromised = 7;
  test_passwords_delegate_.SetNumCompromisedCredentials(kCompromised);
  test_passwords_delegate_.InvokeOnCompromisedCredentialsChanged();
  // The new event should get ignored, since the safe state was final.
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(
              SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  EXPECT_FALSE(event2);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_OnlyCompromisedExist) {
  constexpr int kCompromised = 7;
  test_passwords_delegate_.SetNumCompromisedCredentials(kCompromised);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(
              SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(
      event2, base::NumberToString(kCompromised) + " compromised passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kCompromisedExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_CompromisedAndWeakExist) {
  constexpr int kCompromised = 7;
  constexpr int kWeak = 13;
  test_passwords_delegate_.SetNumCompromisedCredentials(kCompromised);
  test_passwords_delegate_.SetNumWeakCredentials(kWeak);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(
              SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(
      event2, base::NumberToString(kCompromised) + " compromised passwords, " +
                  base::NumberToString(kWeak) + " weak passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kCompromisedExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_OnlyWeakExist) {
  constexpr int kWeak = 13;
  test_passwords_delegate_.SetNumWeakCredentials(kWeak);
  safety_check_->PerformSafetyCheck();
  // First, a "running" change of state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kPasswords,
      static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking)));
  // Compromised passwords found state.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kIdle);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(
              SafetyCheckHandler::PasswordsStatus::kWeakPasswordsExist));
  ASSERT_TRUE(event2);
  VerifyDisplayString(event2, base::NumberToString(kWeak) + " weak passwords");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kWeakPasswordsExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_Error) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(test_passwords_delegate_.StartPasswordCheckTriggered());
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(
          password_manager::BulkLeakCheckService::State::kServiceError);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kError));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check your passwords. Try again "
                      "later.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kError, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_Error_FutureEventsIgnored) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(test_passwords_delegate_.StartPasswordCheckTriggered());
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(
          password_manager::BulkLeakCheckService::State::kServiceError);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kError));
  ASSERT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check your passwords. Try again "
                      "later.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kError, 1);
  // At some point later, the service discovers compromised passwords and goes
  // idle.
  constexpr int kCompromised = 7;
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(password_manager::BulkLeakCheckService::State::kRunning);
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(password_manager::BulkLeakCheckService::State::kIdle);
  // An InsecureCredentialsManager callback fires once the compromised passwords
  // get written to disk.
  test_passwords_delegate_.SetNumCompromisedCredentials(kCompromised);
  test_passwords_delegate_.InvokeOnCompromisedCredentialsChanged();
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(
              SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  // The event for compromised passwords should not exist, since the changes
  // should no longer be observed.
  EXPECT_FALSE(event2);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_FeatureUnavailable) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(test_passwords_delegate_.StartPasswordCheckTriggered());
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(
          password_manager::BulkLeakCheckService::State::kTokenRequestFailure);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(
              SafetyCheckHandler::PasswordsStatus::kFeatureUnavailable));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "Password check is not available in Chromium");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kFeatureUnavailable, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_RunningOneCompromised) {
  test_passwords_delegate_.SetNumCompromisedCredentials(1);
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(test_passwords_delegate_.StartPasswordCheckTriggered());
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnStateChanged(password_manager::BulkLeakCheckService::State::kIdle);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(
              SafetyCheckHandler::PasswordsStatus::kCompromisedExist));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, "1 compromised password");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kCompromisedExist, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_NoPasswords) {
  test_passwords_delegate_.ClearSavedPasswordsList();
  test_passwords_delegate_.SetStartPasswordCheckState(
      password_manager::BulkLeakCheckService::State::kIdle);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kNoPasswords));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "No saved passwords. Chrome can check your passwords "
                      "when you save them.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.PasswordsResult",
      SafetyCheckHandler::PasswordsStatus::kNoPasswords, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckPasswords_Progress) {
  auto credential = password_manager::LeakCheckCredential(
      base::UTF8ToUTF16("test"), base::UTF8ToUTF16("test"));
  auto is_leaked = password_manager::IsLeaked(false);
  safety_check_->PerformSafetyCheck();
  test_passwords_delegate_.SetPasswordCheckState(
      extensions::api::passwords_private::PASSWORD_CHECK_STATE_RUNNING);
  test_passwords_delegate_.SetProgress(1, 3);
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnCredentialDone(credential, is_leaked);
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  EXPECT_TRUE(event);
  VerifyDisplayString(event, base::UTF8ToUTF16("Checking passwords (1 of 3)…"));

  test_passwords_delegate_.SetProgress(2, 3);
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnCredentialDone(credential, is_leaked);
  const base::DictionaryValue* event2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  EXPECT_TRUE(event2);
  VerifyDisplayString(event2,
                      base::UTF8ToUTF16("Checking passwords (2 of 3)…"));

  // Final update comes after status change, so no new progress message should
  // be present.
  test_passwords_delegate_.SetPasswordCheckState(
      extensions::api::passwords_private::PASSWORD_CHECK_STATE_IDLE);
  test_passwords_delegate_.SetProgress(3, 3);
  static_cast<password_manager::BulkLeakCheckService::Observer*>(
      safety_check_.get())
      ->OnCredentialDone(credential, is_leaked);
  const base::DictionaryValue* event3 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  EXPECT_TRUE(event3);
  // Still 2/3 event.
  VerifyDisplayString(event3,
                      base::UTF8ToUTF16("Checking passwords (2 of 3)…"));
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_NoExtensions) {
  safety_check_->PerformSafetyCheck();
  EXPECT_TRUE(GetSafetyCheckStatusChangedWithDataIfExists(
      kExtensions,
      static_cast<int>(
          SafetyCheckHandler::ExtensionsStatus::kNoneBlocklisted)));
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kNoneBlocklisted, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_NoneBlocklisted) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlocklistState(
      extension_id, extensions::NOT_BLOCKLISTED);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions,
          static_cast<int>(
              SafetyCheckHandler::ExtensionsStatus::kNoneBlocklisted));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "You're protected from potentially harmful extensions");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kNoneBlocklisted, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_BlocklistedAllDisabled) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::DISABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlocklistState(
      extension_id, extensions::BLOCKLISTED_MALWARE);
  test_extension_service_.AddExtensionState(extension_id, Enabled(false),
                                            UserCanDisable(false));
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions,
          static_cast<int>(
              SafetyCheckHandler::ExtensionsStatus::kBlocklistedAllDisabled));
  EXPECT_TRUE(event);
  VerifyDisplayString(
      event, "1 potentially harmful extension is off. You can also remove it.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kBlocklistedAllDisabled, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_BlocklistedReenabledAllByUser) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlocklistState(
      extension_id, extensions::BLOCKLISTED_POTENTIALLY_UNWANTED);
  test_extension_service_.AddExtensionState(extension_id, Enabled(true),
                                            UserCanDisable(true));
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions, static_cast<int>(SafetyCheckHandler::ExtensionsStatus::
                                            kBlocklistedReenabledAllByUser));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "You turned 1 potentially harmful extension back on");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kBlocklistedReenabledAllByUser, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_BlocklistedReenabledAllByAdmin) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlocklistState(
      extension_id, extensions::BLOCKLISTED_POTENTIALLY_UNWANTED);
  test_extension_service_.AddExtensionState(extension_id, Enabled(true),
                                            UserCanDisable(false));
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions, static_cast<int>(SafetyCheckHandler::ExtensionsStatus::
                                            kBlocklistedReenabledAllByAdmin));
  VerifyDisplayString(event,
                      "Your administrator turned 1 potentially harmful "
                      "extension back on");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kBlocklistedReenabledAllByAdmin, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_BlocklistedReenabledSomeByUser) {
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlocklistState(
      extension_id, extensions::BLOCKLISTED_POTENTIALLY_UNWANTED);
  test_extension_service_.AddExtensionState(extension_id, Enabled(true),
                                            UserCanDisable(true));

  std::string extension2_id = GenerateExtensionId('b');
  scoped_refptr<const extensions::Extension> extension2 =
      extensions::ExtensionBuilder("test1").SetID(extension2_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension2.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlocklistState(
      extension2_id, extensions::BLOCKLISTED_POTENTIALLY_UNWANTED);
  test_extension_service_.AddExtensionState(extension2_id, Enabled(true),
                                            UserCanDisable(false));

  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions, static_cast<int>(SafetyCheckHandler::ExtensionsStatus::
                                            kBlocklistedReenabledSomeByUser));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "You turned 1 potentially harmful extension back "
                      "on. Your administrator "
                      "turned 1 potentially harmful extension back on.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kBlocklistedReenabledSomeByUser, 1);
}

TEST_F(SafetyCheckHandlerTest, CheckExtensions_Error) {
  // One extension in the error state.
  std::string extension_id = GenerateExtensionId('a');
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test0").SetID(extension_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlocklistState(
      extension_id, extensions::BLOCKLISTED_UNKNOWN);
  test_extension_service_.AddExtensionState(extension_id, Enabled(true),
                                            UserCanDisable(true));

  // Another extension blocklisted.
  std::string extension2_id = GenerateExtensionId('b');
  scoped_refptr<const extensions::Extension> extension2 =
      extensions::ExtensionBuilder("test1").SetID(extension2_id).Build();
  test_extension_prefs_->OnExtensionInstalled(
      extension2.get(), extensions::Extension::State::ENABLED,
      syncer::StringOrdinal(), "");
  test_extension_prefs_->SetExtensionBlocklistState(
      extension2_id, extensions::BLOCKLISTED_POTENTIALLY_UNWANTED);
  test_extension_service_.AddExtensionState(extension2_id, Enabled(true),
                                            UserCanDisable(false));

  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions,
          static_cast<int>(SafetyCheckHandler::ExtensionsStatus::kError));
  EXPECT_TRUE(event);
  VerifyDisplayString(event,
                      "Browser can't check your extensions. Try again later.");
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ExtensionsResult",
      SafetyCheckHandler::ExtensionsStatus::kError, 1);
}

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
class SafetyCheckHandlerChromeCleanerIdleTest
    : public SafetyCheckHandlerTest,
      public testing::WithParamInterface<
          std::tuple<safe_browsing::ChromeCleanerController::IdleReason,
                     SafetyCheckHandler::ChromeCleanerStatus,
                     base::string16>> {
 protected:
  void SetUp() override {
    SafetyCheckHandlerTest::SetUp();
    idle_reason_ = testing::get<0>(GetParam());
    expected_cct_status_ = testing::get<1>(GetParam());
    expected_display_string_ = testing::get<2>(GetParam());
  }

  safe_browsing::ChromeCleanerController::IdleReason idle_reason_;
  SafetyCheckHandler::ChromeCleanerStatus expected_cct_status_;
  base::string16 expected_display_string_;
};

TEST_P(SafetyCheckHandlerChromeCleanerIdleTest, CheckChromeCleanerIdleStates) {
  safe_browsing::ChromeCleanerControllerImpl::ResetInstanceForTesting();
  safe_browsing::ChromeCleanerControllerImpl::GetInstance()->SetIdleForTesting(
      idle_reason_);
  safety_check_->PerformSafetyCheck();
  // Ensure WebUI event is sent.
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kChromeCleaner, static_cast<int>(expected_cct_status_));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, expected_display_string_);
  // Ensure UMA is logged.
  if (expected_cct_status_ ==
          SafetyCheckHandler::ChromeCleanerStatus::kHidden ||
      expected_cct_status_ ==
          SafetyCheckHandler::ChromeCleanerStatus::kChecking) {
    // Hidden and checking state should not get recorded.
    histogram_tester_.ExpectTotalCount(
        "Settings.SafetyCheck.ChromeCleanerResult", 0);
  } else {
    histogram_tester_.ExpectBucketCount(
        "Settings.SafetyCheck.ChromeCleanerResult", expected_cct_status_, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_Initial,
    SafetyCheckHandlerChromeCleanerIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::IdleReason::kInitial,
        SafetyCheckHandler::ChromeCleanerStatus::kNoUwsFoundWithTimestamp,
        base::UTF8ToUTF16("Browser didn't find harmful software on your "
                          "computer • Checked just now"))));

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_ReporterFoundNothing,
    SafetyCheckHandlerChromeCleanerIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::IdleReason::
            kReporterFoundNothing,
        SafetyCheckHandler::ChromeCleanerStatus::kNoUwsFoundWithTimestamp,
        base::UTF8ToUTF16("Browser didn't find harmful software on your "
                          "computer • Checked just now"))));

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_ReporterFailed,
    SafetyCheckHandlerChromeCleanerIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::IdleReason::kReporterFailed,
        SafetyCheckHandler::ChromeCleanerStatus::kError,
        base::UTF8ToUTF16("Something went wrong. Click for more details."))));

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_ScanningFoundNothing,
    SafetyCheckHandlerChromeCleanerIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::IdleReason::
            kScanningFoundNothing,
        SafetyCheckHandler::ChromeCleanerStatus::kNoUwsFoundWithTimestamp,
        base::UTF8ToUTF16("Browser didn't find harmful software on your "
                          "computer • Checked just now"))));

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_ScanningFailed,
    SafetyCheckHandlerChromeCleanerIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::IdleReason::kScanningFailed,
        SafetyCheckHandler::ChromeCleanerStatus::kError,
        base::UTF8ToUTF16("Something went wrong. Click for more details."))));

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_ConnectionLost,
    SafetyCheckHandlerChromeCleanerIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::IdleReason::kConnectionLost,
        SafetyCheckHandler::ChromeCleanerStatus::kInfected,
        base::UTF8ToUTF16("Browser found harmful software on your computer"))));

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_UserDeclinedCleanup,
    SafetyCheckHandlerChromeCleanerIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::IdleReason::
            kUserDeclinedCleanup,
        SafetyCheckHandler::ChromeCleanerStatus::kInfected,
        base::UTF8ToUTF16("Browser found harmful software on your computer"))));

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_CleaningFailed,
    SafetyCheckHandlerChromeCleanerIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::IdleReason::kCleaningFailed,
        SafetyCheckHandler::ChromeCleanerStatus::kError,
        base::UTF8ToUTF16("Something went wrong. Click for more details."))));

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_CleaningSucceed,
    SafetyCheckHandlerChromeCleanerIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::IdleReason::kCleaningSucceeded,
        SafetyCheckHandler::ChromeCleanerStatus::kNoUwsFoundWithTimestamp,
        base::UTF8ToUTF16("Browser didn't find harmful software on your "
                          "computer • Checked just now"))));

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_CleanerDownloadFailed,
    SafetyCheckHandlerChromeCleanerIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::IdleReason::
            kCleanerDownloadFailed,
        SafetyCheckHandler::ChromeCleanerStatus::kError,
        base::UTF8ToUTF16("Something went wrong. Click for more details."))));

class SafetyCheckHandlerChromeCleanerNonIdleTest
    : public SafetyCheckHandlerTest,
      public testing::WithParamInterface<
          std::tuple<safe_browsing::ChromeCleanerController::State,
                     SafetyCheckHandler::ChromeCleanerStatus,
                     base::string16>> {
 protected:
  void SetUp() override {
    SafetyCheckHandlerTest::SetUp();
    state_ = testing::get<0>(GetParam());
    expected_cct_status_ = testing::get<1>(GetParam());
    expected_display_string_ = testing::get<2>(GetParam());
  }

  safe_browsing::ChromeCleanerController::State state_;
  SafetyCheckHandler::ChromeCleanerStatus expected_cct_status_;
  base::string16 expected_display_string_;
};

TEST_P(SafetyCheckHandlerChromeCleanerNonIdleTest,
       CheckChromeCleanerNonIdleStates) {
  safe_browsing::ChromeCleanerControllerImpl::ResetInstanceForTesting();
  safe_browsing::ChromeCleanerControllerImpl::GetInstance()->SetStateForTesting(
      state_);
  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kChromeCleaner, static_cast<int>(expected_cct_status_));
  ASSERT_TRUE(event);
  VerifyDisplayString(event, expected_display_string_);
  // Ensure UMA is logged.
  if (expected_cct_status_ ==
          SafetyCheckHandler::ChromeCleanerStatus::kHidden ||
      expected_cct_status_ ==
          SafetyCheckHandler::ChromeCleanerStatus::kChecking) {
    // Hidden and checking state should not get recorded.
    histogram_tester_.ExpectTotalCount(
        "Settings.SafetyCheck.ChromeCleanerResult", 0);
  } else {
    histogram_tester_.ExpectBucketCount(
        "Settings.SafetyCheck.ChromeCleanerResult", expected_cct_status_, 1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_ReporterRunning,
    SafetyCheckHandlerChromeCleanerNonIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::State::kReporterRunning,
        SafetyCheckHandler::ChromeCleanerStatus::kScanningForUws,
        base::UTF8ToUTF16(
            "Browser is checking your computer for harmful software…"))));

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_Scanning,
    SafetyCheckHandlerChromeCleanerNonIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::State::kScanning,
        SafetyCheckHandler::ChromeCleanerStatus::kScanningForUws,
        base::UTF8ToUTF16(
            "Browser is checking your computer for harmful software…"))));

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_Cleaning,
    SafetyCheckHandlerChromeCleanerNonIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::State::kCleaning,
        SafetyCheckHandler::ChromeCleanerStatus::kRemovingUws,
        base::UTF8ToUTF16(
            "Browser is removing harmful software from your computer…"))));

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_Infected,
    SafetyCheckHandlerChromeCleanerNonIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::State::kInfected,
        SafetyCheckHandler::ChromeCleanerStatus::kInfected,
        base::UTF8ToUTF16("Browser found harmful software on your computer"))));

INSTANTIATE_TEST_SUITE_P(
    CheckChromeCleaner_RebootRequired,
    SafetyCheckHandlerChromeCleanerNonIdleTest,
    ::testing::Values(std::make_tuple(
        safe_browsing::ChromeCleanerController::State::kRebootRequired,
        SafetyCheckHandler::ChromeCleanerStatus::kRebootRequired,
        base::UTF8ToUTF16(
            "To finish removing harmful software, restart your computer"))));

TEST_F(SafetyCheckHandlerTest, CheckChromeCleaner_DisabledByAdmin) {
  safe_browsing::ChromeCleanerControllerImpl::ResetInstanceForTesting();
  safe_browsing::ChromeCleanerControllerImpl::GetInstance()
      ->SetDelegateForTesting(&test_chrome_cleaner_controller_delegate_);

  safety_check_->PerformSafetyCheck();
  const base::DictionaryValue* event =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kChromeCleaner,
          static_cast<int>(
              SafetyCheckHandler::ChromeCleanerStatus::kDisabledByAdmin));
  ASSERT_TRUE(event);
  VerifyDisplayString(
      event,
      "<a target=\"_blank\" "
      "href=\"https://support.google.com/chrome?p=your_administrator\">Your "
      "administrator</a> has turned off checking for harmful software");
}

TEST_F(SafetyCheckHandlerTest, CheckChromeCleaner_ObserverUpdateLogging) {
  safe_browsing::ChromeCleanerControllerImpl::ResetInstanceForTesting();
  safe_browsing::ChromeCleanerControllerImpl::GetInstance()->SetIdleForTesting(
      safe_browsing::ChromeCleanerController::IdleReason::
          kReporterFoundNothing);
  // We expect a user triggering a safety check to log the Chrome cleaner
  // result.
  safety_check_->PerformSafetyCheck();
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ChromeCleanerResult",
      SafetyCheckHandler::ChromeCleanerStatus::kNoUwsFoundWithTimestamp, 1);
  // Subsequent Chrome cleaner status updates without the user running safety
  // check again should not trigger logging.
  safety_check_->OnIdle(safe_browsing::ChromeCleanerController::IdleReason::
                            kReporterFoundNothing);
  safety_check_->OnReporterRunning();
  safety_check_->OnScanning();
  safety_check_->OnRebootRequired();
  safety_check_->OnRebootFailed();
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ChromeCleanerResult",
      SafetyCheckHandler::ChromeCleanerStatus::kNoUwsFoundWithTimestamp, 1);
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ChromeCleanerResult",
      SafetyCheckHandler::ChromeCleanerStatus::kRebootRequired, 0);
  histogram_tester_.ExpectBucketCount(
      "Settings.SafetyCheck.ChromeCleanerResult",
      SafetyCheckHandler::ChromeCleanerStatus::kScanningForUws, 0);
}
#endif

TEST_F(SafetyCheckHandlerTest, CheckParentRanDisplayString) {
  // 1 second before midnight Dec 31st 2020, so that -(24h-1s) is still on the
  // same day. This test time is hard coded to prevent DST flakiness, see
  // crbug.com/1066576.
  const base::Time system_time =
      base::Time::FromDoubleT(1609459199).LocalMidnight() -
      base::TimeDelta::FromSeconds(1);
  // Display strings for given time deltas in seconds.
  std::vector<std::tuple<std::string, int>> tuples{
      std::make_tuple("a moment ago", 1),
      std::make_tuple("a moment ago", 59),
      std::make_tuple("1 minute ago", 60),
      std::make_tuple("2 minutes ago", 60 * 2),
      std::make_tuple("59 minutes ago", 60 * 60 - 1),
      std::make_tuple("1 hour ago", 60 * 60),
      std::make_tuple("2 hours ago", 60 * 60 * 2),
      std::make_tuple("23 hours ago", 60 * 60 * 23),
      std::make_tuple("yesterday", 60 * 60 * 24),
      std::make_tuple("yesterday", 60 * 60 * 24 * 2 - 1),
      std::make_tuple("2 days ago", 60 * 60 * 24 * 2),
      std::make_tuple("2 days ago", 60 * 60 * 24 * 3 - 1),
      std::make_tuple("3 days ago", 60 * 60 * 24 * 3),
      std::make_tuple("3 days ago", 60 * 60 * 24 * 4 - 1)};
  // Test that above time deltas produce the corresponding display strings.
  for (auto tuple : tuples) {
    const base::Time time =
        system_time - base::TimeDelta::FromSeconds(std::get<1>(tuple));
    const base::string16 display_string =
        safety_check_->GetStringForParentRan(time, system_time);
    EXPECT_EQ(base::UTF8ToUTF16(
                  base::StrCat({"Safety check ran ", std::get<0>(tuple)})),
              display_string);
  }
}

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(SafetyCheckHandlerTest, CheckChromeCleanerRanDisplayString) {
  // Test string without timestamp.
  base::Time null_time;
  base::string16 display_string =
      safety_check_->GetStringForChromeCleanerRan(null_time, null_time);
  ReplaceBrowserName(&display_string);
  EXPECT_EQ(display_string,
            base::UTF8ToUTF16(
                "Browser can check your computer for harmful software"));
  // Test strings with timestamp.
  // 1 second before midnight Dec 31st 2020, so that -(24h-1s) is still on the
  // same day. This test time is hard coded to prevent DST flakiness, see
  // crbug.com/1066576.
  const base::Time system_time =
      base::Time::FromDoubleT(1609459199).LocalMidnight() -
      base::TimeDelta::FromSeconds(1);
  // Display strings for given time deltas in seconds.
  std::vector<std::tuple<std::string, int>> tuples{
      std::make_tuple("just now", 1),
      std::make_tuple("just now", 59),
      std::make_tuple("1 minute ago", 60),
      std::make_tuple("2 minutes ago", 60 * 2),
      std::make_tuple("59 minutes ago", 60 * 60 - 1),
      std::make_tuple("1 hour ago", 60 * 60),
      std::make_tuple("2 hours ago", 60 * 60 * 2),
      std::make_tuple("23 hours ago", 60 * 60 * 23),
      std::make_tuple("yesterday", 60 * 60 * 24),
      std::make_tuple("yesterday", 60 * 60 * 24 * 2 - 1),
      std::make_tuple("2 days ago", 60 * 60 * 24 * 2),
      std::make_tuple("2 days ago", 60 * 60 * 24 * 3 - 1),
      std::make_tuple("3 days ago", 60 * 60 * 24 * 3),
      std::make_tuple("3 days ago", 60 * 60 * 24 * 4 - 1)};
  // Test that above time deltas produce the corresponding display strings.
  for (auto tuple : tuples) {
    const base::Time time =
        system_time - base::TimeDelta::FromSeconds(std::get<1>(tuple));
    display_string =
        safety_check_->GetStringForChromeCleanerRan(time, system_time);
    ReplaceBrowserName(&display_string);
    EXPECT_EQ(base::UTF8ToUTF16(
                  base::StrCat({"Browser didn't find harmful software on "
                                "your computer • Checked ",
                                std::get<0>(tuple)})),
              display_string);
  }
}
#endif

TEST_F(SafetyCheckHandlerTest, CheckSafetyCheckStartedWebUiEvents) {
  safety_check_->SendSafetyCheckStartedWebUiUpdates();

  // Check that all initial updates ("running" states) are sent.
  const base::DictionaryValue* event_parent =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kParent,
          static_cast<int>(SafetyCheckHandler::ParentStatus::kChecking));
  ASSERT_TRUE(event_parent);
  VerifyDisplayString(event_parent, base::UTF8ToUTF16("Running…"));
  const base::DictionaryValue* event_updates =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kUpdates,
          static_cast<int>(SafetyCheckHandler::UpdateStatus::kChecking));
  ASSERT_TRUE(event_updates);
  VerifyDisplayString(event_updates, base::UTF8ToUTF16(""));
  const base::DictionaryValue* event_pws =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kPasswords,
          static_cast<int>(SafetyCheckHandler::PasswordsStatus::kChecking));
  ASSERT_TRUE(event_pws);
  VerifyDisplayString(event_pws, base::UTF8ToUTF16(""));
  const base::DictionaryValue* event_sb =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kSafeBrowsing,
          static_cast<int>(SafetyCheckHandler::SafeBrowsingStatus::kChecking));
  ASSERT_TRUE(event_sb);
  VerifyDisplayString(event_sb, base::UTF8ToUTF16(""));
  const base::DictionaryValue* event_extensions =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kExtensions,
          static_cast<int>(SafetyCheckHandler::ExtensionsStatus::kChecking));
  ASSERT_TRUE(event_extensions);
  VerifyDisplayString(event_extensions, base::UTF8ToUTF16(""));
}

TEST_F(SafetyCheckHandlerTest, CheckSafetyCheckCompletedWebUiEvents) {
  // Mock safety check invocation.
  safety_check_->PerformSafetyCheck();

  // Set the password check mock response.
  test_leak_service_->set_state_and_notify(
      password_manager::BulkLeakCheckService::State::kSignedOut);

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Set the Chrome cleaner mock response.
  safe_browsing::ChromeCleanerControllerImpl::ResetInstanceForTesting();
  safe_browsing::ChromeCleanerControllerImpl::GetInstance()->SetStateForTesting(
      safe_browsing::ChromeCleanerController::State::kInfected);
#endif

  // Check that the parent update is sent after all children checks completed.
  const base::DictionaryValue* event_parent =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kParent, static_cast<int>(SafetyCheckHandler::ParentStatus::kAfter));
  ASSERT_TRUE(event_parent);
  VerifyDisplayString(event_parent,
                      base::UTF8ToUTF16("Safety check ran a moment ago"));

#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Subsequent Chrome cleaner status updates without the user running safety
  // check again should not trigger further parent element completion events.
  safety_check_->OnIdle(safe_browsing::ChromeCleanerController::IdleReason::
                            kReporterFoundNothing);
  safety_check_->OnReporterRunning();
  safety_check_->OnScanning();
  safety_check_->OnRebootRequired();
  safety_check_->OnRebootFailed();
#endif

  // Check that there is no new parent completion event.
  const base::DictionaryValue* event_parent2 =
      GetSafetyCheckStatusChangedWithDataIfExists(
          kParent, static_cast<int>(SafetyCheckHandler::ParentStatus::kAfter));
  ASSERT_TRUE(event_parent2);
  ASSERT_TRUE(event_parent == event_parent2);
}
