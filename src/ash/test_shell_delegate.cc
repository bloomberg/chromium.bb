// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test_shell_delegate.h"

#include "ash/accessibility/default_accessibility_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test_screenshot_delegate.h"
#include "ash/wm/gestures/back_gesture/test_back_gesture_contextual_nudge_delegate.h"
#include "ui/gfx/image/image.h"

namespace ash {

TestShellDelegate::TestShellDelegate() = default;

TestShellDelegate::~TestShellDelegate() = default;

bool TestShellDelegate::CanShowWindowForUser(const aura::Window* window) const {
  return true;
}

std::unique_ptr<ScreenshotDelegate>
TestShellDelegate::CreateScreenshotDelegate() {
  return std::make_unique<TestScreenshotDelegate>();
}

AccessibilityDelegate* TestShellDelegate::CreateAccessibilityDelegate() {
  return new DefaultAccessibilityDelegate;
}

std::unique_ptr<BackGestureContextualNudgeDelegate>
TestShellDelegate::CreateBackGestureContextualNudgeDelegate(
    BackGestureContextualNudgeController* controller) {
  return std::make_unique<TestBackGestureContextualNudgeDelegate>(controller);
}

bool TestShellDelegate::CanGoBack(gfx::NativeWindow window) const {
  return can_go_back_;
}

void TestShellDelegate::BindNavigableContentsFactory(
    mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver) {}

void TestShellDelegate::BindMultiDeviceSetup(
    mojo::PendingReceiver<chromeos::multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  if (multidevice_setup_binder_)
    multidevice_setup_binder_.Run(std::move(receiver));
}

void TestShellDelegate::SetCanGoBack(bool can_go_back) {
  can_go_back_ = can_go_back;
}

}  // namespace ash
