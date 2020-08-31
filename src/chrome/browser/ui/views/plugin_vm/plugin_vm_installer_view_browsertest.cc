// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/plugin_vm/plugin_vm_installer_view.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_metrics_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

const char kZipFile[] = "/downloads/a_zip_file.zip";
const char kZipFileHash[] =
    "bb077522e6c6fec07cf863ca44d5701935c4bc36ed12ef154f4cc22df70aec18";
const char kNonMatchingHash[] =
    "842841a4c75a55ad050d686f4ea5f77e83ae059877fe9b6946aa63d3d057ed32";
const char kJpgFile[] = "/downloads/image.jpg";
const char kJpgFileHash[] =
    "01ba4719c80b6fe911b091a7c05124b64eeece964e09c058ef8f9805daca546b";

}  // namespace

class PluginVmInstallerViewBrowserTest : public DialogBrowserTest {
 public:
  PluginVmInstallerViewBrowserTest() = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    fake_concierge_client_ = static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());
    fake_concierge_client_->set_disk_image_progress_signal_connected(true);

    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDownOnMainThread() override { scoped_user_manager_.reset(); }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    plugin_vm::ShowPluginVmInstallerView(browser()->profile());
    view_ = PluginVmInstallerView::GetActiveViewForTesting();
  }

 protected:
  bool HasAcceptButton() { return view_->GetOkButton() != nullptr; }

  bool HasCancelButton() { return view_->GetCancelButton() != nullptr; }

  void AllowPluginVm() {
    EnterpriseEnrollDevice();
    SetUserWithAffiliation();
    SetPluginVmPolicies();
    // Set correct PluginVmImage preference value.
    SetPluginVmImagePref(embedded_test_server()->GetURL(kZipFile).spec(),
                         kZipFileHash);
  }

  void SetPluginVmImagePref(std::string url, std::string hash) {
    DictionaryPrefUpdate update(browser()->profile()->GetPrefs(),
                                plugin_vm::prefs::kPluginVmImage);
    base::DictionaryValue* plugin_vm_image = update.Get();
    plugin_vm_image->SetKey("url", base::Value(url));
    plugin_vm_image->SetKey("hash", base::Value(hash));
  }

  void WaitForSetupToFinish() {
    base::RunLoop run_loop;
    view_->SetFinishedCallbackForTesting(
        base::BindOnce(&PluginVmInstallerViewBrowserTest::OnSetupFinished,
                       run_loop.QuitClosure()));

    run_loop.Run();
    content::RunAllTasksUntilIdle();
  }

  void CheckSetupFailed() {
    EXPECT_TRUE(HasAcceptButton());
    EXPECT_TRUE(HasCancelButton());
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_RETRY_BUTTON));
    EXPECT_EQ(view_->GetBigMessage(),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_ERROR_TITLE));
  }

  void CheckSetupIsFinishedSuccessfully() {
    EXPECT_TRUE(HasAcceptButton());
    EXPECT_TRUE(HasCancelButton());
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL),
              l10n_util::GetStringUTF16(IDS_APP_CLOSE));
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_LAUNCH_BUTTON));
    EXPECT_EQ(
        view_->GetBigMessage(),
        l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_FINISHED_TITLE));
  }

  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  chromeos::ScopedStubInstallAttributes scoped_stub_install_attributes_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  PluginVmInstallerView* view_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  chromeos::FakeConciergeClient* fake_concierge_client_;

 private:
  void EnterpriseEnrollDevice() {
    scoped_stub_install_attributes_.Get()->SetCloudManaged("example.com",
                                                           "device_id");
  }

  void SetPluginVmPolicies() {
    // User polcies.
    browser()->profile()->GetPrefs()->SetBoolean(
        plugin_vm::prefs::kPluginVmAllowed, true);
    // Device policies.
    scoped_testing_cros_settings_.device_settings()->Set(
        chromeos::kPluginVmAllowed, base::Value(true));
    scoped_testing_cros_settings_.device_settings()->Set(
        chromeos::kPluginVmLicenseKey, base::Value("LICENSE_KEY"));
  }

  void SetUserWithAffiliation() {
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        browser()->profile()->GetProfileUserName(), "id"));
    auto user_manager = std::make_unique<chromeos::FakeChromeUserManager>();
    user_manager->AddUserWithAffiliation(account_id, true);
    user_manager->LoginUser(account_id);
    chromeos::ProfileHelper::Get()->SetProfileToUserMappingForTesting(
        user_manager->GetActiveUser());
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
  }

  static void OnSetupFinished(base::OnceClosure quit_closure, bool success) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(quit_closure));
  }

  DISALLOW_COPY_AND_ASSIGN(PluginVmInstallerViewBrowserTest);
};

