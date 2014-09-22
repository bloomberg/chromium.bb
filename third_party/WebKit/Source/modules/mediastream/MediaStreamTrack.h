/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaStreamTrack_h
#define MediaStreamTrack_h

#include "core/dom/ActiveDOMObject.h"
#include "modules/EventTargetModules.h"
#include "modules/mediastream/SourceInfo.h"
#include "platform/mediastream/MediaStreamDescriptor.h"
#include "platform/mediastream/MediaStreamSource.h"
#include "wtf/Forward.h"

namespace blink {

class AudioSourceProvider;
class ExceptionState;
class MediaStreamComponent;
class MediaStreamTrackSourcesCallback;

class MediaStreamTrack FINAL
    : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<MediaStreamTrack>
    , public ActiveDOMObject
    , public EventTargetWithInlineData
    , public MediaStreamSource::Observer {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<MediaStreamTrack>);
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(MediaStreamTrack);
public:
    static MediaStreamTrack* create(ExecutionContext*, MediaStreamComponent*);
    virtual ~MediaStreamTrack();

    String kind() const;
    String id() const;
    String label() const;

    bool enabled() const;
    void setEnabled(bool);

    bool muted() const;

    String readyState() const;

    static void getSources(ExecutionContext*, PassOwnPtrWillBeRawPtr<MediaStreamTrackSourcesCallback>, ExceptionState&);
    void stopTrack(ExceptionState&);
    MediaStreamTrack* clone(ExecutionContext*);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(mute);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(unmute);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(ended);

    MediaStreamComponent* component();
    bool ended() const;

    void registerMediaStream(MediaStream*);
    void unregisterMediaStream(MediaStream*);

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    // ActiveDOMObject
    virtual void stop() OVERRIDE;

    PassOwnPtr<AudioSourceProvider> createWebAudioSource();

    virtual void trace(Visitor*) OVERRIDE;

private:
    MediaStreamTrack(ExecutionContext*, MediaStreamComponent*);

    // MediaStreamSourceObserver
    virtual void sourceChangedState() OVERRIDE;

    void propagateTrackEnded();

    MediaStreamSource::ReadyState m_readyState;
    HeapHashSet<Member<MediaStream> > m_registeredMediaStreams;
    bool m_isIteratingRegisteredMediaStreams;
    bool m_stopped;
    Member<MediaStreamComponent> m_component;
};

typedef HeapVector<Member<MediaStreamTrack> > MediaStreamTrackVector;

} // namespace blink

#endif // MediaStreamTrack_h
