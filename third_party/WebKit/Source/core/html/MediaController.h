/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaController_h
#define MediaController_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/events/Event.h"
#include "core/events/EventTarget.h"
#include "core/html/HTMLMediaElement.h"
#include "platform/Timer.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class Clock;
class Event;
class ExceptionState;
class ExecutionContext;

class MediaController FINAL : public RefCounted<MediaController>, public ScriptWrappable, public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(MediaController);
public:
    static PassRefPtr<MediaController> create(ExecutionContext*);
    virtual ~MediaController();

    void addMediaElement(HTMLMediaElement*);
    void removeMediaElement(HTMLMediaElement*);
    bool containsMediaElement(HTMLMediaElement*) const;

    PassRefPtr<TimeRanges> buffered() const;
    PassRefPtr<TimeRanges> seekable() const;
    PassRefPtr<TimeRanges> played();

    double duration() const;
    double currentTime() const;
    void setCurrentTime(double, ExceptionState&);

    bool paused() const { return m_paused; }
    void play();
    void pause();
    void unpause();

    double defaultPlaybackRate() const { return m_defaultPlaybackRate; }
    void setDefaultPlaybackRate(double, ExceptionState&);

    double playbackRate() const;
    void setPlaybackRate(double, ExceptionState&);

    double volume() const { return m_volume; }
    void setVolume(double, ExceptionState&);

    bool muted() const { return m_muted; }
    void setMuted(bool);

    typedef HTMLMediaElement::ReadyState ReadyState;
    ReadyState readyState() const { return m_readyState; }

    enum PlaybackState { WAITING, PLAYING, ENDED };
    const AtomicString& playbackState() const;

    bool isRestrained() const;
    bool isBlocked() const;

    void clearExecutionContext() { m_executionContext = 0; }

private:
    MediaController(ExecutionContext*);
    void reportControllerState();
    void updateReadyState();
    void updatePlaybackState();
    void updateMediaElements();
    void bringElementUpToSpeed(HTMLMediaElement*);
    void scheduleEvent(const AtomicString& eventName);
    void asyncEventTimerFired(Timer<MediaController>*);
    void clearPositionTimerFired(Timer<MediaController>*);
    bool hasEnded() const;
    void scheduleTimeupdateEvent();
    void timeupdateTimerFired(Timer<MediaController>*);
    void startTimeupdateTimer();

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE { return m_executionContext; }

    friend class HTMLMediaElement;
    friend class MediaControllerEventListener;
    Vector<HTMLMediaElement*> m_mediaElements;
    bool m_paused;
    double m_defaultPlaybackRate;
    double m_volume;
    mutable double m_position;
    bool m_muted;
    ReadyState m_readyState;
    PlaybackState m_playbackState;
    WillBePersistentHeapVector<RefPtrWillBeMember<Event> > m_pendingEvents;
    Timer<MediaController> m_asyncEventTimer;
    mutable Timer<MediaController> m_clearPositionTimer;
    OwnPtr<Clock> m_clock;
    ExecutionContext* m_executionContext;
    Timer<MediaController> m_timeupdateTimer;
    double m_previousTimeupdateTime;
};

} // namespace WebCore

#endif