class PluginVmInstallerViewBrowserTestWithFeatureEnabled
    : public PluginVmInstallerViewBrowserTest {
 public:
  PluginVmInstallerViewBrowserTestWithFeatureEnabled() {
    feature_list_.InitAndEnableFeature(features::kPluginVm);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test the dialog is actually can be launched.
IN_PROC_BROWSER_TEST_F(PluginVmInstallerViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PluginVmInstallerViewBrowserTestWithFeatureEnabled,
                       SetupShouldFinishSuccessfully) {
  AllowPluginVm();
  plugin_vm::SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  view_->AcceptDialog();
  WaitForSetupToFinish();

  CheckSetupIsFinishedSuccessfully();

  histogram_tester_->ExpectUniqueSample(
      plugin_vm::kPluginVmSetupResultHistogram,
      plugin_vm::PluginVmSetupResult::kSuccess, 1);
}

IN_PROC_BROWSER_TEST_F(PluginVmInstallerViewBrowserTestWithFeatureEnabled,
                       SetupShouldFailAsHashesDoNotMatch) {
  AllowPluginVm();
  // Reset PluginVmImage hash to non-matching.
  SetPluginVmImagePref(embedded_test_server()->GetURL(kZipFile).spec(),
                       kNonMatchingHash);

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  view_->AcceptDialog();
  WaitForSetupToFinish();

  CheckSetupFailed();

  histogram_tester_->ExpectUniqueSample(
      plugin_vm::kPluginVmSetupResultHistogram,
      plugin_vm::PluginVmSetupResult::kError, 1);
}

IN_PROC_BROWSER_TEST_F(PluginVmInstallerViewBrowserTestWithFeatureEnabled,
                       SetupShouldFailAsImportingFails) {
  AllowPluginVm();
  SetPluginVmImagePref(embedded_test_server()->GetURL(kJpgFile).spec(),
                       kJpgFileHash);

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  view_->AcceptDialog();
  WaitForSetupToFinish();

  CheckSetupFailed();

  histogram_tester_->ExpectUniqueSample(
      plugin_vm::kPluginVmSetupResultHistogram,
      plugin_vm::PluginVmSetupResult::kError, 1);
}

IN_PROC_BROWSER_TEST_F(PluginVmInstallerViewBrowserTestWithFeatureEnabled,
                       CouldRetryAfterFailedSetup) {
  AllowPluginVm();
  // Reset PluginVmImage hash to non-matching.
  SetPluginVmImagePref(embedded_test_server()->GetURL(kZipFile).spec(),
                       kNonMatchingHash);

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  view_->AcceptDialog();
  WaitForSetupToFinish();

  CheckSetupFailed();

  plugin_vm::SetupConciergeForSuccessfulDiskImageImport(fake_concierge_client_);
  SetPluginVmImagePref(embedded_test_server()->GetURL(kZipFile).spec(),
                       kZipFileHash);

  // Retry button clicked to retry the download.
  view_->AcceptDialog();

  WaitForSetupToFinish();

  CheckSetupIsFinishedSuccessfully();

  histogram_tester_->ExpectBucketCount(plugin_vm::kPluginVmSetupResultHistogram,
                                       plugin_vm::PluginVmSetupResult::kError,
                                       1);
  histogram_tester_->ExpectBucketCount(plugin_vm::kPluginVmSetupResultHistogram,
                                       plugin_vm::PluginVmSetupResult::kSuccess,
                                       1);
}

IN_PROC_BROWSER_TEST_F(
    PluginVmInstallerViewBrowserTest,
    SetupShouldShowDisallowedMessageIfPluginVmIsNotAllowedToRun) {
  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  view_->AcceptDialog();

  base::string16 app_name = l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME);
  EXPECT_FALSE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());
  EXPECT_EQ(view_->GetBigMessage(),
            l10n_util::GetStringFUTF16(
                IDS_PLUGIN_VM_INSTALLER_NOT_ALLOWED_TITLE, app_name));
  EXPECT_EQ(
      view_->GetMessage(),
      l10n_util::GetStringFUTF16(
          IDS_PLUGIN_VM_INSTALLER_NOT_ALLOWED_MESSAGE, app_name,
          base::NumberToString16(
              static_cast<std::underlying_type_t<
                  plugin_vm::PluginVmInstaller::FailureReason>>(
                  plugin_vm::PluginVmInstaller::FailureReason::NOT_ALLOWED))));

  histogram_tester_->ExpectUniqueSample(
      plugin_vm::kPluginVmSetupResultHistogram,
      plugin_vm::PluginVmSetupResult::kError, 1);
}
