// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/MainThreadDebugger.h"

#include <memory>
#include "core/testing/PageTestBase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
class MainThreadDebuggerTest : public PageTestBase {
 public:
  MainThreadDebugger* GetMainThreadDebugger() {
    return MainThreadDebugger::Instance();
  }

 private:
};

TEST_F(MainThreadDebuggerTest, HitBreakPointDuringLifecycle) {
  MainThreadDebugger* debugger = GetMainThreadDebugger();
  Document& document = GetDocument();
  debugger->SetPostponeTransitionScopeForTesting(document);
  EXPECT_TRUE(document.Lifecycle().LifecyclePostponed());

  // The following steps would cause either style update or layout, it should
  // never crash.
  document.View()->ViewportSizeChanged(true, true);
  document.View()->UpdateAllLifecyclePhases();
  document.UpdateStyleAndLayoutIgnorePendingStylesheets();
  document.UpdateStyleAndLayoutTree();

  debugger->ResetPostponeTransitionScopeForTesting();
  EXPECT_FALSE(document.Lifecycle().LifecyclePostponed());
}

}  // namespace blink
