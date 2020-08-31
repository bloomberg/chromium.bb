// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/api/management.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permission_set.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/gpu_data_manager.h"
#endif

namespace extensions {

namespace {

std::unique_ptr<KeyedService> BuildManagementApi(
    content::BrowserContext* context) {
  return std::make_unique<ManagementAPI>(context);
}

std::unique_ptr<KeyedService> BuildEventRouter(
    content::BrowserContext* profile) {
  return std::make_unique<extensions::EventRouter>(
      profile, ExtensionPrefs::Get(profile));
}

namespace constants = extension_management_api_constants;

// TODO(devlin): Unittests are awesome. Test more with unittests and less with
// heavy api/browser tests.
class ManagementApiUnitTest : public ExtensionServiceTestWithInstall {
 protected:
  ManagementApiUnitTest() {}
  ~ManagementApiUnitTest() override {}

  // A wrapper around extension_function_test_utils::RunFunction that runs with
  // the associated browser, no flags, and can take stack-allocated arguments.
  bool RunFunction(const scoped_refptr<ExtensionFunction>& function,
                   const base::ListValue& args);

  // Runs the management.setEnabled() function to enable an extension.
  bool RunSetEnabledFunction(content::WebContents* web_contents,
                             const std::string& extension_id,
                             bool use_user_gesture,
                             bool accept_dialog,
                             std::string* error,
                             bool enabled = true);

  Browser* browser() { return browser_.get(); }

  // Returns the initialization parameters for the extension service.
  virtual ExtensionServiceInitParams GetExtensionServiceInitParams() {
    return CreateDefaultInitParams();
  }

  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  // The browser (and accompanying window).
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  DISALLOW_COPY_AND_ASSIGN(ManagementApiUnitTest);
};

bool ManagementApiUnitTest::RunFunction(
    const scoped_refptr<ExtensionFunction>& function,
    const base::ListValue& args) {
  return extension_function_test_utils::RunFunction(
      function.get(), base::WrapUnique(args.DeepCopy()), browser(),
      api_test_utils::NONE);
}

bool ManagementApiUnitTest::RunSetEnabledFunction(
    content::WebContents* web_contents,
    const std::string& extension_id,
    bool use_user_gesture,
    bool accept_dialog,
    std::string* error,
    bool enabled) {
  ScopedTestDialogAutoConfirm auto_confirm(
      accept_dialog ? ScopedTestDialogAutoConfirm::ACCEPT
                    : ScopedTestDialogAutoConfirm::CANCEL);
  base::Optional<ExtensionFunction::ScopedUserGestureForTests> gesture =
      base::nullopt;
  if (use_user_gesture)
    gesture.emplace();
  scoped_refptr<ManagementSetEnabledFunction> function =
      base::MakeRefCounted<ManagementSetEnabledFunction>();
  function->set_browser_context(profile());
  function->SetRenderFrameHost(web_contents->GetMainFrame());
  base::ListValue args;
  args.AppendString(extension_id);
  args.AppendBoolean(enabled);
  bool result = RunFunction(function, args);
  if (error)
    *error = function->GetError();
  return result;
}

void ManagementApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeExtensionService(GetExtensionServiceInitParams());
  ManagementAPI::GetFactoryInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildManagementApi));

  EventRouterFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildEventRouter));

  browser_window_.reset(new TestBrowserWindow());
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window_.get();
  browser_.reset(new Browser(params));
}

void ManagementApiUnitTest::TearDown() {
  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
}

// Test the basic parts of management.setEnabled.
TEST_F(ManagementApiUnitTest, ManagementSetEnabled) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  scoped_refptr<const Extension> source_extension =
      ExtensionBuilder("Test").Build();
  service()->AddExtension(source_extension.get());
  std::string extension_id = extension->id();
  scoped_refptr<ManagementSetEnabledFunction> function(
      new ManagementSetEnabledFunction());
  function->set_extension(source_extension);

  base::ListValue disable_args;
  disable_args.AppendString(extension_id);
  disable_args.AppendBoolean(false);

  // Test disabling an (enabled) extension.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
  EXPECT_TRUE(RunFunction(function, disable_args)) << function->GetError();
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));

  base::ListValue enable_args;
  enable_args.AppendString(extension_id);
  enable_args.AppendBoolean(true);

  // Test re-enabling it.
  function = new ManagementSetEnabledFunction();
  EXPECT_TRUE(RunFunction(function, enable_args)) << function->GetError();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));

  // Test that the enable function checks management policy, so that we can't
  // disable an extension that is required.
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  ManagementPolicy* policy =
      ExtensionSystem::Get(profile())->management_policy();
  policy->RegisterProvider(&provider);

  function = new ManagementSetEnabledFunction();
  EXPECT_FALSE(RunFunction(function, disable_args));
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUserCantModifyError,
                                           extension_id),
            function->GetError());
  policy->UnregisterProvider(&provider);
}

