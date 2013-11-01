/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#ifndef EventPath_h
#define EventPath_h

#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class Event;
class EventContext;
class EventTarget;
class Node;

enum EventDispatchBehavior {
    RetargetEvent,
    StayInsideShadowDOM
};

class EventPath {
public:
    explicit EventPath(Event*);
    explicit EventPath(Node*);
    void resetWith(Node*);

    EventContext& operator[](size_t index) { return *m_eventContexts[index]; }
    const EventContext& operator[](size_t index) const { return *m_eventContexts[index]; }
    EventContext& last() const { return *m_eventContexts[size() - 1]; }

    bool isEmpty() const { return m_eventContexts.isEmpty(); }
    size_t size() const { return m_eventContexts.size(); }

    void shrink(size_t newSize) { m_eventContexts.shrink(newSize); }

    static Node* parent(Node*);
    static EventTarget* eventTargetRespectingTargetRules(Node*);

private:
    EventPath();

    EventContext& at(size_t index) { return *m_eventContexts[index]; }

    void addEventContext(Node*, bool isMouseOrFocusEvent, bool isTouchEvent);

    void calculatePath();
    void calculateAdjustedTargets();
    void calculateAdjustedEventPathForEachNode();

    Vector<OwnPtr<EventContext>, 32> m_eventContexts;
    Node* m_node;
    Event* m_event;
};

} // namespace

#endif
