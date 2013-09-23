/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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
 *
 */

#ifndef EventContext_h
#define EventContext_h

#include "core/events/EventTarget.h"
#include "core/dom/Node.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/TreeScope.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class Event;
class TouchList;

class EventContext {
public:
    // FIXME: Use ContainerNode instead of Node.
    EventContext(PassRefPtr<Node>, PassRefPtr<EventTarget> currentTarget, PassRefPtr<EventTarget> target);
    virtual ~EventContext();

    Node* node() const { return m_node.get(); }
    EventTarget* target() const { return m_target.get(); }
    PassRefPtr<NodeList> eventPath() { return m_eventPath; }
    void adoptEventPath(Vector<RefPtr<Node> >&);
    void setEventPath(PassRefPtr<NodeList> nodeList) { m_eventPath = nodeList; }

    bool currentTargetSameAsTarget() const { return m_currentTarget.get() == m_target.get(); }
    virtual void handleLocalEvents(Event*) const;
    virtual bool isMouseOrFocusEventContext() const;
    virtual bool isTouchEventContext() const;

protected:
#ifndef NDEBUG
    bool isUnreachableNode(EventTarget*);
#endif
    RefPtr<Node> m_node;
    RefPtr<EventTarget> m_currentTarget;
    RefPtr<EventTarget> m_target;
    RefPtr<NodeList> m_eventPath;
};

typedef Vector<OwnPtr<EventContext>, 32> EventPath;

class MouseOrFocusEventContext : public EventContext {
public:
    MouseOrFocusEventContext(PassRefPtr<Node>, PassRefPtr<EventTarget> currentTarget, PassRefPtr<EventTarget> target);
    virtual ~MouseOrFocusEventContext();
    EventTarget* relatedTarget() const { return m_relatedTarget.get(); }
    void setRelatedTarget(PassRefPtr<EventTarget>);
    virtual void handleLocalEvents(Event*) const OVERRIDE;
    virtual bool isMouseOrFocusEventContext() const OVERRIDE;

private:
    RefPtr<EventTarget> m_relatedTarget;
};


class TouchEventContext : public EventContext {
public:
    TouchEventContext(PassRefPtr<Node>, PassRefPtr<EventTarget> currentTarget, PassRefPtr<EventTarget> target);
    virtual ~TouchEventContext();

    virtual void handleLocalEvents(Event*) const OVERRIDE;
    virtual bool isTouchEventContext() const OVERRIDE;

    TouchList* touches() { return m_touches.get(); }
    TouchList* targetTouches() { return m_targetTouches.get(); }
    TouchList* changedTouches() { return m_changedTouches.get(); }

private:
    RefPtr<TouchList> m_touches;
    RefPtr<TouchList> m_targetTouches;
    RefPtr<TouchList> m_changedTouches;
#ifndef NDEBUG
    void checkReachability(TouchList*) const;
#endif
};

inline TouchEventContext* toTouchEventContext(EventContext* eventContext)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!eventContext || eventContext->isTouchEventContext());
    return static_cast<TouchEventContext*>(eventContext);
}

#ifndef NDEBUG
inline bool EventContext::isUnreachableNode(EventTarget* target)
{
    // FIXME: Checks also for SVG elements.
    return target && target->toNode() && !target->toNode()->isSVGElement() && !target->toNode()->treeScope().isInclusiveAncestorOf(m_node->treeScope());
}
#endif

inline void MouseOrFocusEventContext::setRelatedTarget(PassRefPtr<EventTarget> relatedTarget)
{
    ASSERT(!isUnreachableNode(relatedTarget.get()));
    m_relatedTarget = relatedTarget;
}

}

#endif // EventContext_h
