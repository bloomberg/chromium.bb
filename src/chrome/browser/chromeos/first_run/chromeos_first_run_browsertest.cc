// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/first_run/first_run_controller.h"
#include "chrome/browser/chromeos/first_run/step_names.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/window.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"
#include "ui/views/window/dialog_delegate.h"

namespace chromeos {
namespace {

class TestModalDialogDelegate : public views::DialogDelegateView {
 public:
  TestModalDialogDelegate() = default;
  ~TestModalDialogDelegate() override = default;

  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_SYSTEM; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestModalDialogDelegate);
};

class CountingEventHandler : public ui::EventHandler {
 public:
  explicit CountingEventHandler(int* mouse_events_registered)
      : mouse_events_registered_(mouse_events_registered) {}

  ~CountingEventHandler() override = default;

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    ++*mouse_events_registered_;
  }

  int* mouse_events_registered_;

  DISALLOW_COPY_AND_ASSIGN(CountingEventHandler);
};

}  // namespace

// The param respectively indicate whether tablet mode and the
// kHideShelfControlsInTabletMode feature are enabled.
class FirstRunUIBrowserTest
    : public InProcessBrowserTest,
      public FirstRunActor::Delegate,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  FirstRunUIBrowserTest()
      : initialized_(false),
        finalized_(false) {
    if (IsHomeButtonHiddenInTabletMode()) {
      // kHideShelfControlsInTabletMode is predicated on hotseat being enabled.
      scoped_feature_list_.InitWithFeatures(
          {ash::features::kHideShelfControlsInTabletMode,
           chromeos::features::kShelfHotseat},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {ash::features::kHideShelfControlsInTabletMode});
    }
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    tray_test_api_ = ash::SystemTrayTestApi::Create();

    if (InTabletMode())
      ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  }

  // FirstRunActor::Delegate overrides.
  void OnActorInitialized() override {
    initialized_ = true;
    if (on_initialized_callback_)
      std::move(on_initialized_callback_).Run();
    controller()->OnActorInitialized();
  }

  void OnNextButtonClicked(const std::string& step_name) override {
    controller()->OnNextButtonClicked(step_name);
  }

  void OnStepShown(const std::string& step_name) override {
    current_step_name_ = step_name;
    if (on_step_shown_callback_)
      std::move(on_step_shown_callback_).Run();
    controller()->OnStepShown(step_name);
  }

  void OnStepHidden(const std::string& step_name) override {
    controller()->OnStepHidden(step_name);
  }

  void OnHelpButtonClicked() override { controller()->OnHelpButtonClicked(); }

  void OnActorFinalized() override {
    finalized_ = true;
    if (on_finalized_callback_)
      std::move(on_finalized_callback_).Run();
    controller()->OnActorFinalized();
  }

  void OnActorDestroyed() override { controller()->OnActorDestroyed(); }

  bool InTabletMode() const { return std::get<0>(GetParam()); }

  bool IsHomeButtonHiddenInTabletMode() const {
    return std::get<1>(GetParam());
  }

  bool IsHomeButtonShown() const {
    return !InTabletMode() || !IsHomeButtonHiddenInTabletMode();
  }

  void LaunchTutorial() {
    chromeos::first_run::LaunchTutorial();
    EXPECT_TRUE(controller() != NULL);
    // Replacing delegate to observe all messages coming from WebUI to
    // controller.
    controller()->actor_->set_delegate(this);
    initialized_ = controller()->actor_->IsInitialized();
  }

  void WaitForInitialization() {
    if (initialized_)
      return;
    WaitUntilCalled(&on_initialized_callback_);
    EXPECT_TRUE(initialized_);
    js().set_web_contents(controller()->web_contents_for_tests_);
  }

  void WaitForStep(const std::string& step_name) {
    if (current_step_name_ == step_name)
      return;
    WaitUntilCalled(&on_step_shown_callback_);
    EXPECT_EQ(current_step_name_, step_name);
  }

  void AdvanceStep() {
    js().Evaluate("cr.FirstRun.currentStep_.nextButton_.click()");
  }

  void WaitForFinalization() {
    if (!finalized_) {
      WaitUntilCalled(&on_finalized_callback_);
      EXPECT_TRUE(finalized_);
    }
  }

  void WaitUntilCalled(base::OnceClosure* callback) {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    *callback = runner->QuitClosure();
    runner->Run();
  }

  test::JSChecker& js() { return js_; }

  FirstRunController* controller() {
    return FirstRunController::GetInstanceForTest();
  }

  bool IsTrayBubbleOpen() { return tray_test_api_->IsTrayBubbleOpen(); }

  views::Widget* GetOverlayWidget() { return controller()->widget_.get(); }

 private:
  std::unique_ptr<ash::SystemTrayTestApi> tray_test_api_;
  std::string current_step_name_;
  bool initialized_;
  bool finalized_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::OnceClosure on_initialized_callback_;
  base::OnceClosure on_step_shown_callback_;
  base::OnceClosure on_finalized_callback_;
  test::JSChecker js_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FirstRunUIBrowserTest,
    ::testing::Combine(
        ::testing::Bool() /*tablet mode*/,
        ::testing::Bool() /*home button hidden in tablet mode*/));