// Test that component extensions cannot be disabled, and that policy extensions
// can be disabled only by component/policy extensions.
TEST_F(ManagementApiUnitTest, ComponentPolicyDisabling) {
  auto component =
      ExtensionBuilder("component").SetLocation(Manifest::COMPONENT).Build();
  auto component2 =
      ExtensionBuilder("component2").SetLocation(Manifest::COMPONENT).Build();
  auto policy =
      ExtensionBuilder("policy").SetLocation(Manifest::EXTERNAL_POLICY).Build();
  auto policy2 = ExtensionBuilder("policy2")
                     .SetLocation(Manifest::EXTERNAL_POLICY)
                     .Build();
  auto internal =
      ExtensionBuilder("internal").SetLocation(Manifest::INTERNAL).Build();

  service()->AddExtension(component.get());
  service()->AddExtension(component2.get());
  service()->AddExtension(policy.get());
  service()->AddExtension(policy2.get());
  service()->AddExtension(internal.get());

  auto extension_can_disable_extension =
      [this](scoped_refptr<const Extension> source_extension,
             scoped_refptr<const Extension> target_extension) {
        std::string id = target_extension->id();
        base::ListValue args;
        args.AppendString(id);
        args.AppendBoolean(false /* disable the extension */);
        auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
        function->set_extension(source_extension);
        bool did_disable = RunFunction(function, args);
        // If the extension was disabled, re-enable it.
        if (did_disable) {
          EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
          service()->EnableExtension(id);
        } else {
          EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
        }
        return did_disable;
      };

  // Component extension cannot be disabled.
  EXPECT_FALSE(extension_can_disable_extension(component2, component));
  EXPECT_FALSE(extension_can_disable_extension(policy, component));
  EXPECT_FALSE(extension_can_disable_extension(internal, component));

  // Policy extension can be disabled by component/policy extensions, but not
  // others.
  EXPECT_TRUE(extension_can_disable_extension(component, policy));
  EXPECT_TRUE(extension_can_disable_extension(policy2, policy));
  EXPECT_FALSE(extension_can_disable_extension(internal, policy));
}

// Test that policy extensions can be enabled only by component/policy
// extensions.
TEST_F(ManagementApiUnitTest, ComponentPolicyEnabling) {
  auto component =
      ExtensionBuilder("component").SetLocation(Manifest::COMPONENT).Build();
  auto policy =
      ExtensionBuilder("policy").SetLocation(Manifest::EXTERNAL_POLICY).Build();
  auto policy2 = ExtensionBuilder("policy2")
                     .SetLocation(Manifest::EXTERNAL_POLICY)
                     .Build();
  auto internal =
      ExtensionBuilder("internal").SetLocation(Manifest::INTERNAL).Build();

  service()->AddExtension(component.get());
  service()->AddExtension(policy.get());
  service()->AddExtension(policy2.get());
  service()->AddExtension(internal.get());
  service()->DisableExtensionWithSource(
      component.get(), policy->id(), disable_reason::DISABLE_BLOCKED_BY_POLICY);

  auto extension_can_enable_extension =
      [this, component](scoped_refptr<const Extension> source_extension,
                        scoped_refptr<const Extension> target_extension) {
        std::string id = target_extension->id();
        base::ListValue args;
        args.AppendString(id);
        args.AppendBoolean(true /* enable the extension */);
        auto function = base::MakeRefCounted<ManagementSetEnabledFunction>();
        function->set_extension(source_extension);
        bool did_enable = RunFunction(function, args);
        // If the extension was enabled, disable it.
        if (did_enable) {
          EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
          service()->DisableExtensionWithSource(
              component.get(), id, disable_reason::DISABLE_BLOCKED_BY_POLICY);
        } else {
          EXPECT_TRUE(registry()->disabled_extensions().Contains(id));
        }
        return did_enable;
      };

  // Policy extension can be enabled by component/policy extensions, but not
  // others.
  EXPECT_TRUE(extension_can_enable_extension(component, policy));
  EXPECT_TRUE(extension_can_enable_extension(policy2, policy));
  EXPECT_FALSE(extension_can_enable_extension(internal, policy));
}

// Tests management.uninstall.
TEST_F(ManagementApiUnitTest, ManagementUninstall) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  std::string extension_id = extension->id();

  base::ListValue uninstall_args;
  uninstall_args.AppendString(extension->id());

  // Auto-accept any uninstalls.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);

    // Uninstall requires a user gesture, so this should fail.
    scoped_refptr<ExtensionFunction> function(
        new ManagementUninstallFunction());
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    EXPECT_EQ(std::string(constants::kGestureNeededForUninstallError),
              function->GetError());

    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    function = new ManagementUninstallFunction();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                              ExtensionRegistry::EVERYTHING));
  }

  // Install the extension again, and try uninstalling, auto-canceling the
  // dialog.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    service()->AddExtension(extension.get());
    scoped_refptr<ExtensionFunction> function =
        new ManagementUninstallFunction();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // The uninstall should have failed.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());

    // Try again, using showConfirmDialog: false.
    std::unique_ptr<base::DictionaryValue> options(new base::DictionaryValue());
    options->SetBoolean("showConfirmDialog", false);
    uninstall_args.Append(std::move(options));
    function = new ManagementUninstallFunction();
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // This should still fail, since extensions can only suppress the dialog for
    // uninstalling themselves.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());

    // If we try uninstall the extension itself, the uninstall should succeed
    // (even though we auto-cancel any dialog), because the dialog is never
    // shown.
    uninstall_args.Remove(0u, nullptr);
    function = new ManagementUninstallSelfFunction();
    function->set_extension(extension);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    EXPECT_FALSE(registry()->GetExtensionById(extension_id,
                                              ExtensionRegistry::EVERYTHING));
  }
}

