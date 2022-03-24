// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/plugin_vm/plugin_vm_installer_view.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_installer_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/concierge/fake_concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "chromeos/dbus/vm_plugin_dispatcher/fake_vm_plugin_dispatcher_client.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "services/network/test/test_network_connection_tracker.h"
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

// TODO(timloh): This file should only be responsible for testing the
// interactions between the installer UI and the installer backend. We should
// mock out the backend and move the tests for the backend logic out of here.

class PluginVmInstallerViewBrowserTest : public DialogBrowserTest {
 public:
  PluginVmInstallerViewBrowserTest() = default;

  PluginVmInstallerViewBrowserTest(const PluginVmInstallerViewBrowserTest&) =
      delete;
  PluginVmInstallerViewBrowserTest& operator=(
      const PluginVmInstallerViewBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    fake_concierge_client_ = chromeos::FakeConciergeClient::Get();
    fake_concierge_client_->set_disk_image_progress_signal_connected(true);
    fake_vm_plugin_dispatcher_client_ =
        static_cast<chromeos::FakeVmPluginDispatcherClient*>(
            chromeos::DBusThreadManager::Get()->GetVmPluginDispatcherClient());

    network_connection_tracker_ =
        network::TestNetworkConnectionTracker::CreateInstance();
    content::SetNetworkConnectionTrackerForTesting(nullptr);
    content::SetNetworkConnectionTrackerForTesting(
        network_connection_tracker_.get());
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_WIFI);
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
    auto* installer = plugin_vm::PluginVmInstallerFactory::GetForProfile(
        browser()->profile());
    installer->SetFreeDiskSpaceForTesting(installer->RequiredFreeDiskSpace());
    installer->SkipLicenseCheckForTesting();
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
    EXPECT_EQ(view_->GetTitle(),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_ERROR_TITLE));
  }

  void CheckSetupIsFinishedSuccessfully() {
    EXPECT_TRUE(HasAcceptButton());
    EXPECT_TRUE(HasCancelButton());
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_CANCEL),
              l10n_util::GetStringUTF16(IDS_APP_CLOSE));
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_INSTALLER_LAUNCH_BUTTON));
    EXPECT_EQ(view_->GetTitle(), l10n_util::GetStringUTF16(
                                     IDS_PLUGIN_VM_INSTALLER_FINISHED_TITLE));
  }

  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  chromeos::ScopedStubInstallAttributes scoped_stub_install_attributes_;

  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  PluginVmInstallerView* view_;
  chromeos::FakeConciergeClient* fake_concierge_client_;
  chromeos::FakeVmPluginDispatcherClient* fake_vm_plugin_dispatcher_client_;

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
    scoped_testing_cros_settings_.device_settings()->Set(ash::kPluginVmAllowed,
                                                         base::Value(true));
  }

  void SetUserWithAffiliation() {
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        browser()->profile()->GetProfileUserName(), "id"));
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
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
};

class PluginVmInstallerViewBrowserTestWithFeatureEnabled
    : public PluginVmInstallerViewBrowserTest {
 public:
  PluginVmInstallerViewBrowserTestWithFeatureEnabled() {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kPluginVm, {}},
         {download::kDownloadServiceFeature, {{"start_up_delay_ms", "0"}}}},
        {});
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
}

IN_PROC_BROWSER_TEST_F(
    PluginVmInstallerViewBrowserTest,
    SetupShouldShowDisallowedMessageIfPluginVmIsNotAllowedToRun) {
  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  view_->AcceptDialog();

  std::u16string app_name = l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME);
  EXPECT_FALSE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());
  EXPECT_EQ(view_->GetTitle(),
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
}

IN_PROC_BROWSER_TEST_F(PluginVmInstallerViewBrowserTestWithFeatureEnabled,
                       SetupShouldLaunchIfImageAlreadyImported) {
  AllowPluginVm();

  // Setup concierge and the dispatcher for VM already imported.
  vm_tools::concierge::ListVmDisksResponse list_vm_disks_response;
  list_vm_disks_response.set_success(true);
  list_vm_disks_response.add_images();
  fake_concierge_client_->set_list_vm_disks_response(list_vm_disks_response);

  vm_tools::plugin_dispatcher::ListVmResponse list_vms_response;
  list_vms_response.add_vm_info()->set_state(
      vm_tools::plugin_dispatcher::VmState::VM_STATE_STOPPED);
  fake_vm_plugin_dispatcher_client_->set_list_vms_response(list_vms_response);

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  view_->AcceptDialog();
  WaitForSetupToFinish();

  // Installer should be closed.
  EXPECT_EQ(nullptr, PluginVmInstallerView::GetActiveViewForTesting());
}
