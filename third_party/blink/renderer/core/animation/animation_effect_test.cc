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

#include "third_party/blink/renderer/core/animation/animation_effect.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/animation/animation_effect_owner.h"
#include "third_party/blink/renderer/core/animation/computed_effect_timing.h"
#include "third_party/blink/renderer/core/animation/optional_effect_timing.h"

namespace blink {

class MockAnimationEffectOwner
    : public GarbageCollectedFinalized<MockAnimationEffectOwner>,
      public AnimationEffectOwner {
  USING_GARBAGE_COLLECTED_MIXIN(MockAnimationEffectOwner);

 public:
  MOCK_CONST_METHOD0(SequenceNumber, unsigned());
  MOCK_CONST_METHOD0(Playing, bool());
  MOCK_CONST_METHOD0(IsEventDispatchAllowed, bool());
  MOCK_CONST_METHOD0(EffectSuppressed, bool());
  MOCK_METHOD0(EffectInvalidated, void());
  MOCK_METHOD0(UpdateIfNecessary, void());
  MOCK_METHOD0(GetAnimation, Animation*());
};

class TestAnimationEffectEventDelegate : public AnimationEffect::EventDelegate {
 public:
  void OnEventCondition(const AnimationEffect& animation_node) override {
    event_triggered_ = true;
  }
  bool RequiresIterationEvents(const AnimationEffect& animation_node) override {
    return true;
  }
  void Reset() { event_triggered_ = false; }
  bool EventTriggered() { return event_triggered_; }

 private:
  bool event_triggered_;
};

class TestAnimationEffect : public AnimationEffect {
 public:
  static TestAnimationEffect* Create(const Timing& specified) {
    return new TestAnimationEffect(specified,
                                   new TestAnimationEffectEventDelegate());
  }

  void UpdateInheritedTime(double time) {
    UpdateInheritedTime(time, kTimingUpdateForAnimationFrame);
  }

  void UpdateInheritedTime(double time, TimingUpdateReason reason) {
    event_delegate_->Reset();
    AnimationEffect::UpdateInheritedTime(time, reason);
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

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(event_delegate_);
    AnimationEffect::Trace(visitor);
  }

 private:
  TestAnimationEffect(const Timing& specified,
                      TestAnimationEffectEventDelegate* event_delegate)
      : AnimationEffect(specified, event_delegate),
        event_delegate_(event_delegate) {}

  Member<TestAnimationEffectEventDelegate> event_delegate_;
  mutable double local_time_;
  mutable double time_to_next_iteration_;
};

TEST(AnimationAnimationEffectTest, Sanity) {
  Timing timing;
  timing.iteration_duration = 2;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(AnimationEffect::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(2, animation_node->ActiveDurationInternal());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(AnimationEffect::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(2, animation_node->ActiveDurationInternal());
  EXPECT_EQ(0.5, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);

  EXPECT_EQ(AnimationEffect::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(2, animation_node->ActiveDurationInternal());
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(3);

  EXPECT_EQ(AnimationEffect::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(2, animation_node->ActiveDurationInternal());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, FillAuto) {
  Timing timing;
  timing.iteration_duration = 1;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, FillForwards) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, FillBackwards) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::BACKWARDS;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_FALSE(animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, FillBoth) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::BOTH;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(2);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, StartDelay) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.start_delay = 0.5;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(0);
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0.5);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1.5);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroIteration) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 0;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_TRUE(IsNull(animation_node->CurrentIteration()));
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, InfiniteIteration) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = std::numeric_limits<double>::infinity();
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_TRUE(IsNull(animation_node->CurrentIteration()));
  EXPECT_FALSE(animation_node->Progress());

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, Iteration) {
  Timing timing;
  timing.iteration_count = 2;
  timing.iteration_duration = 2;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

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

TEST(AnimationAnimationEffectTest, IterationStart) {
  Timing timing;
  timing.iteration_start = 1.2;
  timing.iteration_count = 2.2;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::BOTH;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_NEAR(0.2, animation_node->Progress().value(), 0.000000000000001);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_NEAR(0.2, animation_node->Progress().value(), 0.000000000000001);

  animation_node->UpdateInheritedTime(10);
  EXPECT_EQ(3, animation_node->CurrentIteration());
  EXPECT_NEAR(0.4, animation_node->Progress().value(), 0.000000000000001);
}

TEST(AnimationAnimationEffectTest, IterationAlternate) {
  Timing timing;
  timing.iteration_count = 10;
  timing.iteration_duration = 1;
  timing.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

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

TEST(AnimationAnimationEffectTest, IterationAlternateReverse) {
  Timing timing;
  timing.iteration_count = 10;
  timing.iteration_duration = 1;
  timing.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

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

TEST(AnimationAnimationEffectTest, ZeroDurationSanity) {
  Timing timing;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(AnimationEffect::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(AnimationEffect::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationFillForwards) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationFillBackwards) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::BACKWARDS;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_FALSE(animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationFillBoth) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::BOTH;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationStartDelay) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.start_delay = 0.5;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(0);
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0.5);
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1.5);
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationIterationStartAndCount) {
  Timing timing;
  timing.iteration_start = 0.1;
  timing.iteration_count = 0.2;
  timing.fill_mode = Timing::FillMode::BOTH;
  timing.start_delay = 0.3;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0.1, animation_node->Progress());

  animation_node->UpdateInheritedTime(0.3);
  EXPECT_DOUBLE_EQ(0.3, animation_node->Progress().value());

  animation_node->UpdateInheritedTime(1);
  EXPECT_DOUBLE_EQ(0.3, animation_node->Progress().value());
}

