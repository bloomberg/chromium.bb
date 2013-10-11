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
#include "core/html/MediaControllerInterface.h"
#include "platform/Timer.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class Clock;
class Event;
class ExceptionState;
class HTMLMediaElement;
class ExecutionContext;

class MediaController : public RefCounted<MediaController>, public ScriptWrappable, public MediaControllerInterface, public EventTargetWithInlineData {
public:
    static PassRefPtr<MediaController> create(ExecutionContext*);
    virtual ~MediaController();

    void addMediaElement(HTMLMediaElement*);
    void removeMediaElement(HTMLMediaElement*);
    bool containsMediaElement(HTMLMediaElement*) const;

    const String& mediaGroup() const { return m_mediaGroup; }

    virtual PassRefPtr<TimeRanges> buffered() const;
    virtual PassRefPtr<TimeRanges> seekable() const;
    virtual PassRefPtr<TimeRanges> played();

    virtual double duration() const;
    virtual double currentTime() const;
    virtual void setCurrentTime(double, ExceptionState&);

    virtual bool paused() const { return m_paused; }
    virtual void play();
    virtual void pause();
    void unpause();

    virtual double defaultPlaybackRate() const { return m_defaultPlaybackRate; }
    virtual void setDefaultPlaybackRate(double);

    virtual double playbackRate() const;
    virtual void setPlaybackRate(double);

    virtual double volume() const { return m_volume; }
    virtual void setVolume(double, ExceptionState&);

    virtual bool muted() const { return m_muted; }
    virtual void setMuted(bool);

    virtual ReadyState readyState() const { return m_readyState; }

    enum PlaybackState { WAITING, PLAYING, ENDED };
    const AtomicString& playbackState() const;

    virtual bool supportsFullscreen() const { return false; }
    virtual bool isFullscreen() const { return false; }
    virtual void enterFullscreen() { }

    virtual bool hasAudio() const;
    virtual bool hasVideo() const;
    virtual bool hasClosedCaptions() const;
    virtual void setClosedCaptionsVisible(bool);
    virtual bool closedCaptionsVisible() const { return m_closedCaptionsVisible; }

    virtual void beginScrubbing();
    virtual void endScrubbing();

    virtual bool canPlay() const;

    virtual bool hasCurrentSrc() const;

    bool isBlocked() const;

    void clearExecutionContext() { m_executionContext = 0; }

    // EventTarget
    using RefCounted<MediaController>::ref;
    using RefCounted<MediaController>::deref;

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
    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }
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
    Vector<RefPtr<Event> > m_pendingEvents;
    Timer<MediaController> m_asyncEventTimer;
    mutable Timer<MediaController> m_clearPositionTimer;
    String m_mediaGroup;
    bool m_closedCaptionsVisible;
    OwnPtr<Clock> m_clock;
    ExecutionContext* m_executionContext;
    Timer<MediaController> m_timeupdateTimer;
    double m_previousTimeupdateTime;
};

} // namespace WebCore

#endif
