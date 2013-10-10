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
    : m_index(0)
{
    ASSERT(node);
    node->document().updateDistributionForNodeIfNeeded(const_cast<Node*>(node));
    const Node* current = node;
    const Node* distributedNode = node;
    while (current) {
        m_path.append(current);
        if (ElementShadow* shadow = shadowOfParent(current)) {
            if (InsertionPoint* insertionPoint = shadow->findInsertionPointFor(distributedNode)) {
                current = insertionPoint;
                continue;
            }
        }
        if (!current->isShadowRoot()) {
            current = current->parentNode();
            if (!(current && current->isShadowRoot() && toShadowRoot(current)->insertionPoint()))
                distributedNode = current;
            continue;
        }

        const ShadowRoot* shadowRoot = toShadowRoot(current);
        if (InsertionPoint* insertionPoint = shadowRoot->insertionPoint()) {
            current = insertionPoint;
            continue;
        }
        current = shadowRoot->host();
        distributedNode = current;
    }
}

static inline bool movedFromParentToChild(const TreeScope* lastTreeScope, const TreeScope* currentTreeScope)
{
    return currentTreeScope->parentTreeScope() == lastTreeScope;
}

static inline bool movedFromChildToParent(const TreeScope* lastTreeScope, const TreeScope* currentTreeScope)
{
    return lastTreeScope->parentTreeScope() == currentTreeScope;
}

static inline bool movedFromOlderToYounger(const TreeScope* lastTreeScope, const TreeScope* currentTreeScope)
{
    Node* rootNode = lastTreeScope->rootNode();
    return rootNode->isShadowRoot() && toShadowRoot(rootNode)->youngerShadowRoot() == currentTreeScope->rootNode();
}

void EventPathWalker::calculateAdjustedTargets()
{
    ASSERT(m_adjustedTargets.isEmpty());
    Vector<const Node*, 32> targetStack;
    const TreeScope* lastTreeScope = 0;
    bool isSVGElement = m_path[0]->isSVGElement();
    for (size_t i = 0; i < m_path.size(); ++i) {
        const Node* current = m_path[i];
        const TreeScope* currentTreeScope = &current->treeScope();
        if (targetStack.isEmpty()) {
            targetStack.append(current);
        } else if (lastTreeScope != currentTreeScope && !isSVGElement) {
            if (movedFromParentToChild(lastTreeScope, currentTreeScope)) {
                targetStack.append(targetStack.last());
            } else if (movedFromChildToParent(lastTreeScope, currentTreeScope)) {
                ASSERT(!targetStack.isEmpty());
                targetStack.removeLast();
                if (targetStack.isEmpty())
                    targetStack.append(current);
            } else if (movedFromOlderToYounger(lastTreeScope, currentTreeScope)) {
                ASSERT(!targetStack.isEmpty());
                targetStack.removeLast();
                if (targetStack.isEmpty())
                    targetStack.append(current);
                else
                    targetStack.append(targetStack.last());
            } else {
                ASSERT_NOT_REACHED();
            }
        }
        m_adjustedTargets.append(targetStack.last());
        lastTreeScope = currentTreeScope;
    }
}

} // namespace
