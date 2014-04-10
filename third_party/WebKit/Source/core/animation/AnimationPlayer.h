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

#ifndef AnimationPlayer_h
#define AnimationPlayer_h

#include "core/animation/TimedItem.h"
#include "core/events/EventTarget.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class DocumentTimeline;
class ExceptionState;

class AnimationPlayer FINAL : public RefCounted<AnimationPlayer>, public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(AnimationPlayer);
public:
    enum UpdateReason {
        UpdateOnDemand,
        UpdateForAnimationFrame
    };

    ~AnimationPlayer();
    static PassRefPtr<AnimationPlayer> create(DocumentTimeline&, TimedItem*);

    // Returns whether the player is finished.
    bool update(UpdateReason);

    // timeToEffectChange returns:
    //  infinity  - if this player is no longer in effect
    //  0         - if this player requires an update on the next frame
    //  n         - if this player requires an update after 'n' units of time
    double timeToEffectChange();

    void cancel();

    double currentTime();
    void setCurrentTime(double newCurrentTime);

    bool paused() const { return m_paused && !m_isPausedForTesting; }
    void pause();
    void play();
    void reverse();
    void finish(ExceptionState&);
    bool finished() { return limited(currentTime()); }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(finish);

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    double playbackRate() const { return m_playbackRate; }
    void setPlaybackRate(double);
    const DocumentTimeline* timeline() const { return m_timeline; }
    DocumentTimeline* timeline() { return m_timeline; }

    void timelineDestroyed() { m_timeline = 0; }

    bool hasStartTime() const { return !isNull(m_startTime); }
    double startTime() const { return m_startTime; }
    void setStartTime(double);

    const TimedItem* source() const { return m_content.get(); }
    TimedItem* source() { return m_content.get(); }
    TimedItem* source(bool& isNull) { isNull = !m_content; return m_content.get(); }
    void setSource(TimedItem*);

    double timeLag() { return currentTimeWithoutLag() - currentTime(); }

    // Pausing via this method is not reflected in the value returned by
    // paused() and must never overlap with pausing via pause().
    void pauseForTesting(double pauseTime);
    // This should only be used for CSS
    void unpause();

    void setOutdated();
    bool outdated() { return m_outdated; }

    bool maybeStartAnimationOnCompositor();
    void cancelAnimationOnCompositor();
    bool hasActiveAnimationsOnCompositor();

    class SortInfo {
    public:
        friend class AnimationPlayer;
        bool operator<(const SortInfo& other) const;
        double startTime() const { return m_startTime; }
    private:
        SortInfo(unsigned sequenceNumber, double startTime)
            : m_sequenceNumber(sequenceNumber)
            , m_startTime(startTime)
        { }
        unsigned m_sequenceNumber;
        double m_startTime;
    };

    const SortInfo& sortInfo() const { return m_sortInfo; }

    static bool hasLowerPriority(AnimationPlayer* player1, AnimationPlayer* player2)
    {
        return player1->sortInfo() < player2->sortInfo();
    }

    // Checks if the AnimationStack is the last reference holder to the Player.
    // This won't be needed when AnimationPlayer is moved to Oilpan.
    bool canFree() const;

    virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture = false) OVERRIDE;

private:
    AnimationPlayer(DocumentTimeline&, TimedItem*);
    double sourceEnd() const;
    bool limited(double currentTime) const;
    double currentTimeWithoutLag() const;
    double currentTimeWithLag() const;
    void updateTimingState(double newCurrentTime);
    void updateCurrentTimingState();

    double m_playbackRate;
    double m_startTime;
    double m_holdTime;
    double m_storedTimeLag;

    SortInfo m_sortInfo;

    RefPtr<TimedItem> m_content;
    // FIXME: We should keep the timeline alive and have this as non-null
    // but this is tricky to do without Oilpan
    DocumentTimeline* m_timeline;
    // Reflects all pausing, including via pauseForTesting().
    bool m_paused;
    bool m_held;
    bool m_isPausedForTesting;

    // This indicates timing information relevant to the player has changed by
    // means other than the ordinary progression of time
    bool m_outdated;

    bool m_finished;
};

} // namespace

#endif