IN_PROC_BROWSER_TEST_P(FirstRunUIBrowserTest, FirstRunFlow) {
  LaunchTutorial();
  WaitForInitialization();

  if (IsHomeButtonShown()) {
    WaitForStep(first_run::kAppListStep);
    EXPECT_FALSE(IsTrayBubbleOpen());
    AdvanceStep();
  }

  WaitForStep(first_run::kTrayStep);
  EXPECT_TRUE(IsTrayBubbleOpen());

  AdvanceStep();
  WaitForFinalization();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(controller(), nullptr);
  EXPECT_FALSE(IsTrayBubbleOpen());
}

// Tests that a modal window doesn't block events to the tutorial. A modal
// window might be open if enterprise policy forces a browser tab to open
// on first login and the web page opens a JavaScript alert.
// See https://crrev.com/99673003
IN_PROC_BROWSER_TEST_P(FirstRunUIBrowserTest, ModalWindowDoesNotBlock) {
  // Start the tutorial.
  LaunchTutorial();
  WaitForInitialization();
  WaitForStep(IsHomeButtonShown() ? first_run::kAppListStep
                                  : first_run::kTrayStep);

  // Simulate the browser opening a modal dialog.
  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      new TestModalDialogDelegate(), /*context=*/nullptr,
      /*parent=*/nullptr);
  modal_dialog->Show();

  // A mouse click is still received by the overlay widget.
  int mouse_events = 0;
  CountingEventHandler handler(&mouse_events);
  aura::Window* overlay_window = GetOverlayWidget()->GetNativeView();
  overlay_window->AddPreTargetHandler(&handler);
  ui::test::EventGenerator event_generator(GetRootWindow(GetOverlayWidget()));
  event_generator.PressLeftButton();
  EXPECT_EQ(mouse_events, 1);

  overlay_window->RemovePreTargetHandler(&handler);
  modal_dialog->Close();
}

// Tests that the escape key cancels the tutorial.
IN_PROC_BROWSER_TEST_P(FirstRunUIBrowserTest, EscapeCancelsTutorial) {
  // Run the tutorial for a couple steps, but don't finish it.
  LaunchTutorial();
  WaitForInitialization();
  if (IsHomeButtonShown()) {
    WaitForStep(first_run::kAppListStep);
    AdvanceStep();
  }
  WaitForStep(first_run::kTrayStep);
  EXPECT_TRUE(IsTrayBubbleOpen());

  // Press the escape key.
  ui::test::EventGenerator event_generator(GetRootWindow(GetOverlayWidget()));
  event_generator.PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  content::RunAllPendingInMessageLoop();

  // The tutorial stopped.
  EXPECT_EQ(controller(), nullptr);
  EXPECT_FALSE(IsTrayBubbleOpen());
}

}  // namespace chromeos
