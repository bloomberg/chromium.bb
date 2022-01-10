// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/consent_provider.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api/file_system/file_system_delegate.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::file_system_api::ConsentProvider;
using extensions::mojom::ManifestLocation;
using file_manager::Volume;

namespace extensions {
namespace {

class TestingConsentProviderDelegate
    : public ConsentProvider::DelegateInterface {
 public:
  TestingConsentProviderDelegate()
      : show_dialog_counter_(0),
        show_notification_counter_(0),
        dialog_button_(ui::DIALOG_BUTTON_NONE),
        is_auto_launched_(false) {}

  TestingConsentProviderDelegate(const TestingConsentProviderDelegate&) =
      delete;
  TestingConsentProviderDelegate& operator=(
      const TestingConsentProviderDelegate&) = delete;

  ~TestingConsentProviderDelegate() {}

  // Sets a fake dialog response.
  void SetDialogButton(ui::DialogButton button) { dialog_button_ = button; }

  // Sets a fake result of detection the auto launch kiosk mode.
  void SetIsAutoLaunched(bool is_auto_launched) {
    is_auto_launched_ = is_auto_launched;
  }

  // Sets an allowlisted components list with a single id.
  void SetComponentAllowlist(const std::string& extension_id) {
    allowlisted_component_id_ = extension_id;
  }

  int show_dialog_counter() const { return show_dialog_counter_; }
  int show_notification_counter() const { return show_notification_counter_; }

 private:
  // ConsentProvider::DelegateInterface overrides:
  void ShowDialog(const extensions::Extension& extension,
                  content::RenderFrameHost* host,
                  const base::WeakPtr<Volume>& volume,
                  bool writable,
                  ConsentProvider::ShowDialogCallback callback) override {
    ++show_dialog_counter_;
    std::move(callback).Run(dialog_button_);
  }

  void ShowNotification(const extensions::Extension& extension,
                        const base::WeakPtr<Volume>& volume,
                        bool writable) override {
    ++show_notification_counter_;
  }

  bool IsAutoLaunched(const extensions::Extension& extension) override {
    return is_auto_launched_;
  }

  bool IsAllowlistedComponent(const extensions::Extension& extension) override {
    return allowlisted_component_id_.compare(extension.id()) == 0;
  }

  bool HasRequestDownloadsPermission(const Extension& extension) override {
    return extension.permissions_data()->HasAPIPermission(
        mojom::APIPermissionID::kFileSystemRequestDownloads);
  }

  int show_dialog_counter_;
  int show_notification_counter_;
  ui::DialogButton dialog_button_;
  bool is_auto_launched_;
  std::string allowlisted_component_id_;
};

// Rewrites result of a consent request from |result| to |log|.
void OnConsentReceived(ConsentProvider::Consent* log,
                       const ConsentProvider::Consent result) {
  *log = result;
}

}  // namespace

class FileSystemApiConsentProviderTest : public testing::Test {
 public:
  FileSystemApiConsentProviderTest() {}

  void SetUp() override {
    testing_pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    TestingBrowserProcess::GetGlobal()->SetLocalState(
        testing_pref_service_.get());
    user_manager_ = new ash::FakeChromeUserManager;
    scoped_user_manager_enabler_ =
        std::make_unique<user_manager::ScopedUserManager>(
            base::WrapUnique(user_manager_));
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    download_volume_ = Volume::CreateForDownloads(temp_dir_.GetPath());
  }

