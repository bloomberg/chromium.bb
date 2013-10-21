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

#include "core/animation/ActiveAnimations.h"
#include "core/animation/Player.h"
#include "core/dom/Element.h"
#include "core/events/Event.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class Document;
class TimedItem;

// DocumentTimeline is constructed and owned by Document, and tied to its lifecycle.
class DocumentTimeline : public RefCounted<DocumentTimeline> {

public:
    static PassRefPtr<DocumentTimeline> create(Document*);
    void serviceAnimations(double);
    PassRefPtr<Player> play(TimedItem*);
    // Called from setReadyState() in Document.cpp to set m_zeroTimeAsPerfTime to
    // performance.timing.domInteractive.
    void setZeroTimeAsPerfTime(double);
    double currentTime() { return m_currentTime; }
    void pauseAnimationsForTesting(double);
    size_t numberOfActiveAnimationsForTesting() const;
    AnimationStack* animationStack(const Element* element) const
    {
        if (ActiveAnimations* animations = element->activeAnimations())
            return animations->defaultStack();
        return 0;
    }
    void addEventToDispatch(EventTarget* target, PassRefPtr<Event> event)
    {
        m_events.append(EventToDispatch(target, event));
    }

private:
    DocumentTimeline(Document*);
    void dispatchEvents();
    double m_currentTime;
    double m_zeroTimeAsPerfTime;
    Document* m_document;
    Vector<RefPtr<Player> > m_players;

    struct EventToDispatch {
        EventToDispatch(EventTarget* target, PassRefPtr<Event> event)
            : target(target)
            , event(event)
        {
        }
        RefPtr<EventTarget> target;
        RefPtr<Event> event;
    };
    Vector<EventToDispatch> m_events;
};

} // namespace

#endif
