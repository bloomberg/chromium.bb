// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_install_dialog_view.h"

#include <string>
#include <utility>

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_test_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using extensions::PermissionIDSet;
using extensions::PermissionMessage;
using extensions::PermissionMessages;
using extensions::PermissionSet;

namespace {

void CloseAndWait(views::Widget* widget) {
  views::test::WidgetDestroyedWaiter waiter(widget);
  widget->Close();
  waiter.Wait();
}

}  // namespace

class ExtensionInstallDialogViewTestBase
    : public extensions::ExtensionBrowserTest {
 protected:
  ExtensionInstallDialogViewTestBase();

  void SetUpOnMainThread() override;

  // Creates and returns an install prompt of |prompt_type|.
  std::unique_ptr<ExtensionInstallPrompt::Prompt> CreatePrompt(
      ExtensionInstallPrompt::PromptType prompt_type);
  std::unique_ptr<ExtensionInstallPrompt::Prompt> CreatePrompt(
      ExtensionInstallPrompt::PromptType prompt_type,
      const extensions::Extension* extension);

  content::WebContents* web_contents() { return web_contents_; }

 private:
  const extensions::Extension* extension_;
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogViewTestBase);
};

ExtensionInstallDialogViewTestBase::ExtensionInstallDialogViewTestBase()
    : extension_(nullptr), web_contents_(nullptr) {}

void ExtensionInstallDialogViewTestBase::SetUpOnMainThread() {
  extensions::ExtensionBrowserTest::SetUpOnMainThread();

  extension_ = LoadExtension(test_data_dir_.AppendASCII(
      "install_prompt/permissions_scrollbar_regression"));

  web_contents_ = browser()->tab_strip_model()->GetWebContentsAt(0);
}

std::unique_ptr<ExtensionInstallPrompt::Prompt>
ExtensionInstallDialogViewTestBase::CreatePrompt(
    ExtensionInstallPrompt::PromptType prompt_type) {
  return CreatePrompt(prompt_type, extension_);
}

std::unique_ptr<ExtensionInstallPrompt::Prompt>
ExtensionInstallDialogViewTestBase::CreatePrompt(
    ExtensionInstallPrompt::PromptType prompt_type,
    const extensions::Extension* extension) {
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt(
      new ExtensionInstallPrompt::Prompt(prompt_type));
  prompt->set_extension(extension);

  std::unique_ptr<ExtensionIconManager> icon_manager(
      new ExtensionIconManager());
  prompt->set_icon(icon_manager->GetIcon(extension->id()));

  return prompt;
}

class ScrollbarTest : public ExtensionInstallDialogViewTestBase {
 protected:
  ScrollbarTest() {}

  bool IsScrollbarVisible(
      std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt);

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollbarTest);
};

bool ScrollbarTest::IsScrollbarVisible(
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt) {
  ExtensionInstallDialogView* dialog = new ExtensionInstallDialogView(
      profile(), web_contents(), ExtensionInstallPrompt::DoneCallback(),
      std::move(prompt));

  // Create the modal view around the install dialog view.
  views::Widget* modal = constrained_window::CreateBrowserModalDialogViews(
      dialog, web_contents()->GetTopLevelNativeWindow());
  modal->Show();
  content::RunAllTasksUntilIdle();

  // Check if the vertical scrollbar is visible.
  return dialog->scroll_view()->vertical_scroll_bar()->GetVisible();
}

// Tests that a scrollbar _is_ shown for an excessively long extension
// install prompt.
IN_PROC_BROWSER_TEST_F(ScrollbarTest, LongPromptScrollbar) {
  base::string16 permission_string(base::ASCIIToUTF16("Test"));
  PermissionMessages permissions;
  for (int i = 0; i < 20; i++) {
    permissions.push_back(PermissionMessage(permission_string,
                                            PermissionIDSet()));
  }
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt =
      CreatePrompt(ExtensionInstallPrompt::PERMISSIONS_PROMPT);
  prompt->AddPermissionMessages(permissions);
  ASSERT_TRUE(IsScrollbarVisible(std::move(prompt)))
      << "Scrollbar is not visible";
}

