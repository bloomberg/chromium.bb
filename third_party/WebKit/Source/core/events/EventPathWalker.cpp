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

#include "config.h"
#include "core/events/EventPathWalker.h"

#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"

namespace WebCore {

EventPathWalker::EventPathWalker(const Node* node)
    : m_node(node)
    , m_distributedNode(node)
    , m_isVisitingInsertionPointInReprojection(false)
{
    ASSERT(node);
    node->document().updateDistributionForNodeIfNeeded(const_cast<Node*>(node));
}

Node* EventPathWalker::parent(const Node* node)
{
    EventPathWalker walker(node);
    walker.moveToParent();
    return walker.node();
}

void EventPathWalker::moveToParent()
{
    ASSERT(m_node);
    ASSERT(m_distributedNode);
    if (ElementShadow* shadow = shadowOfParent(m_node)) {
        if (InsertionPoint* insertionPoint = shadow->findInsertionPointFor(m_distributedNode)) {
            m_node = insertionPoint;
            m_isVisitingInsertionPointInReprojection = true;
            return;
        }
    }
    if (!m_node->isShadowRoot()) {
        m_node = m_node->parentNode();
        if (!(m_node && m_node->isShadowRoot() && toShadowRoot(m_node)->insertionPoint()))
            m_distributedNode = m_node;
        m_isVisitingInsertionPointInReprojection = false;
        return;
    }

    const ShadowRoot* shadowRoot = toShadowRoot(m_node);
    if (InsertionPoint* insertionPoint = shadowRoot->insertionPoint()) {
        m_node = insertionPoint;
        m_isVisitingInsertionPointInReprojection = true;
        return;
    }
    m_node = shadowRoot->host();
    m_distributedNode = m_node;
    m_isVisitingInsertionPointInReprojection = false;
}

} // namespace
