// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_base.h"

#if defined(OS_WIN)
#include <ole2.h>
#endif

#if defined(USE_AURA)
#include "ui/aura/desktop.h"
#include "ui/aura/test/test_stacking_client.h"
#endif

namespace views {

ViewsTestBase::ViewsTestBase()
    : setup_called_(false),
      teardown_called_(false) {
#if defined(OS_WIN)
  OleInitialize(NULL);
#endif
#if defined(USE_AURA)
  new aura::test::TestStackingClient;
#endif
}

ViewsTestBase::~ViewsTestBase() {
#if defined(OS_WIN)
  OleUninitialize();
#endif
  CHECK(setup_called_)
      << "You have overridden SetUp but never called super class's SetUp";
  CHECK(teardown_called_)
      << "You have overrideen TearDown but never called super class's TearDown";
}

void ViewsTestBase::SetUp() {
  testing::Test::SetUp();
  setup_called_ = true;
  if (!views_delegate_.get())
    views_delegate_.reset(new TestViewsDelegate());
}

void ViewsTestBase::TearDown() {
  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunPendingMessages();
  teardown_called_ = true;
  views_delegate_.reset();
  testing::Test::TearDown();
}

void ViewsTestBase::RunPendingMessages() {
#if defined(USE_AURA)
  message_loop_.RunAllPendingWithDispatcher(
      aura::Desktop::GetInstance()->GetDispatcher());
#else
  message_loop_.RunAllPending();
#endif
}

}  // namespace views
