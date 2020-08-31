// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_ui/ui_handler.h"

#include <memory>

#include "base/test/gtest_util.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/create_options.h"
#include "chrome/browser/chromeos/login/ui/login_screen_extension_ui/window.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ui/ash/test_login_screen.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/session_manager/core/session_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kErrorWindowAlreadyExists[] =
    "Login screen extension UI already in use.";
const char kErrorNoExistingWindow[] = "No open window to close.";
const char kErrorNotOnLoginOrLockScreen[] =
    "Windows can only be created on the login and lock screen.";

const bool kCanBeClosedByUser = false;
const char kUrl[] = "test.html";

const char kWhitelistedExtensionID1[] =
    "oclffehlkdgibkainkilopaalpdobkan";  // Login screen APIs test extension
const char kWhitelistedExtensionID2[] =
    "lpimkpkllnkdlcigdbgmabfplniahkgm";  // Imprivata (login screen)
const char kPermissionName[] = "loginScreenUi";

}  // namespace

namespace chromeos {

namespace login_screen_extension_ui {

class FakeWindowFactory : public WindowFactory {
 public:
  FakeWindowFactory() = default;
  ~FakeWindowFactory() override = default;

  std::unique_ptr<Window> Create(CreateOptions* create_options) override {
    create_was_called_ = true;
    last_extension_name_ = create_options->extension_name;
    last_content_url_ = create_options->content_url;
    last_can_be_closed_by_user_ = create_options->can_be_closed_by_user;
    last_close_callback_ = std::move(create_options->close_callback);
    return std::unique_ptr<Window>();
  }

  bool create_was_called() const { return create_was_called_; }
  const std::string& last_extension_name() const {
    return last_extension_name_;
  }
  const GURL& last_content_url() const { return last_content_url_; }
  bool last_can_be_closed_by_user() const {
    return last_can_be_closed_by_user_;
  }

  void RunLastCloseCallback() { std::move(last_close_callback_).Run(); }

  void Reset() {
    create_was_called_ = false;
    last_extension_name_.clear();
    last_content_url_ = GURL();
    last_can_be_closed_by_user_ = false;
  }

 private:
  // Store arguments of last |Create()| call.
  std::string last_extension_name_;
  GURL last_content_url_;
  bool last_can_be_closed_by_user_;
  base::OnceClosure last_close_callback_;
  bool create_was_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeWindowFactory);
};

class LoginScreenExtensionUiHandlerUnittest : public testing::Test {
 public:
  LoginScreenExtensionUiHandlerUnittest()
      : scoped_current_channel_(version_info::Channel::DEV),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~LoginScreenExtensionUiHandlerUnittest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile* test_profile =
        profile_manager_.CreateTestingProfile(chrome::kInitialProfile);
    ASSERT_TRUE(test_profile);

    // Mark the device as being enterprise managed by policy.
    stub_install_attributes_ =
        test_profile->ScopedCrosSettingsTestHelper()->InstallAttributes();
    ASSERT_TRUE(stub_install_attributes_);
    stub_install_attributes_->SetCloudManaged("domain.com", "device_id");

    extension_registry_ = extensions::ExtensionRegistry::Get(test_profile);
    ASSERT_TRUE(extension_registry_);

    std::unique_ptr<FakeWindowFactory> fake_window_factory =
        std::make_unique<FakeWindowFactory>();
    fake_window_factory_ = fake_window_factory.get();

    ui_handler_ = std::make_unique<UiHandler>(std::move(fake_window_factory));

    session_manager_.SetSessionState(
        session_manager::SessionState::LOGIN_PRIMARY);

    extension_ = extensions::ExtensionBuilder(
                     /*extension_name=*/"LoginScreenUi test extension")
                     .SetID(kWhitelistedExtensionID1)
                     .SetLocation(extensions::Manifest::EXTERNAL_POLICY)
                     .AddPermission(kPermissionName)
                     .AddFlags(extensions::Extension::FOR_LOGIN_SCREEN)
                     .Build();
    extension_registry_->AddEnabled(extension_);
  }