// Tests management.uninstall from Web Store
TEST_F(ManagementApiUnitTest, ManagementWebStoreUninstall) {
  scoped_refptr<const Extension> triggering_extension =
      ExtensionBuilder("Test").SetID(extensions::kWebStoreAppId).Build();
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  std::string extension_id = extension->id();
  base::ListValue uninstall_args;
  uninstall_args.AppendString(extension->id());

  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;

    scoped_refptr<ExtensionFunction> function(
        new ManagementUninstallFunction());
    function->set_extension(triggering_extension);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_FALSE(RunFunction(function, uninstall_args));
    // Webstore does not suppress the dialog for uninstalling extensions.
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    EXPECT_EQ(ErrorUtils::FormatErrorMessage(constants::kUninstallCanceledError,
                                             extension_id),
              function->GetError());
  }

  {
    scoped_refptr<ExtensionFunction> function(
        new ManagementUninstallFunction());
    function->set_extension(triggering_extension);

    bool did_show = false;
    auto callback = base::BindRepeating(
        [](bool* did_show, extensions::ExtensionUninstallDialog* dialog) {
          EXPECT_EQ("Remove \"Test\"?", dialog->GetHeadingText());
          *did_show = true;
        },
        &did_show);
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(
        &callback);

    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_EQ(nullptr, registry()->GetInstalledExtension(extension_id));
    EXPECT_TRUE(did_show);

    // Reset the callback.
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(nullptr);
  }
}

// Tests management.uninstall with programmatic uninstall.
TEST_F(ManagementApiUnitTest, ManagementProgrammaticUninstall) {
  scoped_refptr<const Extension> triggering_extension =
      ExtensionBuilder("Triggering Extension").SetID("123").Build();
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  std::string extension_id = extension->id();
  base::ListValue uninstall_args;
  uninstall_args.AppendString(extension->id());
  {
    scoped_refptr<ExtensionFunction> function(
        new ManagementUninstallFunction());
    function->set_extension(triggering_extension);

    bool did_show = false;
    auto callback = base::BindRepeating(
        [](bool* did_show, extensions::ExtensionUninstallDialog* dialog) {
          EXPECT_EQ("\"Triggering Extension\" would like to remove \"Test\".",
                    dialog->GetHeadingText());
          *did_show = true;
        },
        &did_show);
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(
        &callback);

    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
    EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();
    // The extension should be uninstalled.
    EXPECT_EQ(nullptr, registry()->GetInstalledExtension(extension_id));
    EXPECT_TRUE(did_show);

    // Reset the callback.
    extensions::ExtensionUninstallDialog::SetOnShownCallbackForTesting(nullptr);
  }
}
// Tests uninstalling a blacklisted extension via management.uninstall.
TEST_F(ManagementApiUnitTest, ManagementUninstallBlacklisted) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  std::string id = extension->id();

  service()->BlacklistExtensionForTest(id);
  EXPECT_NE(nullptr, registry()->GetInstalledExtension(id));

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);
  ExtensionFunction::ScopedUserGestureForTests scoped_user_gesture;
  scoped_refptr<ExtensionFunction> function(new ManagementUninstallFunction());
  base::ListValue uninstall_args;
  uninstall_args.AppendString(id);
  EXPECT_TRUE(RunFunction(function, uninstall_args)) << function->GetError();

  EXPECT_EQ(nullptr, registry()->GetInstalledExtension(id));
}

TEST_F(ManagementApiUnitTest, ManagementEnableOrDisableBlacklisted) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());
  std::string id = extension->id();

  service()->BlacklistExtensionForTest(id);
  EXPECT_NE(nullptr, registry()->GetInstalledExtension(id));

  scoped_refptr<ExtensionFunction> function;

  // Test enabling it.
  {
    base::ListValue enable_args;
    enable_args.AppendString(id);
    enable_args.AppendBoolean(true);
    function = new ManagementSetEnabledFunction();
    EXPECT_TRUE(RunFunction(function, enable_args)) << function->GetError();
    EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
    EXPECT_FALSE(registry()->disabled_extensions().Contains(id));
  }

  // Test disabling it
  {
    base::ListValue disable_args;
    disable_args.AppendString(id);
    disable_args.AppendBoolean(false);

    function = new ManagementSetEnabledFunction();
    EXPECT_TRUE(RunFunction(function, disable_args)) << function->GetError();
    EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
    EXPECT_FALSE(registry()->disabled_extensions().Contains(id));
  }
}

TEST_F(ManagementApiUnitTest, ExtensionInfo_MayEnable) {
  using ExtensionInfo = api::management::ExtensionInfo;

  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());

  const std::string args =
      base::StringPrintf("[\"%s\"]", extension->id().c_str());
  scoped_refptr<ExtensionFunction> function;

  // Initially the extension should show as enabled.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  {
    function = new ManagementGetFunction();
    std::unique_ptr<base::Value> value(
        extension_function_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), args, browser()));
    ASSERT_TRUE(value);
    std::unique_ptr<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_TRUE(info->enabled);
    // |may_enable| is only returned for extensions which are not enabled.
    EXPECT_FALSE(info->may_enable.get());
  }

  // Simulate blacklisting the extension and verify that the extension shows as
  // disabled with a false value of |may_enable|.
  ManagementPolicy* policy =
      ExtensionSystem::Get(profile())->management_policy();
  policy->UnregisterAllProviders();
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::PROHIBIT_LOAD);
  policy->RegisterProvider(&provider);
  service()->CheckManagementPolicy();
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  {
    function = new ManagementGetFunction();
    std::unique_ptr<base::Value> value(
        extension_function_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), args, browser()));
    ASSERT_TRUE(value);
    std::unique_ptr<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_FALSE(info->enabled);
    ASSERT_TRUE(info->may_enable.get());
    EXPECT_FALSE(*(info->may_enable));
  }

  // Re-enable the extension.
  policy->UnregisterAllProviders();
  service()->CheckManagementPolicy();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  // Disable the extension with a normal user action. Verify the extension shows
  // as disabled with |may_enable| as true.
  service()->DisableExtension(extension->id(),
                              disable_reason::DISABLE_USER_ACTION);
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  {
    function = new ManagementGetFunction();
    std::unique_ptr<base::Value> value(
        extension_function_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), args, browser()));
    ASSERT_TRUE(value);
    std::unique_ptr<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_FALSE(info->enabled);
    ASSERT_TRUE(info->may_enable.get());
    EXPECT_TRUE(*(info->may_enable));
  }
}

