// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/webid_dialog_views.h"
#include <memory>
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget_observer.h"

namespace {

const std::u16string kRpHostname = u"rp.example";
const char* kRpUrl = "https://rp.example";
const std::u16string kIdpHostname = u"idp.example";
const char* kIdpUrl = "https://idp.example";

class DialogObserver : public views::WidgetObserver {
 public:
  DialogObserver() = default;
  ~DialogObserver() override = default;

  void OnWidgetClosing(views::Widget* widget) override {
    close_observed_ = true;
  }

  bool WasWidgetClosed() { return close_observed_; }

 private:
  bool close_observed_ = false;
};

}  // namespace

using UserApproval = content::IdentityRequestDialogController::UserApproval;
using PermissionCallback =
    content::IdentityRequestDialogController::InitialApprovalCallback;
using CloseCallback =
    content::IdentityRequestDialogController::IdProviderWindowClosedCallback;

class WebIdDialogViewsTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    auto on_close = base::BindLambdaForTesting([&]() { did_close_ = true; });

    ChromeViewsTestBase::SetUp();
    test_contents_ = CreateTestWebContents(GURL{kRpUrl});
    parent_widget_ = CreateTestWidget();
    dialog_ = new WebIdDialogViews(test_contents_.get(),
                                   parent_widget_->GetNativeView(),
                                   std::move(on_close));
    dialog_observer_ = std::make_unique<DialogObserver>();
  }

  void TearDown() override {
    // Reset widget to close all windows before the final teardown.
    parent_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents(GURL url) {
    auto instance = content::SiteInstance::Create(&profile_);
    // Note that we don't initialize the RenderProcessHost. If that is needed
    // use `content::SiteInstance::GetProcess()->Init()`.
    auto contents = content::WebContentsTester::CreateTestWebContents(
        &profile_, std::move(instance));
    content::WebContentsTester::For(contents.get())->NavigateAndCommit(url);
    return contents;
  }

  WebIdDialogViews* dialog() const { return dialog_; }
  content::WebContents* web_contents() const { return test_contents_.get(); }
  DialogObserver* observer() const { return dialog_observer_.get(); }
  bool did_close() const { return did_close_; }

 private:
  std::unique_ptr<views::Widget> parent_widget_;
  raw_ptr<WebIdDialogViews> dialog_{nullptr};
  std::unique_ptr<DialogObserver> dialog_observer_;

  bool did_close_ = false;

  // Following are all that we need to create a test web contents.
  content::RenderViewHostTestEnabler test_render_host_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> test_contents_;
};

