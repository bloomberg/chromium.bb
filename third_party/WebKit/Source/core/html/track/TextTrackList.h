/*
 * Copyright (C) 2011, 2012 Apple Inc.  All rights reserved.
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

#ifndef TextTrackList_h
#define TextTrackList_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "platform/Timer.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class GenericEventQueue;
class HTMLMediaElement;
class TextTrack;
class TextTrackList;

class TextTrackList FINAL : public RefCounted<TextTrackList>, public ScriptWrappable, public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(TextTrackList);
public:
    static PassRefPtr<TextTrackList> create(HTMLMediaElement* owner)
    {
        return adoptRef(new TextTrackList(owner));
    }
    virtual ~TextTrackList();

    unsigned length() const;
    int getTrackIndex(TextTrack*);
    int getTrackIndexRelativeToRenderedTracks(TextTrack*);
    bool contains(TextTrack*) const;

    TextTrack* item(unsigned index);
    TextTrack* getTrackById(const AtomicString& id);
    void append(PassRefPtr<TextTrack>);
    void remove(TextTrack*);

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(addtrack);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(removetrack);

    void clearOwner() { m_owner = 0; }
    Node* owner() const;

    void scheduleChangeEvent();

private:
    explicit TextTrackList(HTMLMediaElement*);

    void scheduleTrackEvent(const AtomicString& eventName, PassRefPtr<TextTrack>);

    void scheduleAddTrackEvent(PassRefPtr<TextTrack>);
    void scheduleRemoveTrackEvent(PassRefPtr<TextTrack>);

    void invalidateTrackIndexesAfterTrack(TextTrack*);

    HTMLMediaElement* m_owner;

    OwnPtr<GenericEventQueue> m_asyncEventQueue;

    Vector<RefPtr<TextTrack> > m_addTrackTracks;
    Vector<RefPtr<TextTrack> > m_elementTracks;
    Vector<RefPtr<TextTrack> > m_inbandTracks;
};

} // namespace WebCore

#endif