TEST_F(ManagementApiUnitTest, ExtensionInfo_MayDisable) {
  using ExtensionInfo = api::management::ExtensionInfo;

  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  service()->AddExtension(extension.get());

  const std::string args =
      base::StringPrintf("[\"%s\"]", extension->id().c_str());

  // Initially the extension should show as enabled, so it may be disabled
  // freely.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  {
    scoped_refptr<ExtensionFunction> function = new ManagementGetFunction();
    std::unique_ptr<base::Value> value(
        extension_function_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), args, browser()));
    ASSERT_TRUE(value);
    std::unique_ptr<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_TRUE(info->enabled);
    EXPECT_TRUE(info->may_disable);
  }

  // Simulate forcing the extension and verify that the extension shows with
  // a false value of |may_disable|.
  ManagementPolicy* policy =
      ExtensionSystem::Get(profile())->management_policy();
  policy->UnregisterAllProviders();
  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::MUST_REMAIN_ENABLED);
  policy->RegisterProvider(&provider);
  service()->CheckManagementPolicy();
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
  {
    scoped_refptr<ExtensionFunction> function = new ManagementGetFunction();
    std::unique_ptr<base::Value> value(
        extension_function_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), args, browser()));
    ASSERT_TRUE(value);
    std::unique_ptr<ExtensionInfo> info = ExtensionInfo::FromValue(*value);
    ASSERT_TRUE(info);
    EXPECT_TRUE(info->enabled);
    EXPECT_FALSE(info->may_disable);
  }
}

// Tests enabling an extension via management API after it was disabled due to
// permission increase.
TEST_F(ManagementApiUnitTest, SetEnabledAfterIncreasedPermissions) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension = PackAndInstallCRX(path, pem_path, INSTALL_NEW);
  // The extension must now be installed and enabled.
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  // Save the id, as |extension| will be destroyed during updating.
  std::string extension_id = extension->id();

  std::unique_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  // v1 extension doesn't have any permissions.
  EXPECT_TRUE(known_perms->IsEmpty());

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(extension_id, path, pem_path, DISABLED);

  // The extension should be disabled.
  ASSERT_FALSE(registry()->enabled_extensions().Contains(extension_id));

  // Due to a permission increase, prefs will contain escalation information.
  EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // 1) Confirm re-enable prompt without user gesture, expect the extension to
  // stay disabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         false /* use_user_gesture */,
                                         true /* accept_dialog */, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 2) Deny re-enable prompt without user gesture, expect the extension to stay
  // disabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         false /* use_user_gesture */,
                                         false /* accept_dialog */, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 3) Deny re-enable prompt with user gesture, expect the extension to stay
  // disabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         true /* use_user_gesture */,
                                         false /* accept_dialog */, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs should still contain permissions escalation information.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // 4) Accept re-enable prompt with user gesture, expect the extension to be
  // enabled.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents.get(), extension_id,
                                         true /* use_user_gesture */,
                                         true /* accept_dialog */, &error);
    EXPECT_TRUE(success) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs will no longer contain the escalation information as user has
    // accepted increased permissions.
    EXPECT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // Some permissions for v2 extension should be granted by now.
  known_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  EXPECT_FALSE(known_perms->IsEmpty());
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)

// A delegate that senses when extensions are enabled or disabled.
class TestManagementAPIDelegate : public ManagementAPIDelegate {
 public:
  TestManagementAPIDelegate() = default;
  ~TestManagementAPIDelegate() override = default;

  void LaunchAppFunctionDelegate(
      const Extension* extension,
      content::BrowserContext* context) const override {}
  GURL GetFullLaunchURL(const Extension* extension) const override {
    return GURL();
  }
  LaunchType GetLaunchType(const ExtensionPrefs* prefs,
                           const Extension* extension) const override {
    return LaunchType::LAUNCH_TYPE_DEFAULT;
  }
  void GetPermissionWarningsByManifestFunctionDelegate(
      ManagementGetPermissionWarningsByManifestFunction* function,
      const std::string& manifest_str) const override {}
  std::unique_ptr<InstallPromptDelegate> SetEnabledFunctionDelegate(
      content::WebContents* web_contents,
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::Callback<void(bool)>& callback) const override {
    return nullptr;
  }
  void EnableExtension(content::BrowserContext* context,
                       const std::string& extension_id) const override {
    ++enable_count_;
  }
  void DisableExtension(
      content::BrowserContext* context,
      const Extension* source_extension,
      const std::string& extension_id,
      disable_reason::DisableReason disable_reason) const override {}
  std::unique_ptr<UninstallDialogDelegate> UninstallFunctionDelegate(
      ManagementUninstallFunctionBase* function,
      const Extension* target_extension,
      bool show_programmatic_uninstall_ui) const override {
    return nullptr;
  }
  bool UninstallExtension(content::BrowserContext* context,
                          const std::string& transient_extension_id,
                          UninstallReason reason,
                          base::string16* error) const override {
    return true;
  }
  bool CreateAppShortcutFunctionDelegate(
      ManagementCreateAppShortcutFunction* function,
      const Extension* extension,
      std::string* error) const override {
    return true;
  }
  void SetLaunchType(content::BrowserContext* context,
                     const std::string& extension_id,
                     LaunchType launch_type) const override {}
  std::unique_ptr<AppForLinkDelegate> GenerateAppForLinkFunctionDelegate(
      ManagementGenerateAppForLinkFunction* function,
      content::BrowserContext* context,
      const std::string& title,
      const GURL& launch_url) const override {
    return nullptr;
  }
  bool CanContextInstallWebApps(
      content::BrowserContext* context) const override {
    return true;
  }
  void InstallOrLaunchReplacementWebApp(
      content::BrowserContext* context,
      const GURL& web_app_url,
      InstallOrLaunchWebAppCallback callback) const override {}
  bool CanContextInstallAndroidApps(
      content::BrowserContext* context) const override {
    return true;
  }
  void CheckAndroidAppInstallStatus(
      const std::string& package_name,
      AndroidAppInstallStatusCallback callback) const override {}
  void InstallReplacementAndroidApp(
      const std::string& package_name,
      InstallAndroidAppCallback callback) const override {}
  GURL GetIconURL(const Extension* extension,
                  int icon_size,
                  ExtensionIconSet::MatchType match,
                  bool grayscale) const override {
    return GURL();
  }