// Tests that a scrollbar isn't shown for this regression case.
// See crbug.com/385570 for details.
IN_PROC_BROWSER_TEST_F(ScrollbarTest, DISABLED_ScrollbarRegression) {
  base::string16 permission_string(base::ASCIIToUTF16(
      "Read and modify your data on *.facebook.com"));
  PermissionMessages permissions;
  permissions.push_back(PermissionMessage(permission_string,
                                          PermissionIDSet()));
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt =
      CreatePrompt(ExtensionInstallPrompt::PERMISSIONS_PROMPT);
  prompt->AddPermissionMessages(permissions);
  ASSERT_FALSE(IsScrollbarVisible(std::move(prompt))) << "Scrollbar is visible";
}

class ExtensionInstallDialogViewTest
    : public ExtensionInstallDialogViewTestBase {
 public:
  ExtensionInstallDialogViewTest() = default;
  ExtensionInstallDialogViewTest(const ExtensionInstallDialogViewTest&) =
      delete;
  ExtensionInstallDialogViewTest& operator=(
      const ExtensionInstallDialogViewTest&) = delete;

 protected:
  views::DialogDelegateView* CreateAndShowPrompt(
      ExtensionInstallPromptTestHelper* helper) {
    auto dialog = std::make_unique<ExtensionInstallDialogView>(
        profile(), web_contents(), helper->GetCallback(),
        CreatePrompt(ExtensionInstallPrompt::INSTALL_PROMPT));
    views::DialogDelegateView* delegate_view = dialog.get();

    views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
        dialog.release(), nullptr,
        platform_util::GetViewForWindow(
            browser()->window()->GetNativeWindow()));
    modal_dialog->Show();

    return delegate_view;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewTest, NotifyDelegate) {
  {
    // User presses install.
    ExtensionInstallPromptTestHelper helper;
    views::DialogDelegateView* delegate_view = CreateAndShowPrompt(&helper);
    delegate_view->AcceptDialog();
    EXPECT_EQ(ExtensionInstallPrompt::Result::ACCEPTED, helper.result());
  }
  {
    // User presses cancel.
    ExtensionInstallPromptTestHelper helper;
    views::DialogDelegateView* delegate_view = CreateAndShowPrompt(&helper);
    delegate_view->CancelDialog();
    EXPECT_EQ(ExtensionInstallPrompt::Result::USER_CANCELED, helper.result());
  }
  {
    // Dialog is closed without the user explicitly choosing to proceed or
    // cancel.
    ExtensionInstallPromptTestHelper helper;
    views::DialogDelegateView* delegate_view = CreateAndShowPrompt(&helper);
    CloseAndWait(delegate_view->GetWidget());
    // TODO(devlin): Should this be ABORTED?
    EXPECT_EQ(ExtensionInstallPrompt::Result::USER_CANCELED, helper.result());
  }
}

