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

#include "core/animation/DocumentTimeline.h"

#include <memory>
#include "core/animation/AnimationClock.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/KeyframeEffectModel.h"
#include "core/animation/PendingAnimations.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/QualifiedName.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/weborigin/KURL.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MockPlatformTiming : public DocumentTimeline::PlatformTiming {
 public:
  MOCK_METHOD1(WakeAfter, void(double));
  MOCK_METHOD0(ServiceOnNextFrame, void());

  DEFINE_INLINE_TRACE() { DocumentTimeline::PlatformTiming::Trace(visitor); }
};

class AnimationDocumentTimelineTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    page_holder = DummyPageHolder::Create();
    document = &page_holder->GetDocument();
    document->GetAnimationClock().ResetTimeForTesting();
    element = Element::Create(QualifiedName::Null(), document.Get());
    platform_timing = new MockPlatformTiming;
    timeline = DocumentTimeline::Create(document.Get(), 0.0, platform_timing);
    timeline->ResetForTesting();
    ASSERT_EQ(0, timeline->CurrentTimeInternal());
  }

  virtual void TearDown() {
    document.Release();
    element.Release();
    timeline.Release();
    ThreadState::Current()->CollectAllGarbage();
  }

  void UpdateClockAndService(double time) {
    document->GetAnimationClock().UpdateTime(time);
    document->GetPendingAnimations().Update(Optional<CompositorElementIdSet>(),
                                            false);
    timeline->ServiceAnimations(kTimingUpdateForAnimationFrame);
    timeline->ScheduleNextService();
  }

  std::unique_ptr<DummyPageHolder> page_holder;
  Persistent<Document> document;
  Persistent<Element> element;
  Persistent<DocumentTimeline> timeline;
  Timing timing;
  Persistent<MockPlatformTiming> platform_timing;

  void Wake() { timeline->Wake(); }

  double MinimumDelay() { return DocumentTimeline::kMinimumDelay; }
};

TEST_F(AnimationDocumentTimelineTest, EmptyKeyframeAnimation) {
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(
          AnimatableValueKeyframeVector());
  KeyframeEffect* keyframe_effect =
      KeyframeEffect::Create(element.Get(), effect, timing);

  timeline->Play(keyframe_effect);

  UpdateClockAndService(0);
  EXPECT_FLOAT_EQ(0, timeline->CurrentTimeInternal());
  EXPECT_FALSE(keyframe_effect->IsInEffect());

  UpdateClockAndService(100);
  EXPECT_FLOAT_EQ(100, timeline->CurrentTimeInternal());
}

TEST_F(AnimationDocumentTimelineTest, EmptyForwardsKeyframeAnimation) {
  AnimatableValueKeyframeEffectModel* effect =
      AnimatableValueKeyframeEffectModel::Create(
          AnimatableValueKeyframeVector());
  timing.fill_mode = Timing::FillMode::FORWARDS;
  KeyframeEffect* keyframe_effect =
      KeyframeEffect::Create(element.Get(), effect, timing);

  timeline->Play(keyframe_effect);

  UpdateClockAndService(0);
  EXPECT_FLOAT_EQ(0, timeline->CurrentTimeInternal());
  EXPECT_TRUE(keyframe_effect->IsInEffect());

  UpdateClockAndService(100);
  EXPECT_FLOAT_EQ(100, timeline->CurrentTimeInternal());
}

TEST_F(AnimationDocumentTimelineTest, ZeroTime) {
  bool is_null;

  document->GetAnimationClock().UpdateTime(100);
  EXPECT_EQ(100, timeline->CurrentTimeInternal());
  EXPECT_EQ(100, timeline->CurrentTimeInternal(is_null));
  EXPECT_FALSE(is_null);

  document->GetAnimationClock().UpdateTime(200);
  EXPECT_EQ(200, timeline->CurrentTimeInternal());
  EXPECT_EQ(200, timeline->CurrentTimeInternal(is_null));
  EXPECT_FALSE(is_null);
}