  // EnableExtension is const, so this is mutable.
  mutable int enable_count_ = 0;
};

// A delegate that allows a child to try to install an extension and tracks
// whether the parent permission dialog would have opened.
class TestSupervisedUserServiceDelegate : public SupervisedUserServiceDelegate {
 public:
  TestSupervisedUserServiceDelegate() = default;
  ~TestSupervisedUserServiceDelegate() override = default;

  // SupervisedUserServiceDelegate:
  bool IsChild(content::BrowserContext* context) const override { return true; }

  bool IsSupervisedChildWhoMayInstallExtensions(
      content::BrowserContext* context) const override {
    return is_supervised_child_who_may_install_extensions_;
  }
  bool IsExtensionAllowedByParent(
      const extensions::Extension& extension,
      content::BrowserContext* context) const override {
    return false;
  }
  void ShowParentPermissionDialogForExtension(
      const extensions::Extension& extension,
      content::BrowserContext* context,
      content::WebContents* contents,
      ParentPermissionDialogDoneCallback done_callback) override {
    ++show_dialog_count_;
    std::move(done_callback).Run(dialog_result_);
  }

  void ShowExtensionEnableBlockedByParentDialogForExtension(
      const extensions::Extension* extension,
      content::WebContents* contents,
      base::OnceClosure done_callback) override {
    show_block_dialog_count_++;
    std::move(done_callback).Run();
  }

  void RecordExtensionEnableBlockedByParentDialogUmaMetric() override {
    SupervisedUserExtensionsMetricsRecorder::RecordEnablementUmaMetrics(
        SupervisedUserExtensionsMetricsRecorder::EnablementState::
            kFailedToEnable);
  }

  void set_next_parent_permission_dialog_result(
      ParentPermissionDialogResult result) {
    dialog_result_ = result;
  }

  void set_is_supervised_child_who_may_install_extensions(bool value) {
    is_supervised_child_who_may_install_extensions_ = value;
  }

  int show_dialog_count() const { return show_dialog_count_; }
  int show_block_dialog_count() const { return show_block_dialog_count_; }

 private:
  ParentPermissionDialogResult dialog_result_ =
      ParentPermissionDialogResult::kParentPermissionFailed;
  int show_dialog_count_ = 0;
  int show_block_dialog_count_ = 0;
  bool is_supervised_child_who_may_install_extensions_ = true;
};

// Tests for supervised users (child accounts). Supervised users are not allowed
// to install apps or extensions unless their parent approves.
class ManagementApiSupervisedUserTest : public ManagementApiUnitTest {
 public:
  ManagementApiSupervisedUserTest() = default;
  ~ManagementApiSupervisedUserTest() override = default;

  // ManagementApiUnitTest:
  ExtensionServiceInitParams GetExtensionServiceInitParams() override {
    ExtensionServiceInitParams params = CreateDefaultInitParams();
    // Force a TestingPrefServiceSyncable to be created.
    params.pref_file.clear();
    params.profile_is_supervised = true;
    return params;
  }

  SupervisedUserService* GetSupervisedUserService() {
    return SupervisedUserServiceFactory::GetForProfile(profile());
  }

  void SetUp() override {
    ManagementApiUnitTest::SetUp();

    // Set up custodians (parents) for the child.
    supervised_user_test_util::AddCustodians(browser()->profile());

    GetSupervisedUserService()->Init();
    // Set the pref to allow the child to request extension install.
    GetSupervisedUserService()
        ->SetSupervisedUserExtensionsMayRequestPermissionsPrefForTesting(true);

    // Create a WebContents to simulate the Chrome Web Store.
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

    management_api_ = ManagementAPI::GetFactoryInstance()->Get(profile());

    // Install a SupervisedUserServiceDelegate to sense the dialog state.
    supervised_user_delegate_ = new TestSupervisedUserServiceDelegate;
    management_api_->set_supervised_user_service_delegate_for_test(
        base::WrapUnique(supervised_user_delegate_));
  }

  std::unique_ptr<content::WebContents> web_contents_;
  ManagementAPI* management_api_ = nullptr;
  TestSupervisedUserServiceDelegate* supervised_user_delegate_ = nullptr;
};

