// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_base.h"

#include "ui/aura/env.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/single_monitor_manager.h"
#include "ui/aura/test/test_stacking_client.h"
#include "ui/aura/ui_controls_aura.h"
#include "ui/base/gestures/gesture_configuration.h"
#include "ui/ui_controls/ui_controls.h"

#if defined(USE_ASH)
#include "ui/aura/test/test_screen.h"
#endif

namespace aura {
namespace test {

AuraTestBase::AuraTestBase() {
}

AuraTestBase::~AuraTestBase() {
}

void AuraTestBase::SetUp() {
  testing::Test::SetUp();

  // Changing the parameters for gesture recognition shouldn't cause
  // tests to fail, so we use a separate set of parameters for unit
  // testing.
  ui::GestureConfiguration::use_test_values();

  Env::GetInstance()->SetMonitorManager(new SingleMonitorManager);
  root_window_.reset(Env::GetInstance()->monitor_manager()->
                     CreateRootWindowForPrimaryMonitor());
#if defined(USE_ASH)
  gfx::Screen::SetInstance(new aura::TestScreen(root_window_.get()));
#endif
  ui_controls::InstallUIControlsAura(CreateUIControlsAura(root_window_.get()));
  helper_.InitRootWindow(root_window());
  helper_.SetUp();
  stacking_client_.reset(new TestStackingClient(root_window()));
}

void AuraTestBase::TearDown() {
  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunAllPendingInMessageLoop();

  stacking_client_.reset();
  helper_.TearDown();
  root_window_.reset();
  testing::Test::TearDown();
}

void AuraTestBase::RunAllPendingInMessageLoop() {
  helper_.RunAllPendingInMessageLoop(root_window());
}

}  // namespace test
}  // namespace aura
