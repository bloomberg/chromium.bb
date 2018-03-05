// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/MainThreadDebugger.h"

#include <memory>
#include "core/testing/PageTestBase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
class MainThreadDebuggerTest : public PageTestBase {
};

TEST_F(MainThreadDebuggerTest, HitBreakPointDuringLifecycle) {
  Document& document = GetDocument();
  std::unique_ptr<DocumentLifecycle::PostponeTransitionScope>
      postponed_transition_scope =
          std::make_unique<DocumentLifecycle::PostponeTransitionScope>(
              document.Lifecycle());
  EXPECT_TRUE(document.Lifecycle().LifecyclePostponed());

  // The following steps would cause either style update or layout, it should
  // never crash.
  document.View()->ViewportSizeChanged(true, true);
  document.View()->UpdateAllLifecyclePhases();
  document.UpdateStyleAndLayoutIgnorePendingStylesheets();
  document.UpdateStyleAndLayoutTree();

  postponed_transition_scope.reset();
  EXPECT_FALSE(document.Lifecycle().LifecyclePostponed());
}

}  // namespace blink