TEST_F(ManagementApiSupervisedUserTest, SetEnabled_BlockedByParent) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());

  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  // The extension should be installed but disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  const std::string extension_id = extension->id();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->HasDisableReason(
      extension_id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));

  // Simulate disabling Permissions for sites, apps and extensions
  // in the testing supervised user service delegate used by the Management API.
  supervised_user_delegate_->set_is_supervised_child_who_may_install_extensions(
      false);
  // Ensure that the web contents can be used to create a modal dialog.
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_contents_.get());

  // The supervised user trying to enable while Permissions for sites, apps and
  // extensions is disabled should fail.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents_.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));

    // The block dialog should have been shown.
    EXPECT_EQ(supervised_user_delegate_->show_block_dialog_count(), 1);
  }

  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      SupervisedUserExtensionsMetricsRecorder::EnablementState::kFailedToEnable,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName, 1);
  EXPECT_EQ(
      1,
      user_action_tester.GetActionCount(
          SupervisedUserExtensionsMetricsRecorder::kFailedToEnableActionName));
}

TEST_F(ManagementApiSupervisedUserTest,
       SetEnabled_BlockedByParentNoDialogWhenNoDialogManagerAvailable) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());

  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  // The extension should be installed but disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  const std::string extension_id = extension->id();
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->HasDisableReason(
      extension_id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));

  // Simulate disabling Permissions for sites, apps and extensions
  // in the testing supervised user service delegate used by the Management API.
  supervised_user_delegate_->set_is_supervised_child_who_may_install_extensions(
      false);

  // The supervised user trying to enable while Permissions for sites, apps and
  // extensions is disabled should fail.
  {
    std::string error;

    bool success = RunSetEnabledFunction(web_contents_.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));

    // The block dialog should not have been shown.
    EXPECT_EQ(supervised_user_delegate_->show_block_dialog_count(), 0);
  }

  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      SupervisedUserExtensionsMetricsRecorder::EnablementState::kFailedToEnable,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName, 1);
  EXPECT_EQ(
      1,
      user_action_tester.GetActionCount(
          SupervisedUserExtensionsMetricsRecorder::kFailedToEnableActionName));
}

// Tests enabling an extension via management API after it was disabled due to
// permission increase for supervised users.
// Prevents a regression to crbug/1068660.
TEST_F(ManagementApiSupervisedUserTest, SetEnabled_AfterIncreasedPermissions) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());

  base::HistogramTester histogram_tester;

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  // The extension should be installed but disabled pending custodian approval.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  // Save the id, as |extension| will be destroyed during updating.
  const std::string extension_id = extension->id();

  // Simulate parent approval for the extension installation.
  GetSupervisedUserService()->AddExtensionApproval(*extension);
  // The extension should be enabled now.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));

  // Should see 1 kApprovalGranted UMA metric.
  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalGranted,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 1);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  std::unique_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  // v1 extension doesn't have any permissions.
  EXPECT_TRUE(known_perms->IsEmpty());

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(extension_id, path, pem_path, DISABLED);
  // The extension should be disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));
  // Due to a permission increase, prefs will contain escalation information.
  EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // Accept re-enable prompt with user gesture, expect the extension to be
  // enabled.
  {
    // The supervised user will approve the additional permissions without
    // parent approval.
    std::string error;
    bool success = RunSetEnabledFunction(web_contents_.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_TRUE(success) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    // Prefs will no longer contain the escalation information as the supervised
    // user has accepted the increased permissions.
    EXPECT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // Permissions for v2 extension should be granted now.
  known_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  EXPECT_FALSE(known_perms->IsEmpty());

  // The parent approval dialog should have not appeared.
  EXPECT_EQ(0, supervised_user_delegate_->show_dialog_count());

  // Should see 1 kPermissionsIncreaseGranted UMA metric.
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kPermissionsIncreaseGranted,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 2);
}

// Tests that supervised users can't approve permission updates by themselves
// when the "Permissions for sites, apps and extensions" toggle is off.
TEST_F(ManagementApiSupervisedUserTest,
       SetEnabled_CantApprovePermissionUpdatesToggleOff) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());

  base::HistogramTester histogram_tester;

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  // The extension should be installed but disabled pending custodian approval.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  // Save the id, as |extension| will be destroyed during updating.
  const std::string extension_id = extension->id();

  // Simulate parent approval for the extension installation.
  GetSupervisedUserService()->AddExtensionApproval(*extension);
  // The extension should be enabled now.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));

  // There should be 1 kApprovalGranted UMA metric.
  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName,
      SupervisedUserExtensionsMetricsRecorder::UmaExtensionState::
          kApprovalGranted,
      1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 1);

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  std::unique_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  // v1 extension doesn't have any permissions.
  EXPECT_TRUE(known_perms->IsEmpty());

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(extension_id, path, pem_path, DISABLED);
  // The extension should be disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));
  // Due to a permission increase, prefs will contain escalation information.
  EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));

  // If the "Permissions for sites, apps and extensions" toggle is off, then the
  // enable attempt should fail.
  {
    GetSupervisedUserService()
        ->SetSupervisedUserExtensionsMayRequestPermissionsPrefForTesting(false);
    std::string error;
    bool success = RunSetEnabledFunction(web_contents_.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));
    // Prefs will still contain the escalation information as the enable attempt
    // failed.
    EXPECT_TRUE(prefs->DidExtensionEscalatePermissions(extension_id));
  }

  // Permissions for v2 extension should not be granted.
  known_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  EXPECT_TRUE(known_perms->IsEmpty());

  // The parent approval dialog should have not appeared. The parent approval
  // dialog should never appear when the "Permissions for sites, apps and
  // extensions" toggle is off.
  EXPECT_EQ(0, supervised_user_delegate_->show_dialog_count());

  // There should be no new UMA metrics.
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName, 1);
}

