// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_base.h"

#include "ui/aura/root_window.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_stacking_client.h"

namespace aura {
namespace test {

AuraTestBase::AuraTestBase() {
}

AuraTestBase::~AuraTestBase() {
}

void AuraTestBase::SetUp() {
  testing::Test::SetUp();
  root_window_.reset(new aura::RootWindow);
  gfx::Screen::SetInstance(new aura::TestScreen(root_window_.get()));
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
