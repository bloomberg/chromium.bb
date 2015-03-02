/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov.
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

#include "config.h"
#include "core/editing/iterators/BitStack.h"

#include "core/dom/ContainerNode.h"
#include "core/dom/Node.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutObject.h"

namespace blink {
static const unsigned bitsInWord = sizeof(unsigned) * 8;
static const unsigned bitInWordMask = bitsInWord - 1;

#if ENABLE(ASSERT)
static unsigned depthCrossingShadowBoundaries(Node* node)
{
    unsigned depth = 0;
    for (ContainerNode* parent = node->parentOrShadowHostNode(); parent; parent = parent->parentOrShadowHostNode())
        ++depth;
    return depth;
}
#endif

static inline bool fullyClipsContents(Node* node)
{
    LayoutObject* renderer = node->renderer();
    if (!renderer || !renderer->isBox() || !renderer->hasOverflowClip())
        return false;
    return toLayoutBox(renderer)->size().isEmpty();
}

static inline bool ignoresContainerClip(Node* node)
{
    LayoutObject* renderer = node->renderer();
    if (!renderer || renderer->isText())
        return false;
    return renderer->style()->hasOutOfFlowPosition();
}

BitStack::BitStack()
    : m_size(0)
{
}

BitStack::~BitStack()
{
}

void BitStack::push(bool bit)
{
    unsigned index = m_size / bitsInWord;
    unsigned shift = m_size & bitInWordMask;
    if (!shift && index == m_words.size()) {
        m_words.grow(index + 1);
        m_words[index] = 0;
    }
    unsigned& word = m_words[index];
    unsigned mask = 1U << shift;
    if (bit)
        word |= mask;
    else
        word &= ~mask;
    ++m_size;
}

void BitStack::pop()
{
    if (m_size)
        --m_size;
}

bool BitStack::top() const
{
    if (!m_size)
        return false;
    unsigned shift = (m_size - 1) & bitInWordMask;
    unsigned index = (m_size - 1) / bitsInWord;
    return m_words[index] & (1U << shift);
}

unsigned BitStack::size() const
{
    return m_size;
}


void BitStack::pushFullyClippedState(Node* node)
{
    ASSERT(size() == depthCrossingShadowBoundaries(node));

    // FIXME: m_fullyClippedStack was added in response to <https://bugs.webkit.org/show_bug.cgi?id=26364>
    // ("Search can find text that's hidden by overflow:hidden"), but the logic here will not work correctly if
    // a shadow tree redistributes nodes. m_fullyClippedStack relies on the assumption that DOM node hierarchy matches
    // the render tree, which is not necessarily true if there happens to be shadow DOM distribution or other mechanics
    // that shuffle around the render objects regardless of node tree hierarchy (like CSS flexbox).
    //
    // A more appropriate way to handle this situation is to detect overflow:hidden blocks by using only rendering
    // primitives, not with DOM primitives.

    // Push true if this node full clips its contents, or if a parent already has fully
    // clipped and this is not a node that ignores its container's clip.
    push(fullyClipsContents(node) || (top() && !ignoresContainerClip(node)));
}

void BitStack::setUpFullyClippedStack(Node* node)
{
    // Put the nodes in a vector so we can iterate in reverse order.
    WillBeHeapVector<RawPtrWillBeMember<ContainerNode>, 100> ancestry;
    for (ContainerNode* parent = node->parentOrShadowHostNode(); parent; parent = parent->parentOrShadowHostNode())
        ancestry.append(parent);

    // Call pushFullyClippedState on each node starting with the earliest ancestor.
    size_t ancestrySize = ancestry.size();
    for (size_t i = 0; i < ancestrySize; ++i)
        pushFullyClippedState(ancestry[ancestrySize - i - 1]);
    pushFullyClippedState(node);

    ASSERT(size() == 1 + depthCrossingShadowBoundaries(node));
}

} // namespace blink
