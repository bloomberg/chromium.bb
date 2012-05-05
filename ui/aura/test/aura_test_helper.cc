// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_helper.h"

#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/layer_animator.h"

namespace aura {
namespace test {

AuraTestHelper::AuraTestHelper()
    : setup_called_(false),
      teardown_called_(false) {
  // Disable animations during tests.
  ui::LayerAnimator::set_disable_animations_for_test(true);
}

AuraTestHelper::~AuraTestHelper() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called super class's SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called super class's TearDown";
  aura::Env::DeleteInstance();
}

void AuraTestHelper::InitRootWindow(RootWindow* root_window) {
  root_window->Show();
  // Ensure width != height so tests won't confuse them.
  root_window->SetHostSize(gfx::Size(800, 600));
}

void AuraTestHelper::SetUp() {
  setup_called_ = true;
}

void AuraTestHelper::TearDown() {
  teardown_called_ = true;
}

void AuraTestHelper::RunAllPendingInMessageLoop(RootWindow* root_window) {
#if !defined(OS_MACOSX)
  message_loop_.RunAllPendingWithDispatcher(
      Env::GetInstance()->GetDispatcher());
#endif
}

}  // namespace test
}  // namespace aura
