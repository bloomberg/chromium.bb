/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/animation/AnimationTimeline.h"

#include "core/animation/AnimationClock.h"
#include "core/animation/ElementAnimations.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"
#include "platform/TraceEvent.h"

namespace blink {

namespace {

bool compareAnimationPlayers(const RefPtrWillBeMember<blink::AnimationPlayer>& left, const RefPtrWillBeMember<blink::AnimationPlayer>& right)
{
    return AnimationPlayer::hasLowerPriority(left.get(), right.get());
}

}

// This value represents 1 frame at 30Hz plus a little bit of wiggle room.
// TODO: Plumb a nominal framerate through and derive this value from that.
const double AnimationTimeline::s_minimumDelay = 0.04;


PassRefPtrWillBeRawPtr<AnimationTimeline> AnimationTimeline::create(Document* document, PassOwnPtrWillBeRawPtr<PlatformTiming> timing)
{
    return adoptRefWillBeNoop(new AnimationTimeline(document, timing));
}

AnimationTimeline::AnimationTimeline(Document* document, PassOwnPtrWillBeRawPtr<PlatformTiming> timing)
    : m_document(document)
    , m_zeroTime(0) // 0 is used by unit tests which cannot initialize from the loader
    , m_zeroTimeInitialized(false)
    , m_playbackRate(1)
    , m_lastCurrentTimeInternal(0)
{
    if (!timing)
        m_timing = adoptPtrWillBeNoop(new AnimationTimelineTiming(this));
    else
        m_timing = timing;

    ASSERT(document);
}

AnimationTimeline::~AnimationTimeline()
{
#if !ENABLE(OILPAN)
    for (const auto& player : m_players)
        player->timelineDestroyed();
#endif
}

void AnimationTimeline::playerAttached(AnimationPlayer& player)
{
    ASSERT(player.timeline() == this);
    ASSERT(!m_players.contains(&player));
    m_players.add(&player);
}

AnimationPlayer* AnimationTimeline::play(AnimationNode* child)
{
    if (!m_document)
        return nullptr;

    RefPtrWillBeRawPtr<AnimationPlayer> player = AnimationPlayer::create(child, this);
    ASSERT(m_players.contains(player.get()));

    player->play();
    ASSERT(m_playersNeedingUpdate.contains(player));

    return player.get();
}

WillBeHeapVector<RefPtrWillBeMember<AnimationPlayer>> AnimationTimeline::getAnimationPlayers()
{
    WillBeHeapVector<RefPtrWillBeMember<AnimationPlayer>> animationPlayers;
    for (const auto& player : m_players) {
        if (player->source() && (player->source()->isCurrent() || player->source()->isInEffect()))
            animationPlayers.append(player);
    }
    std::sort(animationPlayers.begin(), animationPlayers.end(), compareAnimationPlayers);
    return animationPlayers;
}

void AnimationTimeline::wake()
{
    m_timing->serviceOnNextFrame();
}

void AnimationTimeline::serviceAnimations(TimingUpdateReason reason)
{
    TRACE_EVENT0("blink", "AnimationTimeline::serviceAnimations");

    m_lastCurrentTimeInternal = currentTimeInternal();

    m_timing->cancelWake();

    WillBeHeapVector<RawPtrWillBeMember<AnimationPlayer>> players;
    players.reserveInitialCapacity(m_playersNeedingUpdate.size());
    for (RefPtrWillBeMember<AnimationPlayer> player : m_playersNeedingUpdate)
        players.append(player.get());

    std::sort(players.begin(), players.end(), AnimationPlayer::hasLowerPriority);

    for (AnimationPlayer* player : players) {
        if (!player->update(reason))
            m_playersNeedingUpdate.remove(player);
    }

    ASSERT(!hasOutdatedAnimationPlayer());
}

void AnimationTimeline::scheduleNextService()
{
    ASSERT(!hasOutdatedAnimationPlayer());

    double timeToNextEffect = std::numeric_limits<double>::infinity();
    for (const auto& player : m_playersNeedingUpdate) {
        timeToNextEffect = std::min(timeToNextEffect, player->timeToEffectChange());
    }

    if (timeToNextEffect < s_minimumDelay) {
        m_timing->serviceOnNextFrame();
    } else if (timeToNextEffect != std::numeric_limits<double>::infinity()) {
        m_timing->wakeAfter(timeToNextEffect - s_minimumDelay);
    }
}

void AnimationTimeline::AnimationTimelineTiming::wakeAfter(double duration)
{
    m_timer.startOneShot(duration, FROM_HERE);
}

void AnimationTimeline::AnimationTimelineTiming::cancelWake()
{
    m_timer.stop();
}

void AnimationTimeline::AnimationTimelineTiming::serviceOnNextFrame()
{
    if (m_timeline->m_document && m_timeline->m_document->view())
        m_timeline->m_document->view()->scheduleAnimation();
}

DEFINE_TRACE(AnimationTimeline::AnimationTimelineTiming)
{
    visitor->trace(m_timeline);
    AnimationTimeline::PlatformTiming::trace(visitor);
}

double AnimationTimeline::zeroTime()
{
    if (!m_zeroTimeInitialized && m_document && m_document->loader()) {
        m_zeroTime = m_document->loader()->timing().referenceMonotonicTime();
        m_zeroTimeInitialized = true;
    }
    return m_zeroTime;
}

double AnimationTimeline::currentTime(bool& isNull)
{
    return currentTimeInternal(isNull) * 1000;
}

double AnimationTimeline::currentTimeInternal(bool& isNull)
{
    if (!m_document) {
        isNull = true;
        return std::numeric_limits<double>::quiet_NaN();
    }
    double result = m_playbackRate == 0
        ? zeroTime()
        : (document()->animationClock().currentTime() - zeroTime()) * m_playbackRate;
    isNull = std::isnan(result);
    return result;
}

double AnimationTimeline::currentTime()
{
    return currentTimeInternal() * 1000;
}

double AnimationTimeline::currentTimeInternal()
{
    bool isNull;
    return currentTimeInternal(isNull);
}

void AnimationTimeline::setCurrentTime(double currentTime)
{
    setCurrentTimeInternal(currentTime / 1000);
}

void AnimationTimeline::setCurrentTimeInternal(double currentTime)
{
    if (!document())
        return;
    m_zeroTime = m_playbackRate == 0
        ? currentTime
        : document()->animationClock().currentTime() - currentTime / m_playbackRate;
    m_zeroTimeInitialized = true;

    for (const auto& player : m_players) {
        // The Player needs a timing update to pick up a new time.
        player->setOutdated();
        // Any corresponding compositor animation will need to be restarted. Marking the
        // source changed forces this.
        player->setCompositorPending(true);
    }
}

double AnimationTimeline::effectiveTime()
{
    double time = currentTimeInternal();
    return std::isnan(time) ? 0 : time;
}

void AnimationTimeline::pauseAnimationsForTesting(double pauseTime)
{
    for (const auto& player : m_playersNeedingUpdate)
        player->pauseForTesting(pauseTime);
    serviceAnimations(TimingUpdateOnDemand);
}

bool AnimationTimeline::hasOutdatedAnimationPlayer() const
{
    for (const auto& player : m_playersNeedingUpdate) {
        if (player->outdated())
            return true;
    }
    return false;
}

bool AnimationTimeline::needsAnimationTimingUpdate()
{
    return m_playersNeedingUpdate.size() && currentTimeInternal() != m_lastCurrentTimeInternal;
}

void AnimationTimeline::setOutdatedAnimationPlayer(AnimationPlayer* player)
{
    ASSERT(player->outdated());
    m_playersNeedingUpdate.add(player);
    if (m_document && m_document->page() && !m_document->page()->animator().isServicingAnimations())
        m_timing->serviceOnNextFrame();
}

void AnimationTimeline::setPlaybackRate(double playbackRate)
{
    if (!document())
        return;
    double currentTime = currentTimeInternal();
    m_playbackRate = playbackRate;
    m_zeroTime = playbackRate == 0
        ? currentTime
        : document()->animationClock().currentTime() - currentTime / playbackRate;
    m_zeroTimeInitialized = true;

    for (const auto& player : m_players) {
        // Corresponding compositor animation may need to be restarted to pick up
        // the new playback rate. Marking the source changed forces this.
        player->setCompositorPending(true);
    }
}

double AnimationTimeline::playbackRate() const
{
    return m_playbackRate;
}

#if !ENABLE(OILPAN)
void AnimationTimeline::detachFromDocument()
{
    // FIXME: AnimationTimeline should keep Document alive.
    m_document = nullptr;
}
#endif

DEFINE_TRACE(AnimationTimeline)
{
#if ENABLE(OILPAN)
    visitor->trace(m_document);
    visitor->trace(m_timing);
    visitor->trace(m_playersNeedingUpdate);
    visitor->trace(m_players);
#endif
}

} // namespace
