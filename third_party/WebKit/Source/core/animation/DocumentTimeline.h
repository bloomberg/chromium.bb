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

#ifndef DocumentTimeline_h
#define DocumentTimeline_h

#include "core/animation/AnimationEffect.h"
#include "core/animation/AnimationPlayer.h"
#include "core/dom/Element.h"
#include "core/events/Event.h"
#include "platform/Timer.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class Document;
class TimedItem;

// DocumentTimeline is constructed and owned by Document, and tied to its lifecycle.
class DocumentTimeline : public RefCounted<DocumentTimeline> {
public:
    class PlatformTiming {

    public:
        // Calls DocumentTimeline's wake() method after duration seconds.
        virtual void wakeAfter(double duration) = 0;
        virtual void cancelWake() = 0;
        virtual void serviceOnNextFrame() = 0;
        virtual ~PlatformTiming() { }

    };

    static PassRefPtr<DocumentTimeline> create(Document*, PassOwnPtr<PlatformTiming> = nullptr);
    ~DocumentTimeline();

    void serviceAnimations(AnimationPlayer::UpdateReason);

    // Creates a player attached to this timeline, but without a start time.
    AnimationPlayer* createAnimationPlayer(TimedItem*);
    AnimationPlayer* play(TimedItem*);

    void playerDestroyed(AnimationPlayer* player)
    {
        ASSERT(m_players.contains(player));
        m_players.remove(player);
    }

    // Called from setReadyState() in Document.cpp to set m_zeroTime to
    // performance.timing.domInteractive
    void setZeroTime(double);
    bool hasStarted() const { return !isNull(m_zeroTime); }
    bool hasPendingUpdates() const { return !m_playersNeedingUpdate.isEmpty(); }
    double zeroTime() const { return m_zeroTime; }
    double currentTime(bool& isNull);
    double currentTime();
    double effectiveTime();
    void pauseAnimationsForTesting(double);
    size_t numberOfActiveAnimationsForTesting() const;

    void setOutdatedAnimationPlayer(AnimationPlayer*);
    bool hasOutdatedAnimationPlayer() const { return m_hasOutdatedAnimationPlayer; }

    Document* document() { return m_document; }
    void detachFromDocument();

protected:
    DocumentTimeline(Document*, PassOwnPtr<PlatformTiming>);

private:
    double m_zeroTime;
    Document* m_document;
    // AnimationPlayers which will be updated on the next frame
    // i.e. current, in effect, or had timing changed
    HashSet<RefPtr<AnimationPlayer> > m_playersNeedingUpdate;
    HashSet<AnimationPlayer*> m_players;
    bool m_hasOutdatedAnimationPlayer;

    void wake();

    friend class SMILTimeContainer;
    static const double s_minimumDelay;

    OwnPtr<PlatformTiming> m_timing;

    class DocumentTimelineTiming FINAL : public PlatformTiming {
    public:
        DocumentTimelineTiming(DocumentTimeline* documentTimeline)
            : m_timeline(documentTimeline)
            , m_timer(this, &DocumentTimelineTiming::timerFired)
        {
            ASSERT(m_timeline);
        }

        virtual void wakeAfter(double duration) OVERRIDE;
        virtual void cancelWake() OVERRIDE;
        virtual void serviceOnNextFrame() OVERRIDE;

        void timerFired(Timer<DocumentTimelineTiming>*) { m_timeline->wake(); }

    private:
        DocumentTimeline* m_timeline;
        Timer<DocumentTimelineTiming> m_timer;

    };

    friend class AnimationDocumentTimelineTest;
};

} // namespace

#endif
