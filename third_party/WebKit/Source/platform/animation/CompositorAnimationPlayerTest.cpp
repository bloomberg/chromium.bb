// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationPlayer.h"

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "platform/animation/CompositorAnimationDelegate.h"
#include "platform/animation/CompositorAnimationPlayerClient.h"
#include "platform/animation/CompositorAnimationTimeline.h"
#include "platform/animation/CompositorTargetProperty.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CompositorAnimationDelegateForTesting : public CompositorAnimationDelegate {
public:
    CompositorAnimationDelegateForTesting() { resetFlags(); }

    void resetFlags()
    {
        m_started = false;
        m_finished = false;
        m_aborted = false;
    }

    void notifyAnimationStarted(double, int) override { m_started = true; }
    void notifyAnimationFinished(double, int) override { m_finished = true; }
    void notifyAnimationAborted(double, int) override { m_aborted = true; }

    bool m_started;
    bool m_finished;
    bool m_aborted;
};

class CompositorAnimationPlayerTestClient : public CompositorAnimationPlayerClient {
public:
    CompositorAnimationPlayerTestClient() : m_player(new CompositorAnimationPlayer) {}

    CompositorAnimationPlayer* compositorPlayer() const override
    {
        return m_player.get();
    }

    scoped_ptr<CompositorAnimationPlayer> m_player;
};


// Test that when the animation delegate is null, the animation player
// doesn't forward the finish notification.
TEST(CompositorAnimationPlayerTest, NullDelegate)
{
    scoped_ptr<CompositorAnimationDelegateForTesting> delegate(new CompositorAnimationDelegateForTesting);

    scoped_ptr<CompositorAnimationPlayer> player(new CompositorAnimationPlayer);
    cc::AnimationPlayer* ccPlayer = player->animationPlayer();

    player->setAnimationDelegate(delegate.get());
    EXPECT_FALSE(delegate->m_finished);

    ccPlayer->NotifyAnimationFinished(base::TimeTicks(), CompositorTargetProperty::SCROLL_OFFSET, 0);
    EXPECT_TRUE(delegate->m_finished);

    delegate->resetFlags();

    player->setAnimationDelegate(nullptr);
    ccPlayer->NotifyAnimationFinished(base::TimeTicks(), CompositorTargetProperty::SCROLL_OFFSET, 0);
    EXPECT_FALSE(delegate->m_finished);
}

TEST(CompositorAnimationPlayerTest, NotifyFromCCAfterCompositorPlayerDeletion)
{
    scoped_ptr<CompositorAnimationDelegateForTesting> delegate(new CompositorAnimationDelegateForTesting);

    scoped_ptr<CompositorAnimationPlayer> player(new CompositorAnimationPlayer);
    scoped_refptr<cc::AnimationPlayer> ccPlayer = player->animationPlayer();

    player->setAnimationDelegate(delegate.get());
    EXPECT_FALSE(delegate->m_finished);

    // Delete CompositorAnimationPlayer. ccPlayer stays alive.
    player = nullptr;

    // No notifications. Doesn't crash.
    ccPlayer->NotifyAnimationFinished(base::TimeTicks(), CompositorTargetProperty::OPACITY, 0);
    EXPECT_FALSE(delegate->m_finished);
}

TEST(CompositorAnimationPlayerTest, CompositorPlayerDeletionDetachesFromCCTimeline)
{
    scoped_ptr<CompositorAnimationTimeline> timeline(new CompositorAnimationTimeline);
    scoped_ptr<CompositorAnimationPlayerTestClient> client(new CompositorAnimationPlayerTestClient);

    scoped_refptr<cc::AnimationTimeline> ccTimeline = timeline->animationTimeline();
    scoped_refptr<cc::AnimationPlayer> ccPlayer = client->m_player->animationPlayer();
    EXPECT_FALSE(ccPlayer->animation_timeline());

    timeline->playerAttached(*client);
    EXPECT_TRUE(ccPlayer->animation_timeline());
    EXPECT_TRUE(ccTimeline->GetPlayerById(ccPlayer->id()));

    // Delete client and CompositorAnimationPlayer while attached to timeline.
    client = nullptr;

    EXPECT_FALSE(ccPlayer->animation_timeline());
    EXPECT_FALSE(ccTimeline->GetPlayerById(ccPlayer->id()));
}

} // namespace blink
