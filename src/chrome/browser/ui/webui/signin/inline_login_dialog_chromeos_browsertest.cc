// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos.h"

#include "base/json/json_reader.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace chromeos {
namespace {

// Subclass to access protected constructor and protected methods.
class TestInlineLoginDialogChromeOS : public InlineLoginDialogChromeOS {
 public:
  TestInlineLoginDialogChromeOS() = default;
  using SystemWebDialogDelegate::dialog_window;
};

// A simulated modal dialog. Taking focus seems important to repro the crash,
// but I'm not sure why.
class ChildModalDialogDelegate : public views::DialogDelegateView {
 public:
  ChildModalDialogDelegate() {
    // Our views::Widget will delete us.
    DCHECK(owned_by_widget());
    SetModalType(ui::MODAL_TYPE_CHILD);
    SetFocusBehavior(FocusBehavior::ALWAYS);
    // Dialogs that take focus must have a name to pass accessibility checks.
    GetViewAccessibility().OverrideName("Test dialog");
  }
  ChildModalDialogDelegate(const ChildModalDialogDelegate&) = delete;
  ChildModalDialogDelegate& operator=(const ChildModalDialogDelegate&) = delete;
  ~ChildModalDialogDelegate() override = default;
};

}  // namespace

using InlineLoginDialogChromeOSTest = InProcessBrowserTest;

// Regression test for use-after-free and crash. https://1170577
IN_PROC_BROWSER_TEST_F(InlineLoginDialogChromeOSTest,
                       CanOpenChildModelDialogThenCloseParent) {
  // Show the dialog. It is owned by the views system.
  TestInlineLoginDialogChromeOS* login_dialog =
      new TestInlineLoginDialogChromeOS();
  login_dialog->ShowSystemDialog();

  // Create a child modal dialog, similar to an http auth modal dialog.
  content::WebContents* web_contents =
      login_dialog->GetWebUIForTest()->GetWebContents();
  ASSERT_TRUE(web_contents);
  // The ChildModalDialogDelegate is owned by the views system.
  constrained_window::ShowWebModalDialogViews(new ChildModalDialogDelegate,
                                              web_contents);

  // Close the parent window.
  views::Widget* login_widget =
      views::Widget::GetWidgetForNativeWindow(login_dialog->dialog_window());
  views::test::WidgetDestroyedWaiter waiter(login_widget);
  login_dialog->Close();
  waiter.Wait();

  // No crash.
}

IN_PROC_BROWSER_TEST_F(InlineLoginDialogChromeOSTest, ReturnsEmptyDialogArgs) {
  auto* dialog = new InlineLoginDialogChromeOS(
      GURL(chrome::kChromeUIChromeSigninURL), /*options=*/absl::nullopt,
      /*close_dialog_closure=*/base::DoNothing());
  EXPECT_TRUE(InlineLoginDialogChromeOS::IsShown());
  EXPECT_EQ(dialog->GetDialogArgs(), "");

  // Delete dialog by calling OnDialogClosed.
  dialog->OnDialogClosed("");
  // Make sure the dialog is deleted.
  EXPECT_FALSE(InlineLoginDialogChromeOS::IsShown());
}

IN_PROC_BROWSER_TEST_F(InlineLoginDialogChromeOSTest,
                       ReturnsCorrectDialogArgs) {
  account_manager::AccountAdditionOptions options;
  options.is_available_in_arc = true;
  options.show_arc_availability_picker = false;
  auto* dialog = new InlineLoginDialogChromeOS(
      GURL(chrome::kChromeUIChromeSigninURL), options,
      /*close_dialog_closure=*/base::DoNothing());
  EXPECT_TRUE(InlineLoginDialogChromeOS::IsShown());

  absl::optional<base::Value> args =
      base::JSONReader::Read(dialog->GetDialogArgs());
  ASSERT_TRUE(args.has_value());
  EXPECT_TRUE(args.value().is_dict());
  base::DictionaryValue* dict = nullptr;
  args.value().GetAsDictionary(&dict);
  ASSERT_TRUE(dict != nullptr);
  absl::optional<bool> is_available_in_arc =
      dict->FindBoolKey("isAvailableInArc");
  absl::optional<bool> show_arc_availability_picker =
      dict->FindBoolKey("showArcAvailabilityPicker");
  ASSERT_TRUE(is_available_in_arc.has_value());
  ASSERT_TRUE(show_arc_availability_picker.has_value());
  EXPECT_EQ(is_available_in_arc.value(), options.is_available_in_arc);
  EXPECT_EQ(show_arc_availability_picker.value(),
            options.show_arc_availability_picker);

  // Delete dialog by calling OnDialogClosed.
  dialog->OnDialogClosed("");
  // Make sure the dialog is deleted.
  EXPECT_FALSE(InlineLoginDialogChromeOS::IsShown());
}

}  // namespace chromeos