// Verifies that the "Add extension" button is disabled initially, but
// re-enabled after a short time delay.
IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewTest, InstallButtonDelay) {
  ExtensionInstallDialogView::SetInstallButtonDelayForTesting(0);
  ExtensionInstallPromptTestHelper helper;
  views::DialogDelegateView* delegate_view = CreateAndShowPrompt(&helper);

  // Check that dialog is visible.
  EXPECT_TRUE(delegate_view->GetVisible());

  // Check initial button states.
  EXPECT_FALSE(delegate_view->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(delegate_view->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
  EXPECT_TRUE(delegate_view->GetInitiallyFocusedView()->HasFocus());

  // Check OK button state after timeout to verify that it is re-enabled.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(delegate_view->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // Ensure default button (cancel) has focus.
  EXPECT_TRUE(delegate_view->GetInitiallyFocusedView()->HasFocus());
  CloseAndWait(delegate_view->GetWidget());
}

class ExtensionInstallDialogViewInteractiveBrowserTest
    : public DialogBrowserTest {
 public:
  ExtensionInstallDialogViewInteractiveBrowserTest() {
    feature_list_.InitAndEnableFeature(
        extensions_features::kAllowWithholdingExtensionPermissionsOnInstall);
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    extensions::ChromeTestExtensionLoader loader(browser()->profile());
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    scoped_refptr<const extensions::Extension> extension = loader.LoadExtension(
        test_data_dir.AppendASCII("extensions/uitest/long_name"));

    SkBitmap icon;
    // The dialog will downscale large images.
    icon.allocN32Pixels(800, 800);
    icon.eraseARGB(255, 128, 255, 128);

    auto prompt = std::make_unique<ExtensionInstallPrompt::Prompt>(type_);
    // The invoke UI tests can either directly set permission messages to easily
    // test different potential edge cases, or use a proper permission set which
    // goes through the standard flow to generate the messages.
    if (permission_set_)
      prompt->AddPermissionSet(*permission_set_);
    else
      prompt->AddPermissionMessages(permission_messages_);
    prompt->set_retained_files(retained_files_);
    prompt->set_retained_device_messages(retained_devices_);

    if (from_webstore_)
      prompt->SetWebstoreData("69,420", true, 2.5, 37);

    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    auto install_prompt =
        std::make_unique<ExtensionInstallPrompt>(web_contents);
    install_prompt->ShowDialog(base::DoNothing(), extension.get(), &icon,
                               std::move(prompt),
                               ExtensionInstallPrompt::ShowDialogCallback());
  }

  void set_from_webstore() { from_webstore_ = true; }

  void set_type(ExtensionInstallPrompt::PromptType type) { type_ = type; }

  void SetPermissionSet(std::unique_ptr<PermissionSet> permissions) {
    permission_set_ = std::move(permissions);
  }

  void AddPermission(std::string permission) {
    permission_messages_.push_back(
        PermissionMessage(base::ASCIIToUTF16(permission), PermissionIDSet()));
  }

  void AddRetainedFile(const base::FilePath& path) {
    retained_files_.push_back(path);
  }

  void AddRetainedDevice(const std::string& device) {
    retained_devices_.push_back(base::ASCIIToUTF16(device));
  }

  void AddPermissionWithDetails(
      std::string main_permission,
      std::vector<base::string16> detailed_permissions) {
    permission_messages_.push_back(
        PermissionMessage(base::ASCIIToUTF16(main_permission),
                          PermissionIDSet(), std::move(detailed_permissions)));
  }

 private:
  ExtensionInstallPrompt::PromptType type_ =
      ExtensionInstallPrompt::INSTALL_PROMPT;
  bool from_webstore_ = false;
  std::unique_ptr<PermissionSet> permission_set_;
  PermissionMessages permission_messages_;
  std::vector<base::FilePath> retained_files_;
  std::vector<base::string16> retained_devices_;

  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogViewInteractiveBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_Simple) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_External) {
  set_type(ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_ExternalWithPermission) {
  set_type(ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT);
  AddPermission("Example permission");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_ReEnable) {
  set_type(ExtensionInstallPrompt::RE_ENABLE_PROMPT);
  AddPermission("Example permission");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_FromWebstore) {
  set_type(ExtensionInstallPrompt::WEBSTORE_WIDGET_PROMPT);
  set_from_webstore();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_FromWebstoreWithPermission) {
  set_type(ExtensionInstallPrompt::WEBSTORE_WIDGET_PROMPT);
  set_from_webstore();
  AddPermission("Example permission");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_MultilinePermission) {
  AddPermission(
      "In the shade of the house, in the sunshine of the riverbank "
      "near the boats, in the shade of the Sal-wood forest, in the "
      "shade of the fig tree is where Siddhartha grew up");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_ManyPermissions) {
  for (int i = 0; i < 20; i++)
    AddPermission("Example permission");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_DetailedPermission) {
  AddPermissionWithDetails(
      "Example header permission",
      {base::ASCIIToUTF16("Detailed permission 1"),
       base::ASCIIToUTF16("Detailed permission 2"),
       base::ASCIIToUTF16("Very very very very very very long detailed "
                          "permission that wraps to a new line")});
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_WithRetainedFiles) {
  AddRetainedFile(base::FilePath(FILE_PATH_LITERAL("/dev/null")));
  AddRetainedFile(base::FilePath(FILE_PATH_LITERAL("/dev/zero")));
  AddRetainedFile(base::FilePath(FILE_PATH_LITERAL("/dev/random")));
  AddRetainedFile(base::FilePath(FILE_PATH_LITERAL(
      "/some/very/very/very/very/very/long/path/longer/than/the/"
      "line/length/file_with_long_name_too.txt")));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_WithRetainedDevices) {
  AddRetainedDevice("USB Device");
  AddRetainedDevice("USB Device With Longer Name");
  AddRetainedDevice(
      "Another USB Device With A Very Very Very Very Very Very "
      "Long Name So That It Hopefully Wraps to A New Line");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_WithWithholdingOption) {
  // The permissions withholding UI requires a proper permission set to be used,
  // as it checks for host permissions to determine if it should be shown.
  auto permissions = std::make_unique<PermissionSet>(
      extensions::APIPermissionSet(), extensions::ManifestPermissionSet(),
      extensions::URLPatternSet(
          {URLPattern(extensions::Extension::kValidHostPermissionSchemes,
                      "http://example.com/*")}),
      extensions::URLPatternSet());
  SetPermissionSet(std::move(permissions));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewInteractiveBrowserTest,
                       InvokeUi_AllInfoTypes) {
  AddPermission("Example permission");
  AddPermissionWithDetails("This permission has details",
                           {base::ASCIIToUTF16("Detailed permission 1"),
                            base::ASCIIToUTF16("Detailed permission 2")});
  AddRetainedDevice("USB Device");
  AddRetainedFile(base::FilePath(FILE_PATH_LITERAL("/dev/null")));
  ShowAndVerifyUi();
}

class ExtensionInstallDialogViewOnUninstallationTest
    : public ExtensionInstallDialogViewTest {
 public:
  ExtensionInstallDialogViewOnUninstallationTest() = default;
  ExtensionInstallDialogViewOnUninstallationTest(
      const ExtensionInstallDialogViewOnUninstallationTest&) = delete;
  ExtensionInstallDialogViewOnUninstallationTest& operator=(
      const ExtensionInstallDialogViewOnUninstallationTest&) = delete;

 protected:
  void UninstallExtension(const std::string& extension_id);
};

void ExtensionInstallDialogViewOnUninstallationTest::UninstallExtension(
    const std::string& extension_id) {
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(browser()->profile()), extension_id);
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->UninstallExtension(
          extension_id,
          extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING, nullptr);
  observer.WaitForExtensionUninstalled();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewOnUninstallationTest,
                       UninstallingExtensionClosesDialog) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "install_prompt/dialog_on_uninstall/same_extension"));
  ASSERT_TRUE(extension);
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt =
      CreatePrompt(ExtensionInstallPrompt::REPAIR_PROMPT, extension);
  ExtensionInstallDialogView* dialog = new ExtensionInstallDialogView(
      profile(), web_contents(), base::DoNothing(), std::move(prompt));

  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      dialog, nullptr,
      platform_util::GetViewForWindow(browser()->window()->GetNativeWindow()));
  ASSERT_TRUE(modal_dialog);
  views::test::WidgetClosingObserver dialog_observer(modal_dialog);
  modal_dialog->Show();
  EXPECT_FALSE(modal_dialog->IsClosed());
  UninstallExtension(extension->id());
  dialog_observer.Wait();
  EXPECT_TRUE(dialog_observer.widget_closed());
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewOnUninstallationTest,
                       UninstallingOtherExtensionDoesNotCloseDialog) {
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "install_prompt/dialog_on_uninstall/same_extension"));
  const extensions::Extension* other_extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "install_prompt/dialog_on_uninstall/other_extension"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(other_extension);
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt =
      CreatePrompt(ExtensionInstallPrompt::REPAIR_PROMPT, extension);
  ExtensionInstallDialogView* dialog = new ExtensionInstallDialogView(
      profile(), web_contents(), base::DoNothing(), std::move(prompt));

  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      dialog, nullptr,
      platform_util::GetViewForWindow(browser()->window()->GetNativeWindow()));
  ASSERT_TRUE(modal_dialog);
  views::test::WidgetClosingObserver dialog_observer(modal_dialog);
  modal_dialog->Show();
  EXPECT_FALSE(modal_dialog->IsClosed());
  UninstallExtension(other_extension->id());
  ASSERT_TRUE(modal_dialog);
  modal_dialog->Close();
  dialog_observer.Wait();
}

