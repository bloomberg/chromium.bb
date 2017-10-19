/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/animation/AnimationEffectReadOnly.h"

#include "core/animation/ComputedTimingProperties.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TestAnimationEffectEventDelegate
    : public AnimationEffectReadOnly::EventDelegate {
 public:
  void OnEventCondition(
      const AnimationEffectReadOnly& animation_node) override {
    event_triggered_ = true;
  }
  bool RequiresIterationEvents(
      const AnimationEffectReadOnly& animation_node) override {
    return true;
  }
  void Reset() { event_triggered_ = false; }
  bool EventTriggered() { return event_triggered_; }

 private:
  bool event_triggered_;
};

class TestAnimationEffectReadOnly : public AnimationEffectReadOnly {
 public:
  static TestAnimationEffectReadOnly* Create(const Timing& specified) {
    return new TestAnimationEffectReadOnly(
        specified, new TestAnimationEffectEventDelegate());
  }

  void UpdateInheritedTime(double time) {
    UpdateInheritedTime(time, kTimingUpdateForAnimationFrame);
  }

  void UpdateInheritedTime(double time, TimingUpdateReason reason) {
    event_delegate_->Reset();
    AnimationEffectReadOnly::UpdateInheritedTime(time, reason);
  }

  void UpdateChildrenAndEffects() const override {}
  void WillDetach() {}
  TestAnimationEffectEventDelegate* EventDelegate() {
    return event_delegate_.Get();
  }
  double CalculateTimeToEffectChange(
      bool forwards,
      double local_time,
      double time_to_next_iteration) const override {
    local_time_ = local_time;
    time_to_next_iteration_ = time_to_next_iteration;
    return -1;
  }
  double TakeLocalTime() {
    const double result = local_time_;
    local_time_ = NullValue();
    return result;
  }

  double TakeTimeToNextIteration() {
    const double result = time_to_next_iteration_;
    time_to_next_iteration_ = NullValue();
    return result;
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(event_delegate_);
    AnimationEffectReadOnly::Trace(visitor);
  }

 private:
  TestAnimationEffectReadOnly(const Timing& specified,
                              TestAnimationEffectEventDelegate* event_delegate)
      : AnimationEffectReadOnly(specified, event_delegate),
        event_delegate_(event_delegate) {}

  Member<TestAnimationEffectEventDelegate> event_delegate_;
  mutable double local_time_;
  mutable double time_to_next_iteration_;
};

TEST(AnimationAnimationEffectReadOnlyTest, Sanity) {
  Timing timing;
  timing.iteration_duration = 2;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(AnimationEffectReadOnly::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(2, animation_node->ActiveDurationInternal());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(AnimationEffectReadOnly::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(2, animation_node->ActiveDurationInternal());
  EXPECT_EQ(0.5, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);

  EXPECT_EQ(AnimationEffectReadOnly::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(2, animation_node->ActiveDurationInternal());
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(3);

  EXPECT_EQ(AnimationEffectReadOnly::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(2, animation_node->ActiveDurationInternal());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, FillAuto) {
  Timing timing;
  timing.iteration_duration = 1;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, FillForwards) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_TRUE(IsNull(animation_node->Progress()));

  animation_node->UpdateInheritedTime(2);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, FillBackwards) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::BACKWARDS;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_TRUE(IsNull(animation_node->Progress()));
}

TEST(AnimationAnimationEffectReadOnlyTest, FillBoth) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::BOTH;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, StartDelay) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.start_delay = 0.5;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0);
  EXPECT_TRUE(IsNull(animation_node->Progress()));

  animation_node->UpdateInheritedTime(0.5);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1.5);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, ZeroIteration) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 0;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_TRUE(IsNull(animation_node->CurrentIteration()));
  EXPECT_TRUE(IsNull(animation_node->Progress()));

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, InfiniteIteration) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = std::numeric_limits<double>::infinity();
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_TRUE(IsNull(animation_node->CurrentIteration()));
  EXPECT_TRUE(IsNull(animation_node->Progress()));

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, Iteration) {
  Timing timing;
  timing.iteration_count = 2;
  timing.iteration_duration = 2;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0.5, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(5);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, IterationStart) {
  Timing timing;
  timing.iteration_start = 1.2;
  timing.iteration_count = 2.2;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::BOTH;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_NEAR(0.2, animation_node->Progress(), 0.000000000000001);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_NEAR(0.2, animation_node->Progress(), 0.000000000000001);

  animation_node->UpdateInheritedTime(10);
  EXPECT_EQ(3, animation_node->CurrentIteration());
  EXPECT_NEAR(0.4, animation_node->Progress(), 0.000000000000001);
}