// FIXME: Needs specification work.
TEST(AnimationAnimationEffectTest, ZeroDurationInfiniteIteration) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = std::numeric_limits<double>::infinity();
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_TRUE(IsNull(animation_node->CurrentIteration()));
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationIteration) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 2;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_TRUE(IsNull(animation_node->CurrentIteration()));
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationIterationStart) {
  Timing timing;
  timing.iteration_start = 1.2;
  timing.iteration_count = 2.2;
  timing.fill_mode = Timing::FillMode::BOTH;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_NEAR(0.2, animation_node->Progress().value(), 0.000000000000001);

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(3, animation_node->CurrentIteration());
  EXPECT_NEAR(0.4, animation_node->Progress().value(), 0.000000000000001);

  animation_node->UpdateInheritedTime(10);
  EXPECT_EQ(3, animation_node->CurrentIteration());
  EXPECT_NEAR(0.4, animation_node->Progress().value(), 0.000000000000001);
}

TEST(AnimationAnimationEffectTest, ZeroDurationIterationAlternate) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 2;
  timing.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_TRUE(IsNull(animation_node->CurrentIteration()));
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, ZeroDurationIterationAlternateReverse) {
  Timing timing;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 2;
  timing.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(-1);
  EXPECT_TRUE(IsNull(animation_node->CurrentIteration()));
  EXPECT_FALSE(animation_node->Progress());

  animation_node->UpdateInheritedTime(0);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);
  EXPECT_EQ(1, animation_node->CurrentIteration());
  EXPECT_EQ(1, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, InfiniteDurationSanity) {
  Timing timing;
  timing.iteration_duration = std::numeric_limits<double>::infinity();
  timing.iteration_count = 1;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffect::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffect::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

// FIXME: Needs specification work.
TEST(AnimationAnimationEffectTest, InfiniteDurationZeroIterations) {
  Timing timing;
  timing.iteration_duration = std::numeric_limits<double>::infinity();
  timing.iteration_count = 0;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(0, animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffect::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(AnimationEffect::kPhaseAfter, animation_node->GetPhase());
  EXPECT_EQ(AnimationEffect::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, InfiniteDurationInfiniteIterations) {
  Timing timing;
  timing.iteration_duration = std::numeric_limits<double>::infinity();
  timing.iteration_count = std::numeric_limits<double>::infinity();
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffect::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(1);

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffect::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, InfiniteDurationZeroPlaybackRate) {
  Timing timing;
  timing.iteration_duration = std::numeric_limits<double>::infinity();
  timing.playback_rate = 0;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(0);

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffect::kPhaseActive, animation_node->GetPhase());
  EXPECT_TRUE(animation_node->IsInPlay());
  EXPECT_TRUE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());

  animation_node->UpdateInheritedTime(std::numeric_limits<double>::infinity());

  EXPECT_EQ(std::numeric_limits<double>::infinity(),
            animation_node->ActiveDurationInternal());
  EXPECT_EQ(AnimationEffect::kPhaseAfter, animation_node->GetPhase());
  EXPECT_FALSE(animation_node->IsInPlay());
  EXPECT_FALSE(animation_node->IsCurrent());
  EXPECT_TRUE(animation_node->IsInEffect());
  EXPECT_EQ(0, animation_node->CurrentIteration());
  EXPECT_EQ(0, animation_node->Progress());
}

TEST(AnimationAnimationEffectTest, EndTime) {
  Timing timing;
  timing.start_delay = 1;
  timing.end_delay = 2;
  timing.iteration_duration = 4;
  timing.iteration_count = 2;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);
  EXPECT_EQ(11, animation_node->EndTimeInternal());
}

