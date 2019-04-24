// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_generation_popup_view.h"

#include "base/strings/string16.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view_tester.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"

namespace autofill {

class TestPasswordGenerationPopupController
    : public PasswordGenerationPopupControllerImpl {
 public:
  TestPasswordGenerationPopupController(content::WebContents* web_contents,
                                        gfx::NativeView native_view)
      : PasswordGenerationPopupControllerImpl(
            gfx::RectF(0, 0, 10, 10),
            PasswordForm(),
            base::string16(),
            10,
            password_manager::ContentPasswordManagerDriverFactory::
                FromWebContents(web_contents)
                    ->GetDriverForFrame(web_contents->GetMainFrame())
                    ->AsWeakPtr(),
            nullptr /* PasswordGenerationPopupObserver*/,
            web_contents,
            native_view) {}

  ~TestPasswordGenerationPopupController() override {}

  PasswordGenerationPopupView* view() { return view_; }
};

class PasswordGenerationPopupViewTest : public InProcessBrowserTest {
 public:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  gfx::NativeView GetNativeView() { return GetWebContents()->GetNativeView(); }

  std::unique_ptr<PasswordGenerationPopupViewTester> GetViewTester() {
    return PasswordGenerationPopupViewTester::For(controller_->view());
  }

 protected:
  TestPasswordGenerationPopupController* controller_;
};

// Regression test for crbug.com/400543. Verifying that moving the mouse in the
// editing dialog doesn't crash.
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       MouseMovementInEditingPopup) {
  controller_ = new autofill::TestPasswordGenerationPopupController(
      GetWebContents(), GetNativeView());
  controller_->Show(PasswordGenerationPopupController::kEditGeneratedPassword);

  GetViewTester()->SimulateMouseMovementAt(gfx::Point(1, 1));

  // Deletes |controller_|.
  controller_->HideAndDestroy();
}

// Verify that we calling Show() with an invalid container does not crash.
// Regression test for crbug.com/439618.
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest, InvalidContainerView) {
  controller_ = new autofill::TestPasswordGenerationPopupController(
      GetWebContents(), NULL);
  controller_->Show(PasswordGenerationPopupController::kOfferGeneration);
}

// Verify that destroying web contents with visible popup does not crash.
IN_PROC_BROWSER_TEST_F(PasswordGenerationPopupViewTest,
                       CloseWebContentsWithVisiblePopup) {
  controller_ = new autofill::TestPasswordGenerationPopupController(
      GetWebContents(), GetNativeView());
  controller_->Show(PasswordGenerationPopupController::kEditGeneratedPassword);
  GetWebContents()->Close();
}

}  // namespace autofill