  void TearDown() override {
    scoped_user_manager_enabler_.reset();
    user_manager_ = nullptr;
    testing_pref_service_.reset();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

 protected:
  base::WeakPtr<Volume> volume_;
  std::unique_ptr<TestingPrefServiceSimple> testing_pref_service_;
  ash::FakeChromeUserManager* user_manager_;  // Owned by the scope enabler.
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_enabler_;
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<Volume> download_volume_;
};

TEST_F(FileSystemApiConsentProviderTest, ForNonKioskApps) {
  // Component apps are not granted unless they are allowlisted.
  {
    scoped_refptr<const Extension> component_extension(
        ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
            .SetLocation(ManifestLocation::kComponent)
            .Build());
    TestingConsentProviderDelegate delegate;
    ConsentProvider provider(&delegate);
    EXPECT_EQ(provider.GetGrantVolumesMode(*component_extension),
              FileSystemDelegate::kGrantNone);
  }

  // Allowlisted component apps are instantly granted access without asking
  // user.
  {
    scoped_refptr<const Extension> allowlisted_component_extension(
        ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
            .SetLocation(ManifestLocation::kComponent)
            .Build());
    TestingConsentProviderDelegate delegate;
    delegate.SetComponentAllowlist(allowlisted_component_extension->id());
    ConsentProvider provider(&delegate);
    EXPECT_EQ(provider.GetGrantVolumesMode(*allowlisted_component_extension),
              FileSystemDelegate::kGrantAll);

    ConsentProvider::Consent result = ConsentProvider::CONSENT_IMPOSSIBLE;
    provider.RequestConsent(*allowlisted_component_extension.get(), nullptr,
                            volume_, true /* writable */,
                            base::BindOnce(&OnConsentReceived, &result));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(0, delegate.show_dialog_counter());
    EXPECT_EQ(0, delegate.show_notification_counter());
    EXPECT_EQ(ConsentProvider::CONSENT_GRANTED, result);
  }

  // Allowlisted extensions are instantly granted downloads access without
  // asking user.
  {
    scoped_refptr<const Extension> allowlisted_extension(
        ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
            .SetLocation(ManifestLocation::kComponent)
            .AddPermission("fileSystem.requestDownloads")
            .Build());
    TestingConsentProviderDelegate delegate;
    ConsentProvider provider(&delegate);
    EXPECT_EQ(provider.GetGrantVolumesMode(*allowlisted_extension),
              FileSystemDelegate::kGrantPerVolume);
    EXPECT_FALSE(
        provider.IsGrantableForVolume(*allowlisted_extension, volume_));
    EXPECT_TRUE(provider.IsGrantableForVolume(*allowlisted_extension,
                                              download_volume_->AsWeakPtr()));

    ConsentProvider::Consent result = ConsentProvider::CONSENT_IMPOSSIBLE;
    provider.RequestConsent(*allowlisted_extension.get(), nullptr,
                            download_volume_->AsWeakPtr(), true /* writable */,
                            base::BindRepeating(&OnConsentReceived, &result));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(0, delegate.show_dialog_counter());
    EXPECT_EQ(0, delegate.show_notification_counter());
    EXPECT_EQ(ConsentProvider::CONSENT_GRANTED, result);
  }

  // Non-component apps in non-kiosk mode will be rejected instantly, without
  // asking for user consent.
  {
    scoped_refptr<const Extension> non_component_extension(
        ExtensionBuilder("Test").Build());
    TestingConsentProviderDelegate delegate;
    ConsentProvider provider(&delegate);
    EXPECT_EQ(provider.GetGrantVolumesMode(*non_component_extension),
              FileSystemDelegate::kGrantNone);
  }
}

TEST_F(FileSystemApiConsentProviderTest, ForKioskApps) {
  // Non-component apps in auto-launch kiosk mode will be granted access
  // instantly without asking for user consent, but with a notification.
  {
    scoped_refptr<const Extension> auto_launch_kiosk_app(
        ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
            .SetManifestKey("kiosk_enabled", true)
            .SetManifestKey("kiosk_only", true)
            .Build());
    user_manager_->AddKioskAppUser(
        AccountId::FromUserEmail(auto_launch_kiosk_app->id()));
    user_manager_->LoginUser(
        AccountId::FromUserEmail(auto_launch_kiosk_app->id()));

    TestingConsentProviderDelegate delegate;
    delegate.SetIsAutoLaunched(true);
    ConsentProvider provider(&delegate);
    EXPECT_EQ(provider.GetGrantVolumesMode(*auto_launch_kiosk_app),
              FileSystemDelegate::kGrantAll);

    ConsentProvider::Consent result = ConsentProvider::CONSENT_IMPOSSIBLE;
    provider.RequestConsent(*auto_launch_kiosk_app.get(), nullptr, volume_,
                            true /* writable */,
                            base::BindOnce(&OnConsentReceived, &result));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(0, delegate.show_dialog_counter());
    EXPECT_EQ(1, delegate.show_notification_counter());
    EXPECT_EQ(ConsentProvider::CONSENT_GRANTED, result);
  }

  // Non-component apps in manual-launch kiosk mode will be granted access after
  // receiving approval from the user.
  scoped_refptr<const Extension> manual_launch_kiosk_app(
      ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
          .SetManifestKey("kiosk_enabled", true)
          .SetManifestKey("kiosk_only", true)
          .Build());
  user_manager::User* const manual_kiosk_app_user =
      user_manager_->AddKioskAppUser(
          AccountId::FromUserEmail(manual_launch_kiosk_app->id()));
  user_manager_->KioskAppLoggedIn(manual_kiosk_app_user);
  {
    TestingConsentProviderDelegate delegate;
    delegate.SetDialogButton(ui::DIALOG_BUTTON_OK);
    ConsentProvider provider(&delegate);
    EXPECT_EQ(provider.GetGrantVolumesMode(*manual_launch_kiosk_app),
              FileSystemDelegate::kGrantAll);

    ConsentProvider::Consent result = ConsentProvider::CONSENT_IMPOSSIBLE;
    provider.RequestConsent(*manual_launch_kiosk_app.get(), nullptr, volume_,
                            true /* writable */,
                            base::BindOnce(&OnConsentReceived, &result));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, delegate.show_dialog_counter());
    EXPECT_EQ(0, delegate.show_notification_counter());
    EXPECT_EQ(ConsentProvider::CONSENT_GRANTED, result);
  }

  // Non-component apps in manual-launch kiosk mode will be rejected access
  // after rejecting by a user.
  {
    TestingConsentProviderDelegate delegate;
    ConsentProvider provider(&delegate);
    delegate.SetDialogButton(ui::DIALOG_BUTTON_CANCEL);
    EXPECT_EQ(provider.GetGrantVolumesMode(*manual_launch_kiosk_app),
              FileSystemDelegate::kGrantAll);

    ConsentProvider::Consent result = ConsentProvider::CONSENT_IMPOSSIBLE;
    provider.RequestConsent(*manual_launch_kiosk_app.get(), nullptr, volume_,
                            true /* writable */,
                            base::BindOnce(&OnConsentReceived, &result));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(1, delegate.show_dialog_counter());
    EXPECT_EQ(0, delegate.show_notification_counter());
    EXPECT_EQ(ConsentProvider::CONSENT_REJECTED, result);
  }
}

}  // namespace extensions
