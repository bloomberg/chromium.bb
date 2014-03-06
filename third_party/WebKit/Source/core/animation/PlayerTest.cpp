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

#include "config.h"
#include "core/animation/Player.h"

#include "core/animation/ActiveAnimations.h"
#include "core/animation/Animation.h"
#include "core/animation/AnimationClock.h"
#include "core/animation/DocumentTimeline.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "platform/weborigin/KURL.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class AnimationPlayerTest : public ::testing::Test {
protected:
    virtual void SetUp()
    {
        setUpWithoutStartingTimeline();
        startTimeline();
    }

    void setUpWithoutStartingTimeline()
    {
        document = Document::create();
        document->animationClock().resetTimeForTesting();
        timeline = DocumentTimeline::create(document.get());
        player = timeline->createPlayer(0);
        player->setStartTime(0);
        player->setSource(makeAnimation().get());
    }

    void startTimeline()
    {
        timeline->setZeroTime(0);
        updateTimeline(0);
    }

    PassRefPtr<Animation> makeAnimation(double duration = 30, double playbackRate = 1)
    {
        Timing timing;
        timing.iterationDuration = duration;
        timing.playbackRate = playbackRate;
        return Animation::create(nullptr, nullptr, timing);
    }

    bool updateTimeline(double time)
    {
        document->animationClock().updateTime(time);
        // The timeline does not know about our player, so we have to explicitly call update().
        return player->update();
    }

    RefPtr<Document> document;
    RefPtr<DocumentTimeline> timeline;
    RefPtr<Player> player;
    TrackExceptionState exceptionState;
};

TEST_F(AnimationPlayerTest, InitialState)
{
    setUpWithoutStartingTimeline();
    player = timeline->createPlayer(0);
    EXPECT_TRUE(isNull(timeline->currentTime()));
    EXPECT_EQ(0, player->currentTime());
    EXPECT_FALSE(player->paused());
    EXPECT_EQ(1, player->playbackRate());
    EXPECT_EQ(0, player->timeLag());
    EXPECT_FALSE(player->hasStartTime());
    EXPECT_TRUE(isNull(player->startTime()));

    startTimeline();
    player->setStartTime(0);
    EXPECT_EQ(0, timeline->currentTime());
    EXPECT_EQ(0, player->currentTime());
    EXPECT_FALSE(player->paused());
    EXPECT_EQ(1, player->playbackRate());
    EXPECT_EQ(0, player->startTime());
    EXPECT_EQ(0, player->timeLag());
    EXPECT_TRUE(player->hasStartTime());
}


TEST_F(AnimationPlayerTest, CurrentTimeDoesNotSetOutdated)
{
    EXPECT_FALSE(player->outdated());
    EXPECT_EQ(0, player->currentTime());
    EXPECT_FALSE(player->outdated());
    // FIXME: We should split updateTimeline into a version that doesn't update
    // the player and one that does, as most of the tests don't require update()
    // to be called.
    document->animationClock().updateTime(10);
    EXPECT_EQ(10, player->currentTime());
    EXPECT_FALSE(player->outdated());
}