TEST(AnimationAnimationEffectReadOnlyTest, IterationAlternate) {
  Timing timing;
  timing.iteration_count = 10;
  timing.iteration_duration = 1;
  timing.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0.75);
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0.75, animation_node->Progress());

  animation_node->UpdateInheritedTime(1.75);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0.25, animation_node->Progress());

  animation_node->UpdateInheritedTime(2.75);
  EXPECT_EQ(2, animation_node->CurrentIteration());
  EXPECT_EQ(0.75, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, IterationAlternateReverse) {
  Timing timing;
  timing.iteration_count = 10;
  timing.iteration_duration = 1;
  timing.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0.75);
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0.25, animation_node->Progress());

  animation_node->UpdateInheritedTime(1.75);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0.75, animation_node->Progress());

  animation_node->UpdateInheritedTime(2.75);
  EXPECT_EQ(2, animation_node->CurrentIteration());
  EXPECT_EQ(0.25, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, ZeroDurationSanity) {
  Timing timing;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(AnimationEffectReadOnly::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(AnimationEffectReadOnly::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, ZeroDurationFillForwards) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_TRUE(IsNull(animation_node->Progress()));

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, ZeroDurationFillBackwards) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::BACKWARDS;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_TRUE(IsNull(animation_node->Progress()));

  animation_node->UpdateInheritedTime(1);
  EXPECT_TRUE(IsNull(animation_node->Progress()));
}

TEST(AnimationAnimationEffectReadOnlyTest, ZeroDurationFillBoth) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::BOTH;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, ZeroDurationStartDelay) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.start_delay = 0.5;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0);
  EXPECT_TRUE(IsNull(animation_node->Progress()));

  animation_node->UpdateInheritedTime(0.5);
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1.5);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, ZeroDurationIterationStartAndCount) {
  Timing timing;
  timing.iteration_start = 0.1;
  timing.iteration_count = 0.2;
  timing.fill_mode = Timing::FillMode::BOTH;
  timing.start_delay = 0.3;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0.1, animation_node->Progress());

  animation_node->UpdateInheritedTime(0.3);
  EXPECT_DOUBLE_EQ(0.3, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_DOUBLE_EQ(0.3, animation_node->Progress());
}

// FIXME: Needs specification work.
TEST(AnimationAnimationEffectReadOnlyTest, ZeroDurationInfiniteIteration) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = std::numeric_limits<double>::infinity();
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_TRUE(IsNull(animation_node->CurrentIteration()));
  EXPECT_TRUE(IsNull(animation_node->Progress()));

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, ZeroDurationIteration) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 2;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_TRUE(IsNull(animation_node->CurrentIteration()));
  EXPECT_TRUE(IsNull(animation_node->Progress()));

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, ZeroDurationIterationStart) {
  Timing timing;
  timing.iteration_start = 1.2;
  timing.iteration_count = 2.2;
  timing.fill_mode = Timing::FillMode::BOTH;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_NEAR(0.2, animation_node->Progress(), 0.000000000000001);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(3, animation_node->CurrentIteration());
  EXPECT_NEAR(0.4, animation_node->Progress(), 0.000000000000001);

  animation_node->UpdateInheritedTime(10);
  EXPECT_EQ(3, animation_node->CurrentIteration());
  EXPECT_NEAR(0.4, animation_node->Progress(), 0.000000000000001);
}

