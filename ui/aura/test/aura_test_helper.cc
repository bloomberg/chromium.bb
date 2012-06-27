// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_helper.h"

#include "base/message_loop.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/root_window_capture_client.h"
#include "ui/aura/single_monitor_manager.h"
#include "ui/aura/test/test_activation_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_stacking_client.h"
#include "ui/aura/ui_controls_aura.h"
#include "ui/base/test/dummy_input_method.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/screen.h"
#include "ui/ui_controls/ui_controls.h"

namespace aura {
namespace test {

AuraTestHelper::AuraTestHelper(MessageLoopForUI* message_loop)
    : setup_called_(false),
      teardown_called_(false),
      owns_root_window_(false) {
  DCHECK(message_loop);
  message_loop_ = message_loop;
  // Disable animations during tests.
  ui::LayerAnimator::set_disable_animations_for_test(true);
}

AuraTestHelper::~AuraTestHelper() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called super class's SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called super class's TearDown";
}

void AuraTestHelper::SetUp() {
  setup_called_ = true;
  Env::GetInstance()->SetMonitorManager(new SingleMonitorManager);
  root_window_.reset(aura::MonitorManager::CreateRootWindowForPrimaryMonitor());
  gfx::Screen::SetInstance(new aura::TestScreen(root_window_.get()));
  ui_controls::InstallUIControlsAura(CreateUIControlsAura(root_window_.get()));

  focus_manager_.reset(new FocusManager);
  root_window_->set_focus_manager(focus_manager_.get());
  stacking_client_.reset(new TestStackingClient(root_window_.get()));
  test_activation_client_.reset(
      new test::TestActivationClient(root_window_.get()));
  root_window_capture_client_.reset(
      new shared::RootWindowCaptureClient(root_window_.get()));
  test_input_method_.reset(new ui::test::DummyInputMethod);
  root_window_->SetProperty(
      aura::client::kRootWindowInputMethodKey,
      test_input_method_.get());

  root_window_->Show();
  // Ensure width != height so tests won't confuse them.
  root_window_->SetHostSize(gfx::Size(800, 600));
}

void AuraTestHelper::TearDown() {
  teardown_called_ = true;
  test_input_method_.reset();
  stacking_client_.reset();
  test_activation_client_.reset();
  root_window_capture_client_.reset();
  focus_manager_.reset();
  root_window_.reset();
  aura::Env::DeleteInstance();
}

void AuraTestHelper::RunAllPendingInMessageLoop() {
#if !defined(OS_MACOSX)
  message_loop_->RunAllPendingWithDispatcher(
      Env::GetInstance()->GetDispatcher());
#endif
}

}  // namespace test
}  // namespace aura