TEST_F(WebIdDialogViewsTest, DialogButtonsState) {
  // Initial permission should show two dialog buttons for OK and Cancel.
  dialog()->ShowInitialPermission(kRpHostname, kIdpHostname,
                                  PermissionDialogMode::kStateful,
                                  base::DoNothing());
  EXPECT_EQ(dialog()->GetDialogButtons(),
            ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  EXPECT_TRUE(dialog()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
  EXPECT_TRUE(dialog()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // SignIn page should not show any buttons for OK and Cancel.
  auto idp_contents = CreateTestWebContents(GURL{kIdpUrl});
  dialog()->ShowSigninPage(idp_contents.get(), GURL{kIdpUrl});
  EXPECT_EQ(dialog()->GetDialogButtons(), ui::DIALOG_BUTTON_NONE);

  // Token exchange should show two dialog buttons for OK and Cancel.
  dialog()->ShowTokenExchangePermission(kRpHostname, kIdpHostname,
                                        base::DoNothing());
  EXPECT_EQ(dialog()->GetDialogButtons(),
            ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
  EXPECT_TRUE(dialog()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
  EXPECT_TRUE(dialog()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
}

TEST_F(WebIdDialogViewsTest, ExplicitlyClosingSigninInvokesCallback) {
  auto idp_contents = CreateTestWebContents(GURL{kIdpUrl});
  dialog()->ShowSigninPage(idp_contents.get(), GURL{kIdpUrl});
  EXPECT_FALSE(did_close());
  dialog()->CloseSigninPage();
  EXPECT_TRUE(did_close());
}

TEST_F(WebIdDialogViewsTest, ClosingDialogOnSigninInvokesCallback) {
  auto idp_contents = CreateTestWebContents(GURL{kIdpUrl});
  dialog()->ShowSigninPage(idp_contents.get(), GURL{kIdpUrl});
  EXPECT_FALSE(did_close());
  dialog()->Close();
  EXPECT_TRUE(did_close());
}

TEST_F(WebIdDialogViewsTest, ClosingDialogOnInitialPermissionsRejectsCallback) {
  UserApproval approval = UserApproval::kApproved;
  auto on_permission_callback = base::BindLambdaForTesting(
      [&](UserApproval result) { approval = result; });
  dialog()->ShowInitialPermission(kRpHostname, kIdpHostname,
                                  PermissionDialogMode::kStateful,
                                  on_permission_callback);
  dialog()->Close();
  EXPECT_EQ(UserApproval::kDenied, approval);
}

TEST_F(WebIdDialogViewsTest, AcceptingOnInitialPermissionsAcceptsCallback) {
  UserApproval approval = UserApproval::kDenied;
  auto on_permission_callback = base::BindLambdaForTesting(
      [&](UserApproval result) { approval = result; });
  dialog()->ShowInitialPermission(kRpHostname, kIdpHostname,
                                  PermissionDialogMode::kStateful,
                                  on_permission_callback);
  dialog()->GetWidget()->AddObserver(observer());
  dialog()->AcceptDialog();
  EXPECT_EQ(UserApproval::kApproved, approval);
  EXPECT_FALSE(observer()->WasWidgetClosed());
}

TEST_F(WebIdDialogViewsTest, AcceptingStatelessPermissionModeClosesDialog) {
  UserApproval approval = UserApproval::kDenied;
  auto on_permission_callback = base::BindLambdaForTesting(
      [&](UserApproval result) { approval = result; });
  dialog()->ShowInitialPermission(kRpHostname, kIdpHostname,
                                  PermissionDialogMode::kStateless,
                                  on_permission_callback);
  dialog()->GetWidget()->AddObserver(observer());
  dialog()->AcceptDialog();
  EXPECT_EQ(UserApproval::kApproved, approval);
  EXPECT_TRUE(observer()->WasWidgetClosed());
}

TEST_F(WebIdDialogViewsTest, CancellingOnInitialPermissionsRejectsCallback) {
  UserApproval approval = UserApproval::kApproved;
  auto on_permission_callback = base::BindLambdaForTesting(
      [&](UserApproval result) { approval = result; });
  dialog()->ShowInitialPermission(kRpHostname, kIdpHostname,
                                  PermissionDialogMode::kStateful,
                                  on_permission_callback);
  dialog()->CancelDialog();
  EXPECT_EQ(UserApproval::kDenied, approval);
}

TEST_F(WebIdDialogViewsTest, InitialPermissionClosesInSinglePermissionMode) {
  UserApproval approval = UserApproval::kApproved;
  auto on_permission_callback = base::BindLambdaForTesting(
      [&](UserApproval result) { approval = result; });
  dialog()->ShowInitialPermission(kRpHostname, kIdpHostname,
                                  PermissionDialogMode::kStateless,
                                  on_permission_callback);
  dialog()->CancelDialog();
  EXPECT_EQ(UserApproval::kDenied, approval);
}

TEST_F(WebIdDialogViewsTest,
       ClosingDialogOnTokenExchangePermissionsRejectsCallback) {
  UserApproval approval = UserApproval::kApproved;
  auto on_permission_callback = base::BindLambdaForTesting(
      [&](UserApproval result) { approval = result; });
  dialog()->ShowTokenExchangePermission(kRpHostname, kIdpHostname,
                                        on_permission_callback);
  dialog()->Close();
  EXPECT_EQ(UserApproval::kDenied, approval);
}

TEST_F(WebIdDialogViewsTest,
       AcceptingOnTokenExchangePermissionsAcceptsCallback) {
  UserApproval approval = UserApproval::kDenied;
  auto on_permission_callback = base::BindLambdaForTesting(
      [&](UserApproval result) { approval = result; });
  dialog()->ShowTokenExchangePermission(kRpHostname, kIdpHostname,
                                        on_permission_callback);
  dialog()->AcceptDialog();
  EXPECT_EQ(UserApproval::kApproved, approval);
}

TEST_F(WebIdDialogViewsTest,
       CancellingOnTokenExchangePermissionsRejectsCallback) {
  UserApproval approval = UserApproval::kApproved;
  auto on_permission_callback = base::BindLambdaForTesting(
      [&](UserApproval result) { approval = result; });
  dialog()->ShowInitialPermission(kRpHostname, kIdpHostname,
                                  PermissionDialogMode::kStateful,
                                  on_permission_callback);
  dialog()->CancelDialog();
  EXPECT_EQ(UserApproval::kDenied, approval);
}

TEST_F(WebIdDialogViewsTest, AcceptingOnTokenExchangePermissionsClosesDialog) {
  dialog()->ShowTokenExchangePermission(kRpHostname, kIdpHostname,
                                        base::DoNothing());

  views::test::WidgetDestroyedWaiter waiter(dialog()->GetWidget());
  dialog()->AcceptDialog();
  waiter.Wait();
  // This tests will timeout if dialog widget is not destroyed so not timing out
  // is success.
}