  void TearDown() override { ui_handler_.reset(); }

 protected:
  void CheckCanOpenWindow(const extensions::Extension* extension,
                          const std::string& resource_path = kUrl,
                          bool can_be_closed_by_user = kCanBeClosedByUser) {
    std::unique_ptr<Window> ptr;

    EXPECT_FALSE(fake_window_factory_->create_was_called());
    std::string error;
    EXPECT_TRUE(ui_handler_->Show(extension, resource_path,
                                  can_be_closed_by_user, &error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(ui_handler_->HasOpenWindow(extension->id()));
    EXPECT_TRUE(fake_window_factory_->create_was_called());
    EXPECT_EQ(extension->short_name(),
              fake_window_factory_->last_extension_name());
    EXPECT_EQ(extension->GetResourceURL(resource_path),
              fake_window_factory_->last_content_url());
    EXPECT_EQ(can_be_closed_by_user,
              fake_window_factory_->last_can_be_closed_by_user());
    fake_window_factory_->Reset();
  }

  void CheckCanCloseWindow(const extensions::Extension* extension) {
    std::string error;
    EXPECT_TRUE(ui_handler_->Close(extension, &error));
    EXPECT_TRUE(error.empty());
    EXPECT_FALSE(ui_handler_->HasOpenWindow(extension->id()));
  }

  void CheckCannotOpenWindow(const extensions::Extension* extension,
                             const std::string& expected_error) {
    EXPECT_FALSE(fake_window_factory_->create_was_called());
    std::string error;
    EXPECT_FALSE(
        ui_handler_->Show(extension, kUrl, kCanBeClosedByUser, &error));
    EXPECT_EQ(expected_error, error);
    EXPECT_FALSE(fake_window_factory_->create_was_called());
  }

  void CheckCannotCloseWindow(const extensions::Extension* extension,
                              const std::string& expected_error) {
    std::string error;
    EXPECT_FALSE(ui_handler_->Close(extension, &error));
    EXPECT_EQ(expected_error, error);
  }

  void CheckCannotUseAPI(const extensions::Extension* extension) {
    ::testing::FLAGS_gtest_death_test_style = "fast";
    std::string error;
    EXPECT_CHECK_DEATH(
        ui_handler_->Show(extension, kUrl, kCanBeClosedByUser, &error));
  }

  content::BrowserTaskEnvironment task_environment_;
  const extensions::ScopedCurrentChannel scoped_current_channel_;

  session_manager::SessionManager session_manager_;
  TestingProfileManager profile_manager_;
  StubInstallAttributes* stub_install_attributes_ = nullptr;
  extensions::ExtensionRegistry* extension_registry_ = nullptr;
  scoped_refptr<const extensions::Extension> extension_;

  TestLoginScreen test_login_screen_;

  FakeWindowFactory* fake_window_factory_ = nullptr;

  std::unique_ptr<UiHandler> ui_handler_;

  DISALLOW_COPY_AND_ASSIGN(LoginScreenExtensionUiHandlerUnittest);
};

using LoginScreenExtensionUiHandlerDeathUnittest =
    LoginScreenExtensionUiHandlerUnittest;

TEST_F(LoginScreenExtensionUiHandlerUnittest, GlobalInstance) {
  EXPECT_FALSE(UiHandler::Get(false /*can_create*/));
  UiHandler* instance = UiHandler::Get(true /*can_create*/);
  EXPECT_TRUE(instance);
  EXPECT_EQ(instance, UiHandler::Get(true /*can_create*/));
  EXPECT_EQ(instance, UiHandler::Get(false /*can_create*/));
  UiHandler::Shutdown();
  EXPECT_FALSE(UiHandler::Get(false /*can_create*/));
}

TEST_F(LoginScreenExtensionUiHandlerUnittest,
       OpenAndCloseOnlyOnLoginAndLockScreen) {
  // SessionState::UNKNOWN
  session_manager_.SetSessionState(session_manager::SessionState::UNKNOWN);
  CheckCannotOpenWindow(extension_.get(), kErrorNotOnLoginOrLockScreen);

  // SessionState::OOBE
  session_manager_.SetSessionState(session_manager::SessionState::OOBE);
  CheckCannotOpenWindow(extension_.get(), kErrorNotOnLoginOrLockScreen);

  // SessionState::LOGIN_PRIMARY
  session_manager_.SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  CheckCanOpenWindow(extension_.get());
  CheckCanCloseWindow(extension_.get());

  // SessionState::LOGGED_IN_NOT_ACTIVE
  session_manager_.SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  CheckCanOpenWindow(extension_.get());
  CheckCanCloseWindow(extension_.get());

  // SessionState::ACTIVE
  session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
  CheckCannotOpenWindow(extension_.get(), kErrorNotOnLoginOrLockScreen);

  // SessionState::LOCKED
  session_manager_.SetSessionState(session_manager::SessionState::LOCKED);
  CheckCanOpenWindow(extension_.get());
  CheckCanCloseWindow(extension_.get());

  // SessionState::LOGIN_SECONDARY
  session_manager_.SetSessionState(
      session_manager::SessionState::LOGIN_SECONDARY);
  CheckCannotOpenWindow(extension_.get(), kErrorNotOnLoginOrLockScreen);
}

TEST_F(LoginScreenExtensionUiHandlerUnittest, WindowClosedOnLogin) {
  // Create window on login screen.
  session_manager_.SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  CheckCanOpenWindow(extension_.get());

  // State transition to |LOGGED_IN_NOT_ACTIVE| should not close the window.
  session_manager_.SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);
  EXPECT_TRUE(ui_handler_->HasOpenWindow(extension_->id()));

  // State transition to |ACTIVE| should close the window.
  session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(ui_handler_->HasOpenWindow(extension_->id()));
}

TEST_F(LoginScreenExtensionUiHandlerUnittest, WindowClosedOnUnlock) {
  // Create window on lock screen.
  session_manager_.SetSessionState(session_manager::SessionState::LOCKED);
  CheckCanOpenWindow(extension_.get());

  // State transition to |ACTIVE| should close the window.
  session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(ui_handler_->HasOpenWindow(extension_->id()));
}

TEST_F(LoginScreenExtensionUiHandlerUnittest, OnlyOneWindow) {
  scoped_refptr<const extensions::Extension> other_extension =
      extensions::ExtensionBuilder(/*extension_name=*/"Imprivata")
          .SetID(kWhitelistedExtensionID2)
          .SetLocation(extensions::Manifest::EXTERNAL_POLICY)
          .AddPermission(kPermissionName)
          .AddFlags(extensions::Extension::FOR_LOGIN_SCREEN)
          .Build();
  extension_registry_->AddEnabled(other_extension);

  // Open window with extension 1.
  CheckCanOpenWindow(extension_.get());
  EXPECT_TRUE(ui_handler_->HasOpenWindow(extension_->id()));
  EXPECT_FALSE(ui_handler_->HasOpenWindow(other_extension->id()));

  // Try to open another window with extension 1.
  CheckCannotOpenWindow(extension_.get(), kErrorWindowAlreadyExists);
  EXPECT_TRUE(ui_handler_->HasOpenWindow(extension_->id()));
  EXPECT_FALSE(ui_handler_->HasOpenWindow(other_extension->id()));

  // Open window with extension 2.
  CheckCannotOpenWindow(other_extension.get(), kErrorWindowAlreadyExists);
  EXPECT_TRUE(ui_handler_->HasOpenWindow(extension_->id()));
  EXPECT_FALSE(ui_handler_->HasOpenWindow(other_extension->id()));

  // Close window with extension 1.
  CheckCanCloseWindow(extension_.get());
  EXPECT_FALSE(ui_handler_->HasOpenWindow(extension_->id()));
  EXPECT_FALSE(ui_handler_->HasOpenWindow(other_extension->id()));
}

TEST_F(LoginScreenExtensionUiHandlerUnittest, CannotCloseNoWindow) {
  CheckCannotCloseWindow(extension_.get(), kErrorNoExistingWindow);
}

TEST_F(LoginScreenExtensionUiHandlerUnittest, ManualClose) {
  // Open window with extension 1.
  CheckCanOpenWindow(extension_.get());
  EXPECT_TRUE(ui_handler_->HasOpenWindow(extension_->id()));

  // Trigger manual close.
  fake_window_factory_->RunLastCloseCallback();
  EXPECT_FALSE(ui_handler_->HasOpenWindow(extension_->id()));
}

TEST_F(LoginScreenExtensionUiHandlerUnittest, WindowClosedOnUninstall) {
  // Open window.
  CheckCanOpenWindow(extension_.get());
  EXPECT_TRUE(ui_handler_->HasOpenWindow(extension_->id()));

  // Simulate extension uninstall.
  extension_registry_->RemoveEnabled(extension_->id());
  extension_registry_->TriggerOnUninstalled(
      extension_.get(), extensions::UNINSTALL_REASON_FOR_TESTING);
  EXPECT_FALSE(ui_handler_->HasOpenWindow(extension_->id()));
}

TEST_F(LoginScreenExtensionUiHandlerUnittest, WindowClosedOnUnloaded) {
  // Open window.
  CheckCanOpenWindow(extension_.get());
  EXPECT_TRUE(ui_handler_->HasOpenWindow(extension_->id()));

  // Simulate extension unload.
  extension_registry_->RemoveEnabled(extension_->id());
  extension_registry_->TriggerOnUnloaded(
      extension_.get(), extensions::UnloadedExtensionReason::BLACKLIST);
  EXPECT_FALSE(ui_handler_->HasOpenWindow(extension_->id()));
}

TEST_F(LoginScreenExtensionUiHandlerUnittest,
       OpenWindowWithDifferentArguments) {
  // Open window.
  CheckCanOpenWindow(extension_.get(), "some/file/path.html",
                     true /*can_be_closed_by_user*/);
  EXPECT_TRUE(ui_handler_->HasOpenWindow(extension_->id()));
}

TEST_F(LoginScreenExtensionUiHandlerDeathUnittest, NotAllowed) {
  // |other_profile_extension| is not enabled in the sign-in profile's
  // extensions registry.
  scoped_refptr<const extensions::Extension> other_profile_extension =
      extensions::ExtensionBuilder("other profile" /*extension_name*/)
          .SetID(kWhitelistedExtensionID2)  // whitelisted
          .SetLocation(extensions::Manifest::EXTERNAL_POLICY)
          .AddPermission(kPermissionName)
          .AddFlags(extensions::Extension::FOR_LOGIN_SCREEN)
          .Build();

  CheckCannotUseAPI(other_profile_extension.get());

  // |no_permission_extension| is enabled in the sign-in profile's extensions
  // registry, but doesn't have the needed "loginScreenUi" permission.
  scoped_refptr<const extensions::Extension> no_permission_extension =
      extensions::ExtensionBuilder("no permission extension" /*extension_name*/)
          .SetID(kWhitelistedExtensionID2)  // whitelisted
          .SetLocation(extensions::Manifest::EXTERNAL_POLICY)
          .AddFlags(extensions::Extension::FOR_LOGIN_SCREEN)
          .Build();
  extension_registry_->AddEnabled(no_permission_extension);

  CheckCannotUseAPI(no_permission_extension.get());

  // Mark the device as unmanaged.
  stub_install_attributes_->SetConsumerOwned();
  CheckCannotUseAPI(extension_.get());
}

}  // namespace login_screen_extension_ui

}  // namespace chromeos