// Tests that if an extension still requires parental consent, the supervised
// user approving it for permissions increase won't enable the extension and
// bypass parental consent.
// Prevents a regression to crbug/1070760.
TEST_F(ManagementApiSupervisedUserTest,
       SetEnabled_CustodianApprovalRequiredAndPermissionsIncrease) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  // The extension should be installed but disabled pending custodian approval.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  // Save the id, as |extension| will be destroyed during updating.
  const std::string extension_id = extension->id();

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  std::unique_ptr<const PermissionSet> known_perms =
      prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  // v1 extension doesn't have any permissions.
  EXPECT_TRUE(known_perms->IsEmpty());

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(extension_id, path, pem_path, DISABLED);
  // The extension should still be disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));
  // This extension has two concurrent disable reasons.
  EXPECT_TRUE(prefs->HasDisableReason(
      extension_id, disable_reason::DISABLE_PERMISSIONS_INCREASE));
  EXPECT_TRUE(prefs->HasDisableReason(
      extension_id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));

  // The supervised user trying to enable without parent approval should fail.
  {
    std::string error;
    bool success = RunSetEnabledFunction(web_contents_.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));
    // Both disable reasons should still be present.
    EXPECT_TRUE(prefs->HasDisableReason(
        extension_id, disable_reason::DISABLE_PERMISSIONS_INCREASE));
    EXPECT_TRUE(prefs->HasDisableReason(
        extension_id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));
  }

  // Permissions for v2 extension should not be granted.
  known_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  EXPECT_TRUE(known_perms->IsEmpty());

  // The parent approval dialog should have appeared.
  EXPECT_EQ(1, supervised_user_delegate_->show_dialog_count());

  // Now try again with parent approval, and this should succeed.
  {
    supervised_user_delegate_->set_next_parent_permission_dialog_result(
        SupervisedUserServiceDelegate::ParentPermissionDialogResult::
            kParentPermissionReceived);
    std::string error;
    bool success = RunSetEnabledFunction(web_contents_.get(), extension_id,
                                         /*use_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_TRUE(success) << error;
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
    // All disable reasons are gone.
    EXPECT_FALSE(prefs->DidExtensionEscalatePermissions(extension_id));
    EXPECT_FALSE(prefs->HasDisableReason(
        extension_id, disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));
  }

  // Permissions for v2 extension should now be granted.
  known_perms = prefs->GetGrantedPermissions(extension_id);
  ASSERT_TRUE(known_perms);
  EXPECT_FALSE(known_perms->IsEmpty());

  // The parent approval dialog should have appeared again.
  EXPECT_EQ(2, supervised_user_delegate_->show_dialog_count());
}

// Tests that trying to enable an extension with parent approval for supervised
// users still fails, if there's unsupported requirements.
TEST_F(ManagementApiSupervisedUserTest, SetEnabled_UnsupportedRequirement) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());
  ASSERT_EQ(0, supervised_user_delegate_->show_dialog_count());

  // No WebGL will be the unsupported requirement.
  content::GpuDataManager::GetInstance()->BlacklistWebGLForTesting();

  base::FilePath base_path = data_dir().AppendASCII("requirements");
  base::FilePath pem_path = base_path.AppendASCII("v1_good.pem");
  base::FilePath path = base_path.AppendASCII("v2_bad_requirements");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);
  // The extension should be installed but disabled pending custodian approval
  // and unsupported requirements.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());
  EXPECT_TRUE(prefs->HasDisableReason(
      extension->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));
  EXPECT_TRUE(prefs->HasDisableReason(
      extension->id(), disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT));

  // Parent approval should fail because of the unsupported requirements.
  {
    supervised_user_delegate_->set_next_parent_permission_dialog_result(
        SupervisedUserServiceDelegate::ParentPermissionDialogResult::
            kParentPermissionReceived);
    std::string error;
    bool success = RunSetEnabledFunction(web_contents_.get(), extension->id(),
                                         /*user_user_gesture=*/true,
                                         /*accept_dialog=*/true, &error);
    EXPECT_FALSE(success);
    EXPECT_FALSE(error.empty());
    // The parent permission dialog was never opened.
    EXPECT_EQ(0, supervised_user_delegate_->show_dialog_count());
    EXPECT_TRUE(registry()->disabled_extensions().Contains(extension->id()));
    // The extension should still require parent approval.
    EXPECT_TRUE(prefs->HasDisableReason(
        extension->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED));
    EXPECT_TRUE(prefs->HasDisableReason(
        extension->id(), disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT));
  }
}

// Tests UMA metrics related to supervised users enabling and disabling
// extensions.
TEST_F(ManagementApiSupervisedUserTest, SetEnabledDisabled_UmaMetrics) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension =
      PackAndInstallCRX(path, pem_path, INSTALL_WITHOUT_LOAD);
  ASSERT_TRUE(extension);

  // The parent will approve.
  supervised_user_delegate_->set_next_parent_permission_dialog_result(
      SupervisedUserServiceDelegate::ParentPermissionDialogResult::
          kParentPermissionReceived);

  RunSetEnabledFunction(web_contents_.get(), extension->id(),
                        /*use_user_gesture=*/true, /*accept_dialog=*/true,
                        nullptr, /*enabled=*/true);
  histogram_tester.ExpectUniqueSample(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      SupervisedUserExtensionsMetricsRecorder::EnablementState::kEnabled, 1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName, 1);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::kEnabledActionName));
  EXPECT_EQ(0,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::kDisabledActionName));

  // Simulate supervised user disabling extension.
  RunSetEnabledFunction(web_contents_.get(), extension->id(),
                        /*use_user_gesture=*/true, /*accept_dialog=*/true,
                        nullptr, /*enabled=*/false);
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      SupervisedUserExtensionsMetricsRecorder::EnablementState::kDisabled, 1);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName, 2);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::kEnabledActionName));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::kDisabledActionName));

  // Simulate supervised user re-enabling extension.
  RunSetEnabledFunction(web_contents_.get(), extension->id(),
                        /*use_user_gesture=*/true, /*accept_dialog=*/true,
                        nullptr, /*enabled=*/true);
  histogram_tester.ExpectBucketCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName,
      SupervisedUserExtensionsMetricsRecorder::EnablementState::kEnabled, 2);
  histogram_tester.ExpectTotalCount(
      SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName, 3);
  EXPECT_EQ(2,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::kEnabledActionName));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                SupervisedUserExtensionsMetricsRecorder::kDisabledActionName));
}