TEST_F(AnimationDocumentTimelineTest, PlaybackRateNormal) {
  double zero_time = timeline->ZeroTime();
  bool is_null;

  timeline->SetPlaybackRate(1.0);
  EXPECT_EQ(1.0, timeline->PlaybackRate());
  document->GetAnimationClock().UpdateTime(100);
  EXPECT_EQ(zero_time, timeline->ZeroTime());
  EXPECT_EQ(100, timeline->CurrentTimeInternal());
  EXPECT_EQ(100, timeline->CurrentTimeInternal(is_null));
  EXPECT_FALSE(is_null);

  document->GetAnimationClock().UpdateTime(200);
  EXPECT_EQ(zero_time, timeline->ZeroTime());
  EXPECT_EQ(200, timeline->CurrentTimeInternal());
  EXPECT_EQ(200, timeline->CurrentTimeInternal(is_null));
  EXPECT_FALSE(is_null);
}

TEST_F(AnimationDocumentTimelineTest, PlaybackRateNormalWithOriginTime) {
  double origin_time_in_ms = -1000000.0;
  timeline = DocumentTimeline::Create(document.Get(), origin_time_in_ms,
                                      platform_timing);
  timeline->ResetForTesting();

  bool is_null;

  EXPECT_EQ(1.0, timeline->PlaybackRate());
  EXPECT_EQ(-1000, timeline->ZeroTime());
  EXPECT_EQ(1000, timeline->CurrentTimeInternal());
  EXPECT_EQ(1000, timeline->CurrentTimeInternal(is_null));
  EXPECT_FALSE(is_null);

  document->GetAnimationClock().UpdateTime(100);
  EXPECT_EQ(-1000, timeline->ZeroTime());
  EXPECT_EQ(1100, timeline->CurrentTimeInternal());
  EXPECT_EQ(1100, timeline->CurrentTimeInternal(is_null));
  EXPECT_FALSE(is_null);

  document->GetAnimationClock().UpdateTime(200);
  EXPECT_EQ(-1000, timeline->ZeroTime());
  EXPECT_EQ(1200, timeline->CurrentTimeInternal());
  EXPECT_EQ(1200, timeline->CurrentTimeInternal(is_null));
  EXPECT_FALSE(is_null);
}

TEST_F(AnimationDocumentTimelineTest, PlaybackRatePause) {
  bool is_null;

  document->GetAnimationClock().UpdateTime(100);
  EXPECT_EQ(0, timeline->ZeroTime());
  EXPECT_EQ(100, timeline->CurrentTimeInternal());
  EXPECT_EQ(100, timeline->CurrentTimeInternal(is_null));
  EXPECT_FALSE(is_null);

  timeline->SetPlaybackRate(0.0);
  EXPECT_EQ(0.0, timeline->PlaybackRate());
  document->GetAnimationClock().UpdateTime(200);
  EXPECT_EQ(100, timeline->ZeroTime());
  EXPECT_EQ(100, timeline->CurrentTimeInternal());
  EXPECT_EQ(100, timeline->CurrentTimeInternal(is_null));

  timeline->SetPlaybackRate(1.0);
  EXPECT_EQ(1.0, timeline->PlaybackRate());
  document->GetAnimationClock().UpdateTime(400);
  EXPECT_EQ(100, timeline->ZeroTime());
  EXPECT_EQ(300, timeline->CurrentTimeInternal());
  EXPECT_EQ(300, timeline->CurrentTimeInternal(is_null));

  EXPECT_FALSE(is_null);
}

