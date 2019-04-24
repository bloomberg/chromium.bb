// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/display_lock_budget.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_context.h"
#include "third_party/blink/renderer/core/display_lock/strict_yielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/display_lock/unyielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/display_lock/yielding_display_lock_budget.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/wtf/scoped_mock_clock.h"

namespace blink {

class DisplayLockBudgetTest : public RenderingTest {
 public:
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

  double GetBudgetMs(const YieldingDisplayLockBudget& budget) const {
    return budget.GetCurrentBudgetMs();
  }

 private:
  base::Optional<RuntimeEnabledFeatures::Backup> features_backup_;
};

TEST_F(DisplayLockBudgetTest, UnyieldingBudget) {
  // Note that we're not testing the display lock here, just the budget so we
  // can do minimal work to ensure we have a context, ignoring containment and
  // other requirements.
  SetBodyInnerHTML(R"HTML(
    <div id="container"></div>
  )HTML");

  auto* element = GetDocument().getElementById("container");
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  ASSERT_TRUE(element->GetDisplayLockContext());
  UnyieldingDisplayLockBudget budget(element->GetDisplayLockContext());

  // Since the lifecycle is clean, we don't actually need any updates.
  EXPECT_FALSE(budget.NeedsLifecycleUpdates());

  // Dirtying the element will cause us to do updates.
  element->GetLayoutObject()->SetNeedsLayout("");
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());

  // Check everything twice since it shouldn't matter how many times we ask the
  // unyielding budget, the results should always be the same.
  for (int i = 0; i < 2; ++i) {
    budget.WillStartLifecycleUpdate();
    // Note that although we only dirtied layout, all phases "should" complete,
    // since the budget should never be responsible for blocking phases for any
    // reason other than we're out of budget.
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    budget.DidPerformPhase(DisplayLockBudget::Phase::kLayout);
    budget.DidPerformPhase(DisplayLockBudget::Phase::kPrePaint);
    UpdateAllLifecyclePhasesForTest();
    EXPECT_FALSE(budget.NeedsLifecycleUpdates());
  }
}

TEST_F(DisplayLockBudgetTest, StrictYieldingBudget) {
  // Note that we're not testing the display lock here, just the budget so we
  // can do minimal work to ensure we have a context, ignoring containment and
  // other requirements.
  SetBodyInnerHTML(R"HTML(
    <div id="container"></div>
  )HTML");

  auto* element = GetDocument().getElementById("container");
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  ASSERT_TRUE(element->GetDisplayLockContext());
  StrictYieldingDisplayLockBudget budget(element->GetDisplayLockContext());

  // Since the lifecycle is clean, we don't actually need any updates.
  EXPECT_FALSE(budget.NeedsLifecycleUpdates());

  // Dirtying the element will cause us to do updates.
  element->GetLayoutObject()->SetNeedsLayout("");
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());

  {
    budget.WillStartLifecycleUpdate();
    // Initially all of the phase checks should return true, since we don't know
    // which phase the system wants to process next.
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

    // Not doing anything should ensure that we schedule another animation by
    // returning true here.
    EXPECT_TRUE(budget.NeedsLifecycleUpdates());
  }
  {
    budget.WillStartLifecycleUpdate();
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

    // Once we perform a phase, its check should remain true, but the rest
    // will be false for this cycle.
    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_FALSE(
        budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

    // We would need at least one more run to finish everything.
    EXPECT_TRUE(budget.NeedsLifecycleUpdates());
  }
  {
    budget.WillStartLifecycleUpdate();
    // Run the previous block again, now everything will always return true
    // since the phase we complete here (style) has already been completed
    // before, and we are open to complete a new phase.
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

    // Since we already befored style before, no new phase has been processed
    // and all phases are allowed to finish.
    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

    // We would need at least one more run to finish everything.
    EXPECT_TRUE(budget.NeedsLifecycleUpdates());
  }
  {
    budget.WillStartLifecycleUpdate();
    // On the next run, the checks for phases completed before should always
    // return true, and as before since we haven't completed a new phase, the
    // remainder of the phases should return true for now.
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

    // This check is the same as in the previous block, but is here to verify
    // that going through NeedsLifecycleUpdates() and then
    // WillStartLifecycleUpdate() again doesn't change the fact that we should
    // still perform all of the phases at this point.
    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

    // Let's say layout was clean and we jumped and did prepaint instead, now
    // every phase before and including prepaint should be true, the rest are
    // locked from completing.
    budget.DidPerformPhase(DisplayLockBudget::Phase::kPrePaint);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

    // Note that since we processed everything, we no longer need lifecycle
    // updates.
    EXPECT_FALSE(budget.NeedsLifecycleUpdates());
  }
  {
    // Do one more run to ensure everything is still returning true.
    budget.WillStartLifecycleUpdate();
    // On the last run, we'll complete all phases. Since there is only one
    // remaining phase we haven't done, all of the checks should always return
    // true (it's either an old phase or a first uncompleted phase).
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

    budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

    budget.DidPerformPhase(DisplayLockBudget::Phase::kLayout);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

    budget.DidPerformPhase(DisplayLockBudget::Phase::kPrePaint);
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

    // Since we completed everything, we should now be returning false here (no
    // more updates needed).
    EXPECT_FALSE(budget.NeedsLifecycleUpdates());
  }
}