class ExtensionInstallDialogRatingsSectionTest
    : public ExtensionInstallDialogViewTest {
 public:
  ExtensionInstallDialogRatingsSectionTest() {}

  void TestRatingsSectionA11y(int num_ratings,
                              double average_rating,
                              const std::string& expected_text);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogRatingsSectionTest);
};

void ExtensionInstallDialogRatingsSectionTest::TestRatingsSectionA11y(
    int num_ratings,
    double average_rating,
    const std::string& expected_text) {
  SCOPED_TRACE(base::StringPrintf(
      "Testing with %d ratings, %f average rating. Expected text: '%s'.",
      num_ratings, average_rating, expected_text.c_str()));
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt =
      CreatePrompt(ExtensionInstallPrompt::REPAIR_PROMPT);
  prompt->SetWebstoreData("1,234", true, average_rating, num_ratings);

  ExtensionInstallDialogView* dialog = new ExtensionInstallDialogView(
      profile(), web_contents(), base::DoNothing(), std::move(prompt));

  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      dialog, nullptr,
      platform_util::GetViewForWindow(browser()->window()->GetNativeWindow()));
  modal_dialog->Show();

  views::View* rating_view = modal_dialog->non_client_view()->GetViewByID(
      ExtensionInstallDialogView::kRatingsViewId);
  ASSERT_TRUE(rating_view);
  {
    ui::AXNodeData node_data;
    rating_view->GetAccessibleNodeData(&node_data);
    EXPECT_EQ(ax::mojom::Role::kStaticText, node_data.role);
    EXPECT_EQ(expected_text,
              node_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
  }

  for (views::View* child : rating_view->children()) {
    ui::AXNodeData node_data;
    child->GetAccessibleNodeData(&node_data);
    EXPECT_EQ(ax::mojom::Role::kIgnored, node_data.role);
  }

  CloseAndWait(modal_dialog);
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogRatingsSectionTest,
                       RatingsSectionA11y) {
  TestRatingsSectionA11y(400, 3.297, "Rated 3.3 by 400 users.");
  TestRatingsSectionA11y(1, 1.0, "Rated 1.0 by one user.");
  TestRatingsSectionA11y(0, 0.0, "Not yet rated by any users.");
}