TEST_F(AnimationDocumentTimelineTest, PlaybackRatePauseWithOriginTime) {
  bool is_null;

  double origin_time_in_ms = -1000000.0;
  timeline = DocumentTimeline::Create(document.Get(), origin_time_in_ms,
                                      platform_timing);
  timeline->ResetForTesting();

  EXPECT_EQ(-1000, timeline->ZeroTime());
  EXPECT_EQ(1000, timeline->CurrentTimeInternal());
  EXPECT_EQ(1000, timeline->CurrentTimeInternal(is_null));
  EXPECT_FALSE(is_null);

  document->GetAnimationClock().UpdateTime(100);
  EXPECT_EQ(-1000, timeline->ZeroTime());
  EXPECT_EQ(1100, timeline->CurrentTimeInternal());
  EXPECT_EQ(1100, timeline->CurrentTimeInternal(is_null));
  EXPECT_FALSE(is_null);

  timeline->SetPlaybackRate(0.0);
  EXPECT_EQ(0.0, timeline->PlaybackRate());
  document->GetAnimationClock().UpdateTime(200);
  EXPECT_EQ(1100, timeline->ZeroTime());
  EXPECT_EQ(1100, timeline->CurrentTimeInternal());
  EXPECT_EQ(1100, timeline->CurrentTimeInternal(is_null));

  timeline->SetPlaybackRate(1.0);
  EXPECT_EQ(1.0, timeline->PlaybackRate());
  EXPECT_EQ(-900, timeline->ZeroTime());
  EXPECT_EQ(1100, timeline->CurrentTimeInternal());
  EXPECT_EQ(1100, timeline->CurrentTimeInternal(is_null));

  document->GetAnimationClock().UpdateTime(400);
  EXPECT_EQ(-900, timeline->ZeroTime());
  EXPECT_EQ(1300, timeline->CurrentTimeInternal());
  EXPECT_EQ(1300, timeline->CurrentTimeInternal(is_null));

  EXPECT_FALSE(is_null);
}

TEST_F(AnimationDocumentTimelineTest, PlaybackRateSlow) {
  bool is_null;

  document->GetAnimationClock().UpdateTime(100);
  EXPECT_EQ(0, timeline->ZeroTime());
  EXPECT_EQ(100, timeline->CurrentTimeInternal());
  EXPECT_EQ(100, timeline->CurrentTimeInternal(is_null));
  EXPECT_FALSE(is_null);

  timeline->SetPlaybackRate(0.5);
  EXPECT_EQ(0.5, timeline->PlaybackRate());
  document->GetAnimationClock().UpdateTime(300);
  EXPECT_EQ(-100, timeline->ZeroTime());
  EXPECT_EQ(200, timeline->CurrentTimeInternal());
  EXPECT_EQ(200, timeline->CurrentTimeInternal(is_null));

  timeline->SetPlaybackRate(1.0);
  EXPECT_EQ(1.0, timeline->PlaybackRate());
  document->GetAnimationClock().UpdateTime(400);
  EXPECT_EQ(100, timeline->ZeroTime());
  EXPECT_EQ(300, timeline->CurrentTimeInternal());
  EXPECT_EQ(300, timeline->CurrentTimeInternal(is_null));

  EXPECT_FALSE(is_null);
}

TEST_F(AnimationDocumentTimelineTest, PlaybackRateFast) {
  bool is_null;

  document->GetAnimationClock().UpdateTime(100);
  EXPECT_EQ(0, timeline->ZeroTime());
  EXPECT_EQ(100, timeline->CurrentTimeInternal());
  EXPECT_EQ(100, timeline->CurrentTimeInternal(is_null));
  EXPECT_FALSE(is_null);

  timeline->SetPlaybackRate(2.0);
  EXPECT_EQ(2.0, timeline->PlaybackRate());
  document->GetAnimationClock().UpdateTime(300);
  EXPECT_EQ(50, timeline->ZeroTime());
  EXPECT_EQ(500, timeline->CurrentTimeInternal());
  EXPECT_EQ(500, timeline->CurrentTimeInternal(is_null));

  timeline->SetPlaybackRate(1.0);
  EXPECT_EQ(1.0, timeline->PlaybackRate());
  document->GetAnimationClock().UpdateTime(400);
  EXPECT_EQ(-200, timeline->ZeroTime());
  EXPECT_EQ(600, timeline->CurrentTimeInternal());
  EXPECT_EQ(600, timeline->CurrentTimeInternal(is_null));

  EXPECT_FALSE(is_null);
}