TEST(AnimationAnimationEffectReadOnlyTest, ZeroDurationIterationAlternate) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 2;
  timing.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_TRUE(IsNull(animation_node->CurrentIteration()));
  EXPECT_TRUE(IsNull(animation_node->Progress()));

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest,
     ZeroDurationIterationAlternateReverse) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 2;
  timing.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_TRUE(IsNull(animation_node->CurrentIteration()));
  EXPECT_TRUE(IsNull(animation_node->Progress()));

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, InfiniteDurationSanity) {
  Timing timing;
  timing.iteration_duration = std::numeric_limits<double>::infinity();
  timing.iteration_count = 1;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffectReadOnly::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffectReadOnly::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

// FIXME: Needs specification work.
TEST(AnimationAnimationEffectReadOnlyTest, InfiniteDurationZeroIterations) {
  Timing timing;
  timing.iteration_duration = std::numeric_limits<double>::infinity();
  timing.iteration_count = 0;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffectReadOnly::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(AnimationEffectReadOnly::kPhaseAfter, animation_node->GetPhase());
  EXPECT_EQ(AnimationEffectReadOnly::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, InfiniteDurationInfiniteIterations) {
  Timing timing;
  timing.iteration_duration = std::numeric_limits<double>::infinity();
  timing.iteration_count = std::numeric_limits<double>::infinity();
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffectReadOnly::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffectReadOnly::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, InfiniteDurationZeroPlaybackRate) {
  Timing timing;
  timing.iteration_duration = std::numeric_limits<double>::infinity();
  timing.playback_rate = 0;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffectReadOnly::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(std::numeric_limits<double>::infinity());

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffectReadOnly::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectReadOnlyTest, EndTime) {
  Timing timing;
  timing.start_delay = 1;
  timing.end_delay = 2;
  timing.iteration_duration = 4;
  timing.iteration_count = 2;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);
  EXPECT_EQ(11, animation_node->EndTimeInternal());
}

TEST(AnimationAnimationEffectReadOnlyTest, Events) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 2;
  timing.start_delay = 1;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0.0, kTimingUpdateOnDemand);
  EXPECT_FALSE(animation_node->EventDelegate()->EventTriggered());

  animation_node->UpdateInheritedTime(0.0, kTimingUpdateForAnimationFrame);
  EXPECT_TRUE(animation_node->EventDelegate()->EventTriggered());

  animation_node->UpdateInheritedTime(1.5, kTimingUpdateOnDemand);
  EXPECT_FALSE(animation_node->EventDelegate()->EventTriggered());

  animation_node->UpdateInheritedTime(1.5, kTimingUpdateForAnimationFrame);
  EXPECT_TRUE(animation_node->EventDelegate()->EventTriggered());
}

TEST(AnimationAnimationEffectReadOnlyTest, TimeToEffectChange) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_start = 0.2;
  timing.iteration_count = 2.5;
  timing.start_delay = 1;
  timing.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  TestAnimationEffectReadOnly* animation_node =
      TestAnimationEffectReadOnly::Create(timing);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0, animation_node->TakeLocalTime());
  EXPECT_TRUE(std::isinf(animation_node->TakeTimeToNextIteration()));

  // Normal iteration.
  animation_node->UpdateInheritedTime(1.75);
  EXPECT_EQ(1.75, animation_node->TakeLocalTime());
  EXPECT_NEAR(0.05, animation_node->TakeTimeToNextIteration(),
              0.000000000000001);

  // Reverse iteration.
  animation_node->UpdateInheritedTime(2.75);
  EXPECT_EQ(2.75, animation_node->TakeLocalTime());
  EXPECT_NEAR(0.05, animation_node->TakeTimeToNextIteration(),
              0.000000000000001);

  // Item ends before iteration finishes.
  animation_node->UpdateInheritedTime(3.4);
  EXPECT_EQ(AnimationEffectReadOnly::kPhaseActive, animation_node->GetPhase());
  EXPECT_EQ(3.4, animation_node->TakeLocalTime());
  EXPECT_TRUE(std::isinf(animation_node->TakeTimeToNextIteration()));

  // Item has finished.
  animation_node->UpdateInheritedTime(3.5);
  EXPECT_EQ(AnimationEffectReadOnly::kPhaseAfter, animation_node->GetPhase());
  EXPECT_EQ(3.5, animation_node->TakeLocalTime());
  EXPECT_TRUE(std::isinf(animation_node->TakeTimeToNextIteration()));
}

}  // namespace blink