TEST(AnimationAnimationEffectTest, Events) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_count = 2;
  timing.start_delay = 1;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

  animation_node->UpdateInheritedTime(0.0, kTimingUpdateOnDemand);
  EXPECT_FALSE(animation_node->EventDelegate()->EventTriggered());

  animation_node->UpdateInheritedTime(0.0, kTimingUpdateForAnimationFrame);
  EXPECT_TRUE(animation_node->EventDelegate()->EventTriggered());

  animation_node->UpdateInheritedTime(1.5, kTimingUpdateOnDemand);
  EXPECT_FALSE(animation_node->EventDelegate()->EventTriggered());

  animation_node->UpdateInheritedTime(1.5, kTimingUpdateForAnimationFrame);
  EXPECT_TRUE(animation_node->EventDelegate()->EventTriggered());
}

TEST(AnimationAnimationEffectTest, TimeToEffectChange) {
  Timing timing;
  timing.iteration_duration = 1;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  timing.iteration_start = 0.2;
  timing.iteration_count = 2.5;
  timing.start_delay = 1;
  timing.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  TestAnimationEffect* animation_node = TestAnimationEffect::Create(timing);

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
  EXPECT_EQ(AnimationEffect::kPhaseActive, animation_node->GetPhase());
  EXPECT_EQ(3.4, animation_node->TakeLocalTime());
  EXPECT_TRUE(std::isinf(animation_node->TakeTimeToNextIteration()));

  // Item has finished.
  animation_node->UpdateInheritedTime(3.5);
  EXPECT_EQ(AnimationEffect::kPhaseAfter, animation_node->GetPhase());
  EXPECT_EQ(3.5, animation_node->TakeLocalTime());
  EXPECT_TRUE(std::isinf(animation_node->TakeTimeToNextIteration()));
}

TEST(AnimationAnimationEffectTest, UpdateTiming) {
  Timing timing;
  TestAnimationEffect* effect = TestAnimationEffect::Create(timing);

  EXPECT_EQ(0, effect->getTiming().delay());
  OptionalEffectTiming effect_timing;
  effect_timing.setDelay(2);
  effect->updateTiming(effect_timing);
  EXPECT_EQ(2, effect->getTiming().delay());

  EXPECT_EQ(0, effect->getTiming().endDelay());
  effect_timing = OptionalEffectTiming();
  effect_timing.setEndDelay(0.5);
  effect->updateTiming(effect_timing);
  EXPECT_EQ(0.5, effect->getTiming().endDelay());

  EXPECT_EQ("auto", effect->getTiming().fill());
  effect_timing = OptionalEffectTiming();
  effect_timing.setFill("backwards");
  effect->updateTiming(effect_timing);
  EXPECT_EQ("backwards", effect->getTiming().fill());

  EXPECT_EQ(0, effect->getTiming().iterationStart());
  effect_timing = OptionalEffectTiming();
  effect_timing.setIterationStart(2);
  effect->updateTiming(effect_timing);
  EXPECT_EQ(2, effect->getTiming().iterationStart());

  EXPECT_EQ(1, effect->getTiming().iterations());
  effect_timing = OptionalEffectTiming();
  effect_timing.setIterations(10);
  effect->updateTiming(effect_timing);
  EXPECT_EQ(10, effect->getTiming().iterations());

  EXPECT_EQ("normal", effect->getTiming().direction());
  effect_timing = OptionalEffectTiming();
  effect_timing.setDirection("reverse");
  effect->updateTiming(effect_timing);
  EXPECT_EQ("reverse", effect->getTiming().direction());

  EXPECT_EQ("linear", effect->getTiming().easing());
  effect_timing = OptionalEffectTiming();
  effect_timing.setEasing("ease-in-out");
  effect->updateTiming(effect_timing);
  EXPECT_EQ("ease-in-out", effect->getTiming().easing());

  EXPECT_EQ("auto", effect->getTiming().duration().GetAsString());
  effect_timing = OptionalEffectTiming();
  effect_timing.setDuration(
      UnrestrictedDoubleOrString::FromUnrestrictedDouble(2.5));
  effect->updateTiming(effect_timing);
  EXPECT_EQ(2.5, effect->getTiming().duration().GetAsUnrestrictedDouble());
}