TEST_F(DisplayLockBudgetTest,
       StrictYieldingBudgetOnlyNeedsUpdatesForDirtyPhases) {
  // Note that we're not testing the display lock here, just the budget so we
  // can do minimal work to ensure we have a context, ignoring containment and
  // other requirements.
  SetBodyInnerHTML(R"HTML(
    <div id="container"></div>
  )HTML");

  auto* element = GetDocument().getElementById("container");
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  ASSERT_TRUE(element->GetDisplayLockContext());
  StrictYieldingDisplayLockBudget budget(element->GetDisplayLockContext());

  // Since the lifecycle is clean, we don't actually need any updates.
  EXPECT_FALSE(budget.NeedsLifecycleUpdates());

  // Dirtying the element will cause us to do updates.
  element->GetLayoutObject()->SetNeedsLayout("");
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());

  // Cleaning the lifecycle phases makes the budget not want any more updates.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(budget.NeedsLifecycleUpdates());

  element->GetLayoutObject()->SetNeedsLayout("");
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());
  budget.WillStartLifecycleUpdate();

  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
  budget.DidPerformPhase(DisplayLockBudget::Phase::kLayout);
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());

  budget.WillStartLifecycleUpdate();
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
  budget.DidPerformPhase(DisplayLockBudget::Phase::kPrePaint);

  // Note that since the layout was indicated as done (from the budget
  // perspective), it will no longer need updates even though the true layout is
  // dirty. This is because it will no longer block layout from synchronously
  // completing whenever necessary.
  EXPECT_FALSE(budget.NeedsLifecycleUpdates());
}

TEST_F(DisplayLockBudgetTest, YieldingBudget) {
  // Note that we're not testing the display lock here, just the budget so we
  // can do minimal work to ensure we have a context, ignoring containment and
  // other requirements.
  SetBodyInnerHTML(R"HTML(
    <div id="container"></div>
  )HTML");

  auto* element = GetDocument().getElementById("container");
  {
    auto* script_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState::Scope scope(script_state);
    element->getDisplayLockForBindings()->acquire(script_state, nullptr);
  }

  ASSERT_TRUE(element->GetDisplayLockContext());
  YieldingDisplayLockBudget budget(element->GetDisplayLockContext());

  WTF::ScopedMockClock clock;

  // Since the lifecycle is clean, we don't actually need any updates.
  EXPECT_FALSE(budget.NeedsLifecycleUpdates());

  // Dirtying the element will cause us to do updates.
  element->GetLayoutObject()->SetNeedsLayout("");
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());

  budget.WillStartLifecycleUpdate();
  // Initially all of the phase checks should return true, since we don't know
  // which phase the system wants to process next.
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

  // Not doing anything should ensure that we schedule another animation by
  // returning true here.
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());

  // Advancing the clock a bit will make us still want to the phases.
  clock.Advance(TimeDelta::FromMillisecondsD(GetBudgetMs(budget) / 2));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

  // However, once we're out of budget, we will not do anything.
  clock.Advance(TimeDelta::FromMillisecondsD(GetBudgetMs(budget)));
  EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
  EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
  EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

  // Starting a new lifecycle will reset the budget.
  budget.WillStartLifecycleUpdate();
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

  // Performing a phase still keeps the rest of the phases available for work
  // since we haven't advanced the clock.
  budget.DidPerformPhase(DisplayLockBudget::Phase::kStyle);
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

  // Now that we're out of budget, phases performed previously should remain
  // true.
  clock.Advance(TimeDelta::FromMillisecondsD(GetBudgetMs(budget) * 2));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
  EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
  EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

  // Sanity check here: the element still needs layout.
  EXPECT_TRUE(budget.NeedsLifecycleUpdates());

  // Resetting the budget, and advnacing again should yield the same results as
  // before.
  budget.WillStartLifecycleUpdate();
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
  clock.Advance(TimeDelta::FromMillisecondsD(GetBudgetMs(budget) * 2));
  EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
  EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
  EXPECT_FALSE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));

  // Eventually the budget becomes essentially infinite.
  for (int i = 0; i < 60; ++i)
    budget.WillStartLifecycleUpdate();

  EXPECT_GT(GetBudgetMs(budget), 1e6);
  for (int i = 0; i < 60; ++i) {
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kStyle));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kLayout));
    EXPECT_TRUE(budget.ShouldPerformPhase(DisplayLockBudget::Phase::kPrePaint));
    clock.Advance(TimeDelta::FromMillisecondsD(10000));
  }
}
}  // namespace blink
