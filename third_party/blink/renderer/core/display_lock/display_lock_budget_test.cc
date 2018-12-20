// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_budget.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/display_lock/strict_yielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/display_lock/unyielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

class DisplayLockBudgetTest : public RenderingTest {
  void SetUp() override {
    RenderingTest::SetUp();
    features_backup_.emplace();
    RuntimeEnabledFeatures::SetDisplayLockingEnabled(true);
  }

  void TearDown() override {
    if (features_backup_) {
      features_backup_->Restore();
      features_backup_.reset();
    }
  }

 private:
  base::Optional<RuntimeEnabledFeatures::Backup> features_backup_;
};

TEST_F(DisplayLockBudgetTest, UnyieldingBudget) {
  WeakPersistent<Element> element =
      GetDocument().CreateRawElement(html_names::kDivTag);
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  ASSERT_TRUE(element->GetDisplayLockContext());
  UnyieldingDisplayLockBudget budget(element->GetDisplayLockContext());
  // Check everything twice since it shouldn't matter how many times we ask the
  // unyielding budget, the results should always be the same.
  for (int i = 0; i < 2; ++i) {
    budget.WillStartLifecycleUpdate();
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));
    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    budget.DidPerformPhase(DisplayLockBudget::Phase::kLayout);
    budget.DidPerformPhase(DisplayLockBudget::Phase::kPrePaint);
    budget.DidPerformPhase(DisplayLockBudget::Phase::kPaint);
    EXPECT_FALSE(budget.DidFinishLifecycleUpdate());
  }
}

TEST_F(DisplayLockBudgetTest, StrictYieldingBudget) {
  WeakPersistent<Element> element =
      GetDocument().CreateRawElement(html_names::kDivTag);
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  ASSERT_TRUE(element->GetDisplayLockContext());
  StrictYieldingDisplayLockBudget budget(element->GetDisplayLockContext());
  {
    budget.WillStartLifecycleUpdate();
    // Initially all of the phase checks should return true, since we don't know
    // which phase the system wants to process next.
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));

    // Not doing anything should ensure that we schedule another animation by
    // returning true here.
    EXPECT_TRUE(budget.DidFinishLifecycleUpdate());
  }
  {
    budget.WillStartLifecycleUpdate();
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));

    // Once we perform a phase, its check should remain true, but the rest
    // will be false for this cycle.
    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_FALSE(
        budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));

    // We would need at least one more run to finish everything.
    EXPECT_TRUE(budget.DidFinishLifecycleUpdate());
  }
  {
    budget.WillStartLifecycleUpdate();
    // Run the previous block again, now everything will always return true
    // since the phase we complete here (style) has already been completed
    // before, and we are open to complete a new phase.
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));

    // Since we already befored style before, no new phase has been processed
    // and all phases are allowed to finish.
    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));

    // We would need at least one more run to finish everything.
    EXPECT_TRUE(budget.DidFinishLifecycleUpdate());
  }
  {
    budget.WillStartLifecycleUpdate();
    // On the next run, the checks for phases completed before should always
    // return true, and as before since we haven't completed a new phase, the
    // remainder of the phases should return true for now.
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));

    // This check is the same as in the previous block, but is here to verify
    // that going through DidFinishLifecycleUpdate() and then
    // WillStartLifecycleUpdate() again doesn't change the fact that we should
    // still perform all of the phases at this point.
    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));

    // Let's say layout was clean and we jumped and did prepaint instead, now
    // every phase before and including prepaint should be true, the rest are
    // locked from completing.
    budget.DidPerformPhase(DisplayLockBudget::Phase::kPrePaint);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));

    // We would need at least one more run to finish everything.
    EXPECT_TRUE(budget.DidFinishLifecycleUpdate());
  }
  {
    budget.WillStartLifecycleUpdate();
    // On the last run, we'll complete all phases. Since there is only one
    // remaining phase we haven't done, all of the checks should always return
    // true (it's either an old phase or a first uncompleted phase).
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));

    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));

    budget.DidPerformPhase(DisplayLockBudget::Phase::kLayout);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));

    budget.DidPerformPhase(DisplayLockBudget::Phase::kPrePaint);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));

    budget.DidPerformPhase(DisplayLockBudget::Phase::kPaint);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPaint));

    // Since we completed everything, we should now be returning false here (no
    // more updates needed).
    EXPECT_FALSE(budget.DidFinishLifecycleUpdate());
  }
}

}  // namespace blink
