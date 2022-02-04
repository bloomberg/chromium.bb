// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test_shell_delegate.h"

#include <memory>

#include "ash/accessibility/default_accessibility_delegate.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/public/cpp/test/test_desks_templates_delegate.h"
#include "ash/public/cpp/test/test_nearby_share_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/wm/gestures/back_gesture/test_back_gesture_contextual_nudge_delegate.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace ash {

TestShellDelegate::TestShellDelegate() = default;

TestShellDelegate::~TestShellDelegate() = default;

bool TestShellDelegate::CanShowWindowForUser(const aura::Window* window) const {
  return true;
}

std::unique_ptr<CaptureModeDelegate>
TestShellDelegate::CreateCaptureModeDelegate() const {
  return std::make_unique<TestCaptureModeDelegate>();
}

AccessibilityDelegate* TestShellDelegate::CreateAccessibilityDelegate() {
  return new DefaultAccessibilityDelegate;
}

std::unique_ptr<BackGestureContextualNudgeDelegate>
TestShellDelegate::CreateBackGestureContextualNudgeDelegate(
    BackGestureContextualNudgeController* controller) {
  return std::make_unique<TestBackGestureContextualNudgeDelegate>(controller);
}

std::unique_ptr<NearbyShareDelegate>
TestShellDelegate::CreateNearbyShareDelegate(
    NearbyShareController* controller) const {
  return std::make_unique<TestNearbyShareDelegate>();
}

std::unique_ptr<DesksTemplatesDelegate>
TestShellDelegate::CreateDesksTemplatesDelegate() const {
  return std::make_unique<TestDesksTemplatesDelegate>();
}

bool TestShellDelegate::CanGoBack(gfx::NativeWindow window) const {
  return can_go_back_;
}

void TestShellDelegate::SetTabScrubberChromeOSEnabled(bool enabled) {
  tab_scrubber_enabled_ = enabled;
}

bool TestShellDelegate::ShouldWaitForTouchPressAck(gfx::NativeWindow window) {
  return should_wait_for_touch_ack_;
}

int TestShellDelegate::GetBrowserWebUITabStripHeight() {
  return 0;
}

void TestShellDelegate::BindMultiDeviceSetup(
    mojo::PendingReceiver<chromeos::multidevice_setup::mojom::MultiDeviceSetup>
        receiver) {
  if (multidevice_setup_binder_)
    multidevice_setup_binder_.Run(std::move(receiver));
}

void TestShellDelegate::SetCanGoBack(bool can_go_back) {
  can_go_back_ = can_go_back;
}

void TestShellDelegate::SetShouldWaitForTouchAck(
    bool should_wait_for_touch_ack) {
  should_wait_for_touch_ack_ = should_wait_for_touch_ack;
}

bool TestShellDelegate::IsSessionRestoreInProgress() const {
  return session_restore_in_progress_;
}

void TestShellDelegate::SetSessionRestoreInProgress(bool in_progress) {
  session_restore_in_progress_ = in_progress;
}

bool TestShellDelegate::IsLoggingRedirectDisabled() const {
  return false;
}

base::FilePath TestShellDelegate::GetPrimaryUserDownloadsFolder() const {
  return base::FilePath();
}

const GURL& TestShellDelegate::GetLastCommittedURLForWindowIfAny(
    aura::Window* window) {
  return last_committed_url_;
}

void TestShellDelegate::SetLastCommittedURLForWindow(const GURL& url) {
  last_committed_url_ = url;
}

}  // namespace ash