TEST_F(AnimationPlayerTest, SetCurrentTime)
{
    player->setCurrentTime(10);
    EXPECT_EQ(10, player->currentTime());
    updateTimeline(10);
    EXPECT_EQ(20, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetCurrentTimeNegative)
{
    player->setCurrentTime(-10);
    EXPECT_EQ(-10, player->currentTime());
    updateTimeline(20);
    EXPECT_EQ(10, player->currentTime());

    player->setPlaybackRate(-2);
    player->setCurrentTime(-10);
    EXPECT_EQ(-10, player->currentTime());
    updateTimeline(40);
    EXPECT_EQ(-10, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetCurrentTimePastContentEnd)
{
    player->setCurrentTime(50);
    EXPECT_EQ(50, player->currentTime());
    updateTimeline(20);
    EXPECT_EQ(50, player->currentTime());

    player->setPlaybackRate(-2);
    player->setCurrentTime(50);
    EXPECT_EQ(50, player->currentTime());
    updateTimeline(40);
    EXPECT_EQ(10, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetCurrentTimeBeforeTimelineStarted)
{
    setUpWithoutStartingTimeline();
    player->setCurrentTime(5);
    EXPECT_EQ(5, player->currentTime());
    startTimeline();
    updateTimeline(10);
    EXPECT_EQ(15, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetCurrentTimePastContentEndBeforeTimelineStarted)
{
    setUpWithoutStartingTimeline();
    player->setCurrentTime(250);
    EXPECT_EQ(250, player->currentTime());
    startTimeline();
    updateTimeline(10);
    EXPECT_EQ(250, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetCurrentTimeMax)
{
    player->setCurrentTime(std::numeric_limits<double>::max());
    EXPECT_EQ(std::numeric_limits<double>::max(), player->currentTime());
    updateTimeline(100);
    EXPECT_EQ(std::numeric_limits<double>::max(), player->currentTime());
}

TEST_F(AnimationPlayerTest, SetCurrentTimeUnrestrictedDouble)
{
    updateTimeline(10);
    player->setCurrentTime(nullValue());
    EXPECT_EQ(10, player->currentTime());
    player->setCurrentTime(std::numeric_limits<double>::infinity());
    EXPECT_EQ(10, player->currentTime());
    player->setCurrentTime(-std::numeric_limits<double>::infinity());
    EXPECT_EQ(10, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetCurrentTimeBeforeStartTimeSet)
{
    player = timeline->createPlayer(0);
    player->setSource(makeAnimation().get());
    player->setCurrentTime(20);
    EXPECT_EQ(20, player->currentTime());
    updateTimeline(5);
    EXPECT_EQ(20, player->currentTime());
    player->setStartTime(10);
    EXPECT_EQ(15, player->currentTime());
}


TEST_F(AnimationPlayerTest, TimeLag)
{
    player->setCurrentTime(10);
    EXPECT_EQ(-10, player->timeLag());
    updateTimeline(10);
    EXPECT_EQ(-10, player->timeLag());
    player->setCurrentTime(40);
    EXPECT_EQ(-30, player->timeLag());
    updateTimeline(20);
    EXPECT_EQ(-20, player->timeLag());
}


TEST_F(AnimationPlayerTest, SetStartTime)
{
    updateTimeline(20);
    EXPECT_EQ(0, player->startTime());
    EXPECT_EQ(20, player->currentTime());
    player->setStartTime(10);
    EXPECT_EQ(10, player->startTime());
    EXPECT_EQ(10, player->currentTime());
    updateTimeline(30);
    EXPECT_EQ(10, player->startTime());
    EXPECT_EQ(20, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetStartTimeLimitsPlayer)
{
    player->setStartTime(-50);
    EXPECT_EQ(30, player->currentTime());
    player->setPlaybackRate(-1);
    player->setStartTime(-100);
    EXPECT_EQ(0, player->currentTime());
    EXPECT_TRUE(player->finished());
}

TEST_F(AnimationPlayerTest, SetStartTimeOnLimitedPlayer)
{
    updateTimeline(30);
    player->setStartTime(-10);
    EXPECT_EQ(30, player->currentTime());
    player->setCurrentTime(50);
    player->setStartTime(-40);
    EXPECT_EQ(50, player->currentTime());
    EXPECT_TRUE(player->finished());
}

TEST_F(AnimationPlayerTest, SetStartTimeWhilePaused)
{
    updateTimeline(10);
    player->pause();
    player->setStartTime(-40);
    EXPECT_EQ(10, player->currentTime());
    updateTimeline(50);
    player->setStartTime(60);
    EXPECT_EQ(10, player->currentTime());
}


TEST_F(AnimationPlayerTest, PausePlay)
{
    updateTimeline(10);
    player->pause();
    EXPECT_TRUE(player->paused());
    EXPECT_EQ(10, player->currentTime());
    updateTimeline(20);
    player->play();
    EXPECT_FALSE(player->paused());
    EXPECT_EQ(10, player->currentTime());
    updateTimeline(30);
    EXPECT_EQ(20, player->currentTime());
}

TEST_F(AnimationPlayerTest, PauseBeforeTimelineStarted)
{
    setUpWithoutStartingTimeline();
    player->pause();
    EXPECT_TRUE(player->paused());
    player->play();
    EXPECT_FALSE(player->paused());

    player->pause();
    startTimeline();
    updateTimeline(100);
    EXPECT_TRUE(player->paused());
    EXPECT_EQ(0, player->currentTime());
}

TEST_F(AnimationPlayerTest, PauseBeforeStartTimeSet)
{
    player = timeline->createPlayer(0);
    player->setSource(makeAnimation().get());
    updateTimeline(100);
    player->pause();
    updateTimeline(200);
    EXPECT_EQ(0, player->currentTime());

    player->setStartTime(150);
    player->play();
    EXPECT_EQ(0, player->currentTime());
    updateTimeline(220);
    EXPECT_EQ(20, player->currentTime());
}

TEST_F(AnimationPlayerTest, PlayRewindsToStart)
{
    player->setCurrentTime(30);
    player->play();
    EXPECT_EQ(0, player->currentTime());

    player->setCurrentTime(40);
    player->play();
    EXPECT_EQ(0, player->currentTime());

    player->setCurrentTime(-10);
    player->play();
    EXPECT_EQ(0, player->currentTime());
}

TEST_F(AnimationPlayerTest, PlayRewindsToEnd)
{
    player->setPlaybackRate(-1);
    player->play();
    EXPECT_EQ(30, player->currentTime());

    player->setCurrentTime(40);
    player->play();
    EXPECT_EQ(30, player->currentTime());

    player->setCurrentTime(-10);
    player->play();
    EXPECT_EQ(30, player->currentTime());
}

TEST_F(AnimationPlayerTest, PlayWithPlaybackRateZeroDoesNotSeek)
{
    player->setPlaybackRate(0);
    player->play();
    EXPECT_EQ(0, player->currentTime());

    player->setCurrentTime(40);
    player->play();
    EXPECT_EQ(40, player->currentTime());

    player->setCurrentTime(-10);
    player->play();
    EXPECT_EQ(-10, player->currentTime());
}


TEST_F(AnimationPlayerTest, Reverse)
{
    player->setCurrentTime(10);
    player->pause();
    player->reverse();
    EXPECT_FALSE(player->paused());
    EXPECT_EQ(-1, player->playbackRate());
    EXPECT_EQ(10, player->currentTime());
}

TEST_F(AnimationPlayerTest, ReverseDoesNothingWithPlaybackRateZero)
{
    player->setCurrentTime(10);
    player->setPlaybackRate(0);
    player->pause();
    player->reverse();
    EXPECT_TRUE(player->paused());
    EXPECT_EQ(0, player->playbackRate());
    EXPECT_EQ(10, player->currentTime());
}

TEST_F(AnimationPlayerTest, ReverseDoesNotSeekWithNoSource)
{
    player->setSource(0);
    player->setCurrentTime(10);
    player->reverse();
    EXPECT_EQ(10, player->currentTime());
}

TEST_F(AnimationPlayerTest, ReverseSeeksToStart)
{
    player->setCurrentTime(-10);
    player->setPlaybackRate(-1);
    player->reverse();
    EXPECT_EQ(0, player->currentTime());
}

TEST_F(AnimationPlayerTest, ReverseSeeksToEnd)
{
    player->setCurrentTime(40);
    player->reverse();
    EXPECT_EQ(30, player->currentTime());
}

TEST_F(AnimationPlayerTest, ReverseLimitsPlayer)
{
    player->setCurrentTime(40);
    player->setPlaybackRate(-1);
    player->reverse();
    EXPECT_TRUE(player->finished());
    EXPECT_EQ(40, player->currentTime());

    player->setCurrentTime(-10);
    player->reverse();
    EXPECT_TRUE(player->finished());
    EXPECT_EQ(-10, player->currentTime());
}


TEST_F(AnimationPlayerTest, Finish)
{
    player->finish(exceptionState);
    EXPECT_EQ(30, player->currentTime());
    EXPECT_TRUE(player->finished());

    player->setPlaybackRate(-1);
    player->finish(exceptionState);
    EXPECT_EQ(0, player->currentTime());
    EXPECT_TRUE(player->finished());

    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(AnimationPlayerTest, FinishAfterSourceEnd)
{
    player->setCurrentTime(40);
    player->finish(exceptionState);
    EXPECT_EQ(30, player->currentTime());
}

TEST_F(AnimationPlayerTest, FinishBeforeStart)
{
    player->setCurrentTime(-10);
    player->setPlaybackRate(-1);
    player->finish(exceptionState);
    EXPECT_EQ(0, player->currentTime());
}

TEST_F(AnimationPlayerTest, FinishDoesNothingWithPlaybackRateZero)
{
    player->setCurrentTime(10);
    player->setPlaybackRate(0);
    player->finish(exceptionState);
    EXPECT_EQ(10, player->currentTime());
}

TEST_F(AnimationPlayerTest, FinishRaisesException)
{
    Timing timing;
    timing.iterationDuration = 1;
    timing.iterationCount = std::numeric_limits<double>::infinity();
    player->setSource(Animation::create(nullptr, nullptr, timing).get());
    player->setCurrentTime(10);

    player->finish(exceptionState);
    EXPECT_EQ(10, player->currentTime());
    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(InvalidStateError, exceptionState.code());
}


TEST_F(AnimationPlayerTest, LimitingAtSourceEnd)
{
    updateTimeline(30);
    EXPECT_EQ(30, player->currentTime());
    EXPECT_TRUE(player->finished());
    updateTimeline(40);
    EXPECT_EQ(30, player->currentTime());
    EXPECT_FALSE(player->paused());
}

TEST_F(AnimationPlayerTest, LimitingAtStart)
{
    updateTimeline(30);
    player->setPlaybackRate(-2);
    updateTimeline(45);
    EXPECT_EQ(0, player->currentTime());
    EXPECT_TRUE(player->finished());
    updateTimeline(60);
    EXPECT_EQ(0, player->currentTime());
    EXPECT_FALSE(player->paused());
}

TEST_F(AnimationPlayerTest, LimitingWithNoSource)
{
    player->setSource(0);
    EXPECT_TRUE(player->finished());
    updateTimeline(30);
    EXPECT_EQ(0, player->currentTime());
}


TEST_F(AnimationPlayerTest, SetPlaybackRate)
{
    player->setPlaybackRate(2);
    EXPECT_EQ(2, player->playbackRate());
    EXPECT_EQ(0, player->currentTime());
    updateTimeline(10);
    EXPECT_EQ(20, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetPlaybackRateBeforeTimelineStarted)
{
    setUpWithoutStartingTimeline();
    player->setPlaybackRate(2);
    EXPECT_EQ(2, player->playbackRate());
    EXPECT_EQ(0, player->currentTime());
    startTimeline();
    updateTimeline(10);
    EXPECT_EQ(20, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetPlaybackRateWhilePaused)
{
    updateTimeline(10);
    player->pause();
    player->setPlaybackRate(2);
    updateTimeline(20);
    player->play();
    EXPECT_EQ(10, player->currentTime());
    updateTimeline(25);
    EXPECT_EQ(20, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetPlaybackRateWhileLimited)
{
    updateTimeline(40);
    EXPECT_EQ(30, player->currentTime());
    player->setPlaybackRate(2);
    updateTimeline(50);
    EXPECT_EQ(30, player->currentTime());
    player->setPlaybackRate(-2);
    EXPECT_FALSE(player->finished());
    updateTimeline(60);
    EXPECT_EQ(10, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetPlaybackRateZero)
{
    updateTimeline(10);
    player->setPlaybackRate(0);
    EXPECT_EQ(10, player->currentTime());
    updateTimeline(20);
    EXPECT_EQ(10, player->currentTime());
    player->setCurrentTime(20);
    EXPECT_EQ(20, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetPlaybackRateMax)
{
    player->setPlaybackRate(std::numeric_limits<double>::max());
    EXPECT_EQ(std::numeric_limits<double>::max(), player->playbackRate());
    EXPECT_EQ(0, player->currentTime());
    updateTimeline(1);
    EXPECT_EQ(30, player->currentTime());
}


TEST_F(AnimationPlayerTest, SetSource)
{
    player = timeline->createPlayer(0);
    player->setStartTime(0);
    RefPtr<TimedItem> source1 = makeAnimation();
    RefPtr<TimedItem> source2 = makeAnimation();
    player->setSource(source1.get());
    EXPECT_EQ(source1, player->source());
    EXPECT_EQ(0, player->currentTime());
    player->setCurrentTime(15);
    player->setSource(source2.get());
    EXPECT_EQ(15, player->currentTime());
    EXPECT_EQ(0, source1->player());
    EXPECT_EQ(player.get(), source2->player());
    EXPECT_EQ(source2, player->source());
}

TEST_F(AnimationPlayerTest, SetSourceLimitsPlayer)
{
    player->setCurrentTime(20);
    player->setSource(makeAnimation(10).get());
    EXPECT_EQ(20, player->currentTime());
    EXPECT_TRUE(player->finished());
    updateTimeline(10);
    EXPECT_EQ(20, player->currentTime());
}

TEST_F(AnimationPlayerTest, SetSourceUnlimitsPlayer)
{
    player->setCurrentTime(40);
    player->setSource(makeAnimation(60).get());
    EXPECT_FALSE(player->finished());
    EXPECT_EQ(40, player->currentTime());
    updateTimeline(10);
    EXPECT_EQ(50, player->currentTime());
}


TEST_F(AnimationPlayerTest, EmptyPlayersDontUpdateEffects)
{
    player = timeline->createPlayer(0);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->timeToEffectChange());

    updateTimeline(1234);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->timeToEffectChange());
}

TEST_F(AnimationPlayerTest, PlayersDisassociateFromSource)
{
    TimedItem* timedItem = player->source();
    Player* player2 = timeline->createPlayer(timedItem);
    EXPECT_EQ(0, player->source());
    player->setSource(timedItem);
    EXPECT_EQ(0, player2->source());
}

TEST_F(AnimationPlayerTest, PlayersReturnTimeToNextEffect)
{
    Timing timing;
    timing.startDelay = 1;
    timing.iterationDuration = 1;
    timing.endDelay = 1;
    RefPtr<Animation> animation = Animation::create(nullptr, nullptr, timing);
    player = timeline->createPlayer(animation.get());
    player->setStartTime(0);

    updateTimeline(0);
    EXPECT_EQ(1, player->timeToEffectChange());

    updateTimeline(0.5);
    EXPECT_EQ(0.5, player->timeToEffectChange());

    updateTimeline(1);
    EXPECT_EQ(0, player->timeToEffectChange());

    updateTimeline(1.5);
    EXPECT_EQ(0, player->timeToEffectChange());

    updateTimeline(2);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->timeToEffectChange());

    updateTimeline(3);
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->timeToEffectChange());

    player->setCurrentTime(0);
    player->update();
    EXPECT_EQ(1, player->timeToEffectChange());

    player->setPlaybackRate(2);
    player->update();
    EXPECT_EQ(0.5, player->timeToEffectChange());

    player->setPlaybackRate(0);
    player->update();
    EXPECT_EQ(std::numeric_limits<double>::infinity(), player->timeToEffectChange());

    player->setCurrentTime(3);
    player->setPlaybackRate(-1);
    player->update();
    EXPECT_EQ(1, player->timeToEffectChange());

    player->setPlaybackRate(-2);
    player->update();
    EXPECT_EQ(0.5, player->timeToEffectChange());
}

TEST_F(AnimationPlayerTest, AttachedPlayers)
{
    RefPtr<Element> element = document->createElement("foo", ASSERT_NO_EXCEPTION);

    Timing timing;
    RefPtr<Animation> animation = Animation::create(element, nullptr, timing);
    RefPtr<Player> player = timeline->createPlayer(animation.get());
    timeline->serviceAnimations();
    EXPECT_EQ(1U, element->activeAnimations()->players().find(player.get())->value);

    player.release();
    EXPECT_TRUE(element->activeAnimations()->players().isEmpty());
}

TEST_F(AnimationPlayerTest, HasLowerPriority)
{
    // Note that start time defaults to null
    RefPtr<Player> player1 = timeline->createPlayer(0);
    RefPtr<Player> player2 = timeline->createPlayer(0);
    player2->setStartTime(10);
    RefPtr<Player> player3 = timeline->createPlayer(0);
    RefPtr<Player> player4 = timeline->createPlayer(0);
    player4->setStartTime(20);
    RefPtr<Player> player5 = timeline->createPlayer(0);
    player5->setStartTime(10);
    RefPtr<Player> player6 = timeline->createPlayer(0);
    player6->setStartTime(-10);
    Vector<RefPtr<Player> > players;
    players.append(player1);
    players.append(player3);
    players.append(player6);
    players.append(player2);
    players.append(player5);
    players.append(player4);
    for (size_t i = 0; i < players.size(); i++) {
        for (size_t j = 0; j < players.size(); j++)
            EXPECT_EQ(i < j, Player::hasLowerPriority(players[i].get(), players[j].get()));
    }
}

}
