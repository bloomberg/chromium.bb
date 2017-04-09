// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationPlayer.h"

#include "base/time/time.h"
#include "platform/animation/CompositorAnimation.h"
#include "platform/animation/CompositorAnimationDelegate.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/animation/CompositorFloatAnimationCurve.h"
#include "platform/animation/CompositorTargetProperty.h"
#include "platform/testing/CompositorTest.h"

#include <memory>

namespace blink {

class CompositorAnimationDelegateForTesting
    : public CompositorAnimationDelegate {
 public:
  CompositorAnimationDelegateForTesting() { ResetFlags(); }

  void ResetFlags() {
    started_ = false;
    finished_ = false;
    aborted_ = false;
  }

  void NotifyAnimationStarted(double, int) override { started_ = true; }
  void NotifyAnimationFinished(double, int) override { finished_ = true; }
  void NotifyAnimationAborted(double, int) override { aborted_ = true; }

  bool started_;
  bool finished_;
  bool aborted_;
};

class CompositorAnimationPlayerTestClient
    : public CompositorAnimationPlayerClient {
 public:
  CompositorAnimationPlayerTestClient()
      : player_(CompositorAnimationPlayer::Create()) {}

  CompositorAnimationPlayer* CompositorPlayer() const override {
    return player_.get();
  }

  std::unique_ptr<CompositorAnimationPlayer> player_;
};

class CompositorAnimationPlayerTest : public CompositorTest {};

// Test that when the animation delegate is null, the animation player
// doesn't forward the finish notification.
TEST_F(CompositorAnimationPlayerTest, NullDelegate) {
  std::unique_ptr<CompositorAnimationDelegateForTesting> delegate(
      new CompositorAnimationDelegateForTesting);

  std::unique_ptr<CompositorAnimationPlayer> player =
      CompositorAnimationPlayer::Create();
  cc::AnimationPlayer* cc_player = player->CcAnimationPlayer();

  std::unique_ptr<CompositorAnimationCurve> curve =
      CompositorFloatAnimationCurve::Create();
  std::unique_ptr<CompositorAnimation> animation = CompositorAnimation::Create(
      *curve, CompositorTargetProperty::TRANSFORM, 1, 0);
  player->AddAnimation(std::move(animation));

  player->SetAnimationDelegate(delegate.get());
  EXPECT_FALSE(delegate->finished_);

  cc_player->NotifyAnimationFinishedForTesting(
      CompositorTargetProperty::TRANSFORM, 1);
  EXPECT_TRUE(delegate->finished_);

  delegate->ResetFlags();

  player->SetAnimationDelegate(nullptr);
  cc_player->NotifyAnimationFinishedForTesting(
      CompositorTargetProperty::TRANSFORM, 1);
  EXPECT_FALSE(delegate->finished_);
}

TEST_F(CompositorAnimationPlayerTest,
       NotifyFromCCAfterCompositorPlayerDeletion) {
  std::unique_ptr<CompositorAnimationDelegateForTesting> delegate(
      new CompositorAnimationDelegateForTesting);

  std::unique_ptr<CompositorAnimationPlayer> player =
      CompositorAnimationPlayer::Create();
  scoped_refptr<cc::AnimationPlayer> cc_player = player->CcAnimationPlayer();

  std::unique_ptr<CompositorAnimationCurve> curve =
      CompositorFloatAnimationCurve::Create();
  std::unique_ptr<CompositorAnimation> animation = CompositorAnimation::Create(
      *curve, CompositorTargetProperty::OPACITY, 1, 0);
  player->AddAnimation(std::move(animation));

  player->SetAnimationDelegate(delegate.get());
  EXPECT_FALSE(delegate->finished_);

  cc_player->NotifyAnimationFinishedForTesting(
      CompositorTargetProperty::OPACITY, 1);
  EXPECT_TRUE(delegate->finished_);
  delegate->finished_ = false;

  // Delete CompositorAnimationPlayer. ccPlayer stays alive.
  player = nullptr;

  // No notifications. Doesn't crash.
  cc_player->NotifyAnimationFinishedForTesting(
      CompositorTargetProperty::OPACITY, 1);
  EXPECT_FALSE(delegate->finished_);
}

TEST_F(CompositorAnimationPlayerTest,
       CompositorPlayerDeletionDetachesFromCCTimeline) {
  std::unique_ptr<CompositorAnimationTimeline> timeline =
      CompositorAnimationTimeline::Create();
  std::unique_ptr<CompositorAnimationPlayerTestClient> client(
      new CompositorAnimationPlayerTestClient);

  scoped_refptr<cc::AnimationTimeline> cc_timeline =
      timeline->GetAnimationTimeline();
  scoped_refptr<cc::AnimationPlayer> cc_player =
      client->player_->CcAnimationPlayer();
  EXPECT_FALSE(cc_player->animation_timeline());

  timeline->PlayerAttached(*client);
  EXPECT_TRUE(cc_player->animation_timeline());
  EXPECT_TRUE(cc_timeline->GetPlayerById(cc_player->id()));

  // Delete client and CompositorAnimationPlayer while attached to timeline.
  client = nullptr;

  EXPECT_FALSE(cc_player->animation_timeline());
  EXPECT_FALSE(cc_timeline->GetPlayerById(cc_player->id()));
}

}  // namespace blink
