// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/test/views_test_base.h"

#if defined(OS_WIN)
#include <ole2.h>
#endif

namespace views {

ViewsTestBase::ViewsTestBase() {
#if defined(OS_WIN)
  OleInitialize(NULL);
#endif
}

ViewsTestBase::~ViewsTestBase() {
#if defined(OS_WIN)
  OleUninitialize();
#endif
}

void ViewsTestBase::TearDown() {
  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunPendingMessages();
}

}  // namespace views
