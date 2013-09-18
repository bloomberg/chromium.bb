/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef FontFaceSet_h
#define FontFaceSet_h

#include "core/css/FontFace.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/EventListener.h"
#include "core/dom/EventNames.h"
#include "core/dom/EventTarget.h"
#include "core/html/VoidCallback.h"
#include "core/platform/Timer.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class FontResource;
class CSSFontFaceSource;
class Dictionary;
class Document;
class Event;
class Font;
class ScriptExecutionContext;

class FontFaceSet : public RefCounted<FontFaceSet>, public ActiveDOMObject, public EventTarget {
public:
    static PassRefPtr<FontFaceSet> create(Document* document)
    {
        return adoptRef<FontFaceSet>(new FontFaceSet(document));
    }
    virtual ~FontFaceSet();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(loading);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadingdone);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadingerror);

    bool checkFont(const String&, const String&);
    void loadFont(const Dictionary&);
    void notifyWhenFontsReady(PassRefPtr<VoidCallback>);

    bool loading() const { return m_loadingCount > 0 || m_shouldFireDoneEvent; }

    virtual ScriptExecutionContext* scriptExecutionContext() const;
    virtual const AtomicString& interfaceName() const;

    using RefCounted<FontFaceSet>::ref;
    using RefCounted<FontFaceSet>::deref;

    Document* document() const;

    void didLayout();
    void beginFontLoading(FontFace*);
    void fontLoaded(FontFace*);
    void loadError(FontFace*);
    void scheduleCallback(PassRefPtr<VoidCallback>);

private:
    class FontLoadHistogram {
    public:
        FontLoadHistogram() : m_count(0), m_recorded(false) { }
        void incrementCount() { m_count++; }
        void record();

    private:
        int m_count;
        bool m_recorded;
    };

    FontFaceSet(Document*);

    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }
    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();

    void scheduleEvent(PassRefPtr<Event>);
    void queueDoneEvent(FontFace*);
    void firePendingEvents();
    void firePendingCallbacks();
    void fireDoneEventIfPossible();
    bool resolveFontStyle(const String&, Font&);
    void timerFired(Timer<FontFaceSet>*);

    EventTargetData m_eventTargetData;
    unsigned m_loadingCount;
    Vector<RefPtr<Event> > m_pendingEvents;
    Vector<RefPtr<VoidCallback> > m_pendingCallbacks;
    Vector<RefPtr<VoidCallback> > m_fontsReadyCallbacks;
    FontFaceArray m_loadedFonts;
    FontFaceArray m_failedFonts;
    bool m_shouldFireDoneEvent;
    Timer<FontFaceSet> m_timer;
    FontLoadHistogram m_histogram;
};

} // namespace WebCore

#endif // FontFaceSet_h
