// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_base.h"

#if defined(OS_WIN)
#include <ole2.h>
#endif

#include "ui/aura/desktop.h"
#include "ui/aura/test/test_desktop_delegate.h"
#include "ui/gfx/compositor/test_compositor.h"

namespace aura {
namespace test {

static ui::Compositor* TestCreateCompositor() {
  return new ui::TestCompositor();
}

AuraTestBase::AuraTestBase()
    : setup_called_(false),
      teardown_called_(false) {
#if defined(OS_WIN)
  OleInitialize(NULL);
#endif

  aura::Desktop::set_compositor_factory_for_testing(&TestCreateCompositor);
  // TestDesktopDelegate is owned by the desktop.
  new TestDesktopDelegate();
  Desktop::GetInstance()->Show();
  Desktop::GetInstance()->SetHostSize(gfx::Size(600, 600));
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
  message_loop_.RunAllPending();

  // Ensure that we don't use the previously-allocated static Desktop object
  // later -- on Linux, it holds a reference to our message loop's X connection.
  aura::Desktop::DeleteInstanceForTesting();
  aura::Desktop::set_compositor_factory_for_testing(NULL);
}

void AuraTestBase::SetUp() {
  testing::Test::SetUp();
  setup_called_ = true;
}

void AuraTestBase::TearDown() {
  teardown_called_ = true;
  testing::Test::TearDown();
}

}  // namespace test
}  // namespace aura
