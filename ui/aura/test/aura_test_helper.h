// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_AURA_TEST_HELPER_H_
#define UI_AURA_TEST_AURA_TEST_HELPER_H_
#pragma once

#include "base/basictypes.h"
#include "base/message_loop.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

namespace aura {
class RootWindow;
namespace test {

// A helper class owned by tests that does common initialization required for
// Aura use. This class must create no special environment (e.g. no RootWindow)
// since different users will want their own specific environments and will
// set that up themselves.
class AuraTestHelper {
 public:
  AuraTestHelper();
  virtual ~AuraTestHelper();

  // Initializes (shows and sizes) the provided RootWindow for use in tests.
  void InitRootWindow(RootWindow* root_window);

  void SetUp();
  void TearDown();

  // Flushes message loop.
  void RunAllPendingInMessageLoop(RootWindow* root_window);

  MessageLoopForUI* message_loop() { return &message_loop_; }

 private:
  MessageLoopForUI message_loop_;
  bool setup_called_;
  bool teardown_called_;

#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AuraTestHelper);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_AURA_TEST_HELPER_H_
