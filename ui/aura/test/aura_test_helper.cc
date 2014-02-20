// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_helper.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/default_activation_client.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/env.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_window_tree_client.h"
#include "ui/base/ime/dummy_input_method.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/screen.h"

#if defined(USE_X11)
#include "ui/aura/window_tree_host_x11.h"
#include "ui/base/x/x11_util.h"
#endif

namespace aura {
namespace test {

AuraTestHelper::AuraTestHelper(base::MessageLoopForUI* message_loop)
    : setup_called_(false),
      teardown_called_(false),
      owns_root_window_(false) {
  DCHECK(message_loop);
  message_loop_ = message_loop;
  // Disable animations during tests.
  zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
#if defined(USE_X11)
  test::SetUseOverrideRedirectWindowByDefault(true);
#endif
}

AuraTestHelper::~AuraTestHelper() {
  CHECK(setup_called_)
      << "AuraTestHelper::SetUp() never called.";
  CHECK(teardown_called_)
      << "AuraTestHelper::TearDown() never called.";
}

void AuraTestHelper::SetUp() {
  setup_called_ = true;

  Env::CreateInstance();
  // Unit tests generally don't want to query the system, rather use the state
  // from RootWindow.
  EnvTestHelper(Env::GetInstance()).SetInputStateLookup(
      scoped_ptr<InputStateLookup>());

  ui::InitializeInputMethodForTesting();

  test_screen_.reset(TestScreen::Create());
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, test_screen_.get());
  root_window_.reset(test_screen_->CreateRootWindowForPrimaryDisplay());

  focus_client_.reset(new TestFocusClient);
  client::SetFocusClient(root_window(), focus_client_.get());
  stacking_client_.reset(new TestWindowTreeClient(root_window()));
  activation_client_.reset(
      new client::DefaultActivationClient(root_window()));
  capture_client_.reset(new client::DefaultCaptureClient(root_window()));
  test_input_method_.reset(new ui::DummyInputMethod);
  root_window()->SetProperty(
      client::kRootWindowInputMethodKey,
      test_input_method_.get());

  root_window()->Show();
  // Ensure width != height so tests won't confuse them.
  dispatcher()->host()->SetBounds(gfx::Rect(800, 600));
}

void AuraTestHelper::TearDown() {
  teardown_called_ = true;
  test_input_method_.reset();
  stacking_client_.reset();
  activation_client_.reset();
  capture_client_.reset();
  focus_client_.reset();
  client::SetFocusClient(root_window(), NULL);
  root_window_.reset();
  ui::GestureRecognizer::Reset();
  test_screen_.reset();
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, NULL);

#if defined(USE_X11)
  ui::ResetXCursorCache();
#endif

  ui::ShutdownInputMethodForTesting();

  Env::DeleteInstance();
}

void AuraTestHelper::RunAllPendingInMessageLoop() {
  // TODO(jbates) crbug.com/134753 Find quitters of this RunLoop and have them
  //              use run_loop.QuitClosure().
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

}  // namespace test
}  // namespace aura