class ExtensionInstallDialogWithWithholdPermissionsUI
    : public ExtensionInstallDialogViewTestBase {
 public:
  ExtensionInstallDialogWithWithholdPermissionsUI() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kAllowWithholdingExtensionPermissionsOnInstall);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogWithWithholdPermissionsUI);
};

// Verifies that some UI is displayed in the extra view for withholding
// permissions on installation.
IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogWithWithholdPermissionsUI,
                       ShowsWithholdUI) {
  ExtensionInstallPromptTestHelper helper;
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt =
      CreatePrompt(ExtensionInstallPrompt::INSTALL_PROMPT);
  // Add a permission to the prompt with example.com as an explicit pattern.
  PermissionSet permissions(
      extensions::APIPermissionSet(), extensions::ManifestPermissionSet(),
      extensions::URLPatternSet(
          {URLPattern(extensions::Extension::kValidHostPermissionSchemes,
                      "http://example.com/*")}),
      extensions::URLPatternSet());
  prompt->AddPermissionSet(permissions);
  auto dialog = std::make_unique<ExtensionInstallDialogView>(
      profile(), web_contents(), helper.GetCallback(), std::move(prompt));
  views::DialogDelegateView* delegate_view = dialog.get();

  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      dialog.release(), nullptr,
      platform_util::GetViewForWindow(browser()->window()->GetNativeWindow()));
  modal_dialog->Show();

  const views::View* const extra_view = delegate_view->GetExtraView();
  EXPECT_TRUE(extra_view);
  EXPECT_EQ("Checkbox", std::string(extra_view->GetClassName()));

  CloseAndWait(delegate_view->GetWidget());
}
