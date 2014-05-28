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

#ifndef MediaSourceBase_h
#define MediaSourceBase_h

#include "core/dom/ActiveDOMObject.h"
#include "core/html/HTMLMediaSource.h"
#include "core/html/URLRegistry.h"
#include "modules/EventTargetModules.h"
#include "public/platform/WebMediaSource.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace blink {
class WebSourceBuffer;
}

namespace WebCore {

class ExceptionState;
class GenericEventQueue;

class MediaSourceBase : public RefCountedWillBeRefCountedGarbageCollected<MediaSourceBase>, public HTMLMediaSource, public ActiveDOMObject, public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(MediaSourceBase);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(MediaSourceBase);
public:
    static const AtomicString& openKeyword();
    static const AtomicString& closedKeyword();
    static const AtomicString& endedKeyword();

    virtual ~MediaSourceBase();

    void addedToRegistry();
    void removedFromRegistry();
    void openIfInEndedState();
    bool isOpen() const;

    // HTMLMediaSource
    virtual bool attachToElement(HTMLMediaElement*) OVERRIDE FINAL;
    virtual void setWebMediaSourceAndOpen(PassOwnPtr<blink::WebMediaSource>) OVERRIDE FINAL;
    virtual void close() OVERRIDE FINAL;
    virtual bool isClosed() const OVERRIDE FINAL;
    virtual double duration() const OVERRIDE FINAL;
    virtual PassRefPtr<TimeRanges> buffered() const OVERRIDE FINAL;
    virtual void refHTMLMediaSource() OVERRIDE FINAL { ref(); }
    virtual void derefHTMLMediaSource() OVERRIDE FINAL { deref(); }

    void setDuration(double, ExceptionState&);
    const AtomicString& readyState() const { return m_readyState; }
    void setReadyState(const AtomicString&);
    void endOfStream(const AtomicString& error, ExceptionState&);
    void endOfStream(ExceptionState&);

    // ActiveDOMObject interface
    virtual bool hasPendingActivity() const OVERRIDE FINAL;
    virtual void stop() OVERRIDE FINAL;

    // EventTarget interface
    virtual ExecutionContext* executionContext() const OVERRIDE FINAL;

    // URLRegistrable interface
    virtual URLRegistry& registry() const OVERRIDE FINAL;

protected:
    explicit MediaSourceBase(ExecutionContext*);

    virtual void onReadyStateChange(const AtomicString& oldState, const AtomicString& newState) = 0;
    virtual Vector<RefPtr<TimeRanges> > activeRanges() const = 0;
    virtual bool isUpdating() const = 0;

    PassOwnPtr<blink::WebSourceBuffer> createWebSourceBuffer(const String& type, const Vector<String>& codecs, ExceptionState&);
    void scheduleEvent(const AtomicString& eventName);
    GenericEventQueue* asyncEventQueue() const { return m_asyncEventQueue.get(); }

private:
    void endOfStreamInternal(const blink::WebMediaSource::EndOfStreamStatus, ExceptionState&);
    OwnPtr<blink::WebMediaSource> m_webMediaSource;
    AtomicString m_readyState;
    OwnPtr<GenericEventQueue> m_asyncEventQueue;
    // FIXME: oilpan: This should become a Member. For now, m_attachedElement will be cleared by the HTMLMediaElement destructor.
    HTMLMediaElement* m_attachedElement;
};

}

#endif