// Tests for supervised users (child accounts) with additional setup code.
class ManagementApiSupervisedUserTestWithSetup
    : public ManagementApiSupervisedUserTest {
 public:
  ManagementApiSupervisedUserTestWithSetup() = default;
  ~ManagementApiSupervisedUserTestWithSetup() override = default;

  void SetUp() override {
    ManagementApiSupervisedUserTest::SetUp();

    // Install a ManagementAPIDelegate to sense extension enable.
    delegate_ = new TestManagementAPIDelegate;
    management_api_->set_delegate_for_test(base::WrapUnique(delegate_));

    // Add a generic extension.
    extension_ = ExtensionBuilder("Test").Build();
    service()->AddExtension(extension_.get());
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_->id()));
  }

  TestManagementAPIDelegate* delegate_ = nullptr;
  scoped_refptr<const Extension> extension_;
};

TEST_F(ManagementApiSupervisedUserTestWithSetup, SetEnabled_ParentApproves) {
  // Preconditions.
  ASSERT_TRUE(profile()->IsChild());
  ASSERT_EQ(0, delegate_->enable_count_);
  ASSERT_EQ(0, supervised_user_delegate_->show_dialog_count());

  // Start with a disabled extension that needs parent permission.
  service()->DisableExtension(
      extension_->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);

  // The parent will approve.
  supervised_user_delegate_->set_next_parent_permission_dialog_result(
      SupervisedUserServiceDelegate::ParentPermissionDialogResult::
          kParentPermissionReceived);

  // Simulate a call to chrome.management.setEnabled(). It should succeed.
  std::string error;
  bool success = RunSetEnabledFunction(web_contents_.get(), extension_->id(),
                                       /*use_user_gesture=*/true,
                                       /*accept_dialog=*/true, &error);
  EXPECT_TRUE(success) << error;
  EXPECT_TRUE(error.empty());

  // Parent permission dialog was opened.
  EXPECT_EQ(1, supervised_user_delegate_->show_dialog_count());

  // Extension was enabled.
  EXPECT_EQ(1, delegate_->enable_count_);
}

TEST_F(ManagementApiSupervisedUserTestWithSetup, SetEnabled_ParentDenies) {
  // Start with a disabled extension that needs parent permission.
  service()->DisableExtension(
      extension_->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);

  // The parent will deny the next dialog.
  supervised_user_delegate_->set_next_parent_permission_dialog_result(
      SupervisedUserServiceDelegate::ParentPermissionDialogResult::
          kParentPermissionCanceled);

  // Simulate a call to chrome.management.setEnabled(). It should not succeed.
  std::string error;
  bool success = RunSetEnabledFunction(web_contents_.get(), extension_->id(),
                                       /*use_user_gesture=*/true,
                                       /*accept_dialog=*/true, &error);
  EXPECT_FALSE(success);
  EXPECT_FALSE(error.empty());

  // Parent permission dialog was opened.
  EXPECT_EQ(1, supervised_user_delegate_->show_dialog_count());

  // Extension was not enabled.
  EXPECT_EQ(0, delegate_->enable_count_);
}

TEST_F(ManagementApiSupervisedUserTestWithSetup, SetEnabled_DialogFails) {
  // Start with a disabled extension that needs parent permission.
  service()->DisableExtension(
      extension_->id(), disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);

  // The next dialog will close due to a failure (e.g. network failure while
  // looking up parent information).
  supervised_user_delegate_->set_next_parent_permission_dialog_result(
      SupervisedUserServiceDelegate::ParentPermissionDialogResult::
          kParentPermissionFailed);

  // Simulate a call to chrome.management.setEnabled(). It should not succeed.
  std::string error;
  bool success = RunSetEnabledFunction(web_contents_.get(), extension_->id(),
                                       /*use_user_gesture=*/true,
                                       /*accept_dialog=*/true, &error);
  EXPECT_FALSE(success);
  EXPECT_FALSE(error.empty());

  // Extension was not enabled.
  EXPECT_EQ(0, delegate_->enable_count_);
}

TEST_F(ManagementApiSupervisedUserTestWithSetup, SetEnabled_PreviouslyAllowed) {
  // Disable the extension.
  service()->DisableExtension(extension_->id(),
                              disable_reason::DISABLE_USER_ACTION);

  // Simulate previous parent approval.
  GetSupervisedUserService()->AddExtensionApproval(*extension_);

  // Simulate a call to chrome.management.setEnabled().
  std::string error;
  bool success = RunSetEnabledFunction(web_contents_.get(), extension_->id(),
                                       /*use_user_gesture=*/true,
                                       /*accept_dialog=*/true, &error);
  EXPECT_TRUE(success) << error;
  EXPECT_TRUE(error.empty());

  // Parent permission dialog was not opened.
  EXPECT_EQ(0, supervised_user_delegate_->show_dialog_count());
}
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

}  // namespace
}  // namespace extensions
