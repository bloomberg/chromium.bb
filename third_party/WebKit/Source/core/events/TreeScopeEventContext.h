/*
 * Copyright (C) 2014 Google Inc. All Rights Reserved.
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

#ifndef TreeScopeEventContext_h
#define TreeScopeEventContext_h

#include "core/dom/Node.h"
#include "core/dom/NodeList.h"
#include "core/dom/TreeScope.h"
#include "core/events/EventTarget.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class EventTarget;
class Node;
class TouchEventContext;
class TreeScope;

class TreeScopeEventContext : public RefCounted<TreeScopeEventContext> {
public:
    static PassRefPtr<TreeScopeEventContext> create(TreeScope&);
    ~TreeScopeEventContext();

    TreeScope& treeScope() const { return m_treeScope; }

    EventTarget* target() const { return m_target.get(); }
    void setTarget(PassRefPtr<EventTarget>);

    EventTarget* relatedTarget() const { return m_relatedTarget.get(); }
    void setRelatedTarget(PassRefPtr<EventTarget>);

    TouchEventContext* touchEventContext() const { return m_touchEventContext.get(); }
    TouchEventContext* ensureTouchEventContext();

    PassRefPtr<NodeList> eventPath() const { return m_eventPath; }
    void adoptEventPath(Vector<RefPtr<Node> >&);

private:
    TreeScopeEventContext(TreeScope&);

#ifndef NDEBUG
    bool isUnreachableNode(EventTarget&);
#endif

    TreeScope& m_treeScope;
    RefPtr<EventTarget> m_target;
    RefPtr<EventTarget> m_relatedTarget;
    RefPtr<NodeList> m_eventPath;
    RefPtrWillBePersistent<TouchEventContext> m_touchEventContext;
};

#ifndef NDEBUG
inline bool TreeScopeEventContext::isUnreachableNode(EventTarget& target)
{
    // FIXME: Checks also for SVG elements.
    return target.toNode() && !target.toNode()->isSVGElement() && !target.toNode()->treeScope().isInclusiveOlderSiblingShadowRootOrAncestorTreeScopeOf(m_treeScope);
}
#endif

inline void TreeScopeEventContext::setTarget(PassRefPtr<EventTarget> target)
{
    ASSERT(target);
    ASSERT(!isUnreachableNode(*target));
    m_target = target;
}

inline void TreeScopeEventContext::setRelatedTarget(PassRefPtr<EventTarget> relatedTarget)
{
    ASSERT(relatedTarget);
    ASSERT(!isUnreachableNode(*relatedTarget));
    m_relatedTarget = relatedTarget;
}

}

#endif // TreeScopeEventContext_h
