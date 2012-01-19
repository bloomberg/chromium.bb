// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_base.h"

#if defined(OS_WIN)
#include <ole2.h>
#endif

#include "ui/aura/root_window.h"

namespace aura {
namespace test {

AuraTestBase::AuraTestBase()
    : setup_called_(false),
      teardown_called_(false) {
#if defined(OS_WIN)
  OleInitialize(NULL);
#endif

  RootWindow::GetInstance()->Show();
  RootWindow::GetInstance()->SetHostSize(gfx::Size(600, 600));
}

AuraTestBase::~AuraTestBase() {
#if defined(OS_WIN)
  OleUninitialize();
#endif
  CHECK(setup_called_)
      << "You have overridden SetUp but never called super class's SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called super class's TearDown";

  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunAllPendingInMessageLoop();

  // Ensure that we don't use the previously-allocated static RootWindow object
  // later -- on Linux, it holds a reference to our message loop's X connection.
  aura::RootWindow::DeleteInstance();
}

void AuraTestBase::SetUp() {
  testing::Test::SetUp();
  setup_called_ = true;
}

void AuraTestBase::TearDown() {
  teardown_called_ = true;
  testing::Test::TearDown();
}

void AuraTestBase::RunAllPendingInMessageLoop() {
#if !defined(OS_MACOSX)
  message_loop_.RunAllPendingWithDispatcher(
      RootWindow::GetInstance()->GetDispatcher());
#endif
}

}  // namespace test
}  // namespace aura
