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

#ifndef EventPathWalker_h
#define EventPathWalker_h

#include "wtf/Vector.h"

namespace WebCore {

class Node;

class EventPathWalker {
public:
    explicit EventPathWalker(const Node*);
    static Node* parent(const Node*);
    void moveToParent() { ++m_index; };
    Node* node() const;
    Node* adjustedTarget();

private:
    void calculateAdjustedTargets();
    size_t m_index;
    Vector<const Node*> m_path;
    Vector<const Node*> m_adjustedTargets;
};

inline Node* EventPathWalker::node() const
{
    ASSERT(m_index <= m_path.size());
    return m_index == m_path.size() ? 0 : const_cast<Node*>(m_path[m_index]);
}

inline Node* EventPathWalker::adjustedTarget()
{
    if (m_adjustedTargets.isEmpty())
        calculateAdjustedTargets();
    ASSERT(m_index <= m_adjustedTargets.size());
    return m_index == m_adjustedTargets.size() ? 0 : const_cast<Node*>(m_adjustedTargets[m_index]);
}

inline Node* EventPathWalker::parent(const Node* node)
{
    EventPathWalker walker(node);
    walker.moveToParent();
    return walker.node();
}

} // namespace

#endif
