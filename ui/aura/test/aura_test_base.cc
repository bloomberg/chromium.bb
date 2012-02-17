// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_base.h"

#include "ui/aura/env.h"
#include "ui/aura/root_window.h"

namespace aura {
namespace test {

AuraTestBase::AuraTestBase() {
  helper_.InitRootWindow(RootWindow::GetInstance());
}

AuraTestBase::~AuraTestBase() {
  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  helper_.RunAllPendingInMessageLoop(RootWindow::GetInstance());

  // Ensure that we don't use the previously-allocated static RootWindow object
  // later -- on Linux, it holds a reference to our message loop's X connection.
  RootWindow::DeleteInstance();
}

void AuraTestBase::SetUp() {
  testing::Test::SetUp();
  helper_.SetUp();
}

void AuraTestBase::TearDown() {
  helper_.TearDown();
  testing::Test::TearDown();
}

void AuraTestBase::RunAllPendingInMessageLoop() {
  helper_.RunAllPendingInMessageLoop(RootWindow::GetInstance());
}

}  // namespace test
}  // namespace aura