TEST(AnimationAnimationEffectTest, UpdateTimingThrowsWhenExpected) {
  Timing timing;
  TestAnimationEffect* effect = TestAnimationEffect::Create(timing);

  DummyExceptionStateForTesting exception_state;

  // iterationStart must be non-negative
  OptionalEffectTiming effect_timing;
  effect_timing.setIterationStart(-10);
  effect->updateTiming(effect_timing, exception_state);
  EXPECT_TRUE(exception_state.HadException());

  // iterations must be non-negative and non-null.
  exception_state.ClearException();
  effect_timing = OptionalEffectTiming();
  effect_timing.setIterations(-2);
  effect->updateTiming(effect_timing, exception_state);
  EXPECT_TRUE(exception_state.HadException());

  exception_state.ClearException();
  effect_timing = OptionalEffectTiming();
  effect_timing.setIterations(std::numeric_limits<double>::quiet_NaN());
  effect->updateTiming(effect_timing, exception_state);
  EXPECT_TRUE(exception_state.HadException());

  // If it is a number, duration must be non-negative and non-null.
  exception_state.ClearException();
  effect_timing = OptionalEffectTiming();
  effect_timing.setDuration(
      UnrestrictedDoubleOrString::FromUnrestrictedDouble(-100));
  effect->updateTiming(effect_timing, exception_state);
  EXPECT_TRUE(exception_state.HadException());

  exception_state.ClearException();
  effect_timing = OptionalEffectTiming();
  effect_timing.setDuration(UnrestrictedDoubleOrString::FromUnrestrictedDouble(
      std::numeric_limits<double>::quiet_NaN()));
  effect->updateTiming(effect_timing, exception_state);
  EXPECT_TRUE(exception_state.HadException());

  // easing must be a valid timing function
  exception_state.ClearException();
  effect_timing = OptionalEffectTiming();
  effect_timing.setEasing("my-custom-timing-function");
  effect->updateTiming(effect_timing, exception_state);
  EXPECT_TRUE(exception_state.HadException());
}

TEST(AnimationAnimationEffectTest, UpdateTimingInformsOwnerOnChange) {
  Timing timing;
  TestAnimationEffect* effect = TestAnimationEffect::Create(timing);

  MockAnimationEffectOwner* owner = new MockAnimationEffectOwner();
  effect->Attach(owner);

  EXPECT_CALL(*owner, EffectInvalidated()).Times(1);

  OptionalEffectTiming effect_timing;
  effect_timing.setDelay(5);
  effect->updateTiming(effect_timing);
}

TEST(AnimationAnimationEffectTest, UpdateTimingNoChange) {
  Timing timing;
  timing.start_delay = 0;
  timing.end_delay = 5;
  timing.fill_mode = Timing::FillMode::BOTH;
  timing.iteration_start = 0.1;
  timing.iteration_count = 3;
  timing.iteration_duration = 2;
  timing.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  timing.timing_function = CubicBezierTimingFunction::Create(1, 1, 0.3, 0.3);
  TestAnimationEffect* effect = TestAnimationEffect::Create(timing);

  MockAnimationEffectOwner* owner = new MockAnimationEffectOwner();
  effect->Attach(owner);

  // None of the below calls to updateTime should cause the AnimationEffect to
  // update, as they all match the existing timing information.
  EXPECT_CALL(*owner, EffectInvalidated()).Times(0);

  OptionalEffectTiming effect_timing;
  effect->updateTiming(effect_timing);

  effect_timing = OptionalEffectTiming();
  effect_timing.setDelay(0);
  effect->updateTiming(effect_timing);

  effect_timing = OptionalEffectTiming();
  effect_timing.setEndDelay(5000);
  effect_timing.setFill("both");
  effect_timing.setIterationStart(0.1);
  effect->updateTiming(effect_timing);

  effect_timing = OptionalEffectTiming();
  effect_timing.setIterations(3);
  effect_timing.setDuration(
      UnrestrictedDoubleOrString::FromUnrestrictedDouble(2000));
  effect_timing.setDirection("alternate-reverse");
  effect->updateTiming(effect_timing);

  effect_timing = OptionalEffectTiming();
  effect_timing.setEasing("cubic-bezier(1, 1, 0.3, 0.3)");
  effect->updateTiming(effect_timing);
}

}  // namespace blink