TEST_F(AnimationDocumentTimelineTest, PlaybackRateFastWithOriginTime) {
  bool is_null;

  double origin_time_in_ms = -1000000.0;
  timeline = DocumentTimeline::Create(document.Get(), origin_time_in_ms,
                                      platform_timing);
  timeline->ResetForTesting();

  document->GetAnimationClock().UpdateTime(100);
  EXPECT_EQ(-1000, timeline->ZeroTime());
  EXPECT_EQ(1100, timeline->CurrentTimeInternal());
  EXPECT_EQ(1100, timeline->CurrentTimeInternal(is_null));
  EXPECT_FALSE(is_null);

  timeline->SetPlaybackRate(2.0);
  EXPECT_EQ(2.0, timeline->PlaybackRate());
  EXPECT_EQ(-450, timeline->ZeroTime());
  EXPECT_EQ(1100, timeline->CurrentTimeInternal());
  EXPECT_EQ(1100, timeline->CurrentTimeInternal(is_null));

  document->GetAnimationClock().UpdateTime(300);
  EXPECT_EQ(-450, timeline->ZeroTime());
  EXPECT_EQ(1500, timeline->CurrentTimeInternal());
  EXPECT_EQ(1500, timeline->CurrentTimeInternal(is_null));

  timeline->SetPlaybackRate(1.0);
  EXPECT_EQ(1.0, timeline->PlaybackRate());
  EXPECT_EQ(-1200, timeline->ZeroTime());
  EXPECT_EQ(1500, timeline->CurrentTimeInternal());
  EXPECT_EQ(1500, timeline->CurrentTimeInternal(is_null));

  document->GetAnimationClock().UpdateTime(400);
  EXPECT_EQ(-1200, timeline->ZeroTime());
  EXPECT_EQ(1600, timeline->CurrentTimeInternal());
  EXPECT_EQ(1600, timeline->CurrentTimeInternal(is_null));

  EXPECT_FALSE(is_null);
}

TEST_F(AnimationDocumentTimelineTest, PauseForTesting) {
  float seek_time = 1;
  timing.fill_mode = Timing::FillMode::FORWARDS;
  KeyframeEffect* anim1 =
      KeyframeEffect::Create(element.Get(),
                             AnimatableValueKeyframeEffectModel::Create(
                                 AnimatableValueKeyframeVector()),
                             timing);
  KeyframeEffect* anim2 =
      KeyframeEffect::Create(element.Get(),
                             AnimatableValueKeyframeEffectModel::Create(
                                 AnimatableValueKeyframeVector()),
                             timing);
  Animation* animation1 = timeline->Play(anim1);
  Animation* animation2 = timeline->Play(anim2);
  timeline->PauseAnimationsForTesting(seek_time);

  EXPECT_FLOAT_EQ(seek_time, animation1->CurrentTimeInternal());
  EXPECT_FLOAT_EQ(seek_time, animation2->CurrentTimeInternal());
}

TEST_F(AnimationDocumentTimelineTest, DelayBeforeAnimationStart) {
  timing.iteration_duration = 2;
  timing.start_delay = 5;

  KeyframeEffect* keyframe_effect =
      KeyframeEffect::Create(element.Get(), nullptr, timing);

  timeline->Play(keyframe_effect);

  // TODO: Put the animation startTime in the future when we add the capability
  // to change animation startTime
  EXPECT_CALL(*platform_timing, WakeAfter(timing.start_delay - MinimumDelay()));
  UpdateClockAndService(0);

  EXPECT_CALL(*platform_timing,
              WakeAfter(timing.start_delay - MinimumDelay() - 1.5));
  UpdateClockAndService(1.5);

  EXPECT_CALL(*platform_timing, ServiceOnNextFrame());
  Wake();

  EXPECT_CALL(*platform_timing, ServiceOnNextFrame());
  UpdateClockAndService(4.98);
}

TEST_F(AnimationDocumentTimelineTest, UseAnimationAfterTimelineDeref) {
  Animation* animation = timeline->Play(0);
  timeline.Clear();
  // Test passes if this does not crash.
  animation->setStartTime(0, false);
}

TEST_F(AnimationDocumentTimelineTest, PlayAfterDocumentDeref) {
  timing.iteration_duration = 2;
  timing.start_delay = 5;

  timeline = &document->Timeline();
  document = nullptr;

  KeyframeEffect* keyframe_effect = KeyframeEffect::Create(0, nullptr, timing);
  // Test passes if this does not crash.
  timeline->Play(keyframe_effect);
}

}  // namespace blink
