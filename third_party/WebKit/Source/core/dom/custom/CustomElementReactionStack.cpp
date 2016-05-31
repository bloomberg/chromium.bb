// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementReactionStack.h"

#include "core/dom/Element.h"
#include "core/dom/custom/CustomElementReactionQueue.h"

namespace blink {

// TODO(dominicc): Consider using linked heap structures, avoiding
// finalizers, to make short-lived entries fast.

CustomElementReactionStack::CustomElementReactionStack()
{
}

CustomElementReactionStack::~CustomElementReactionStack()
{
}

DEFINE_TRACE(CustomElementReactionStack)
{
    visitor->trace(m_map);
    visitor->trace(m_stack);
}

void CustomElementReactionStack::push()
{
    m_stack.append(nullptr);
}

void CustomElementReactionStack::popInvokingReactions()
{
    ElementQueue* queue = m_stack.last();
    m_stack.removeLast();
    if (!queue)
        return;
    for (auto& element : *queue) {
        if (CustomElementReactionQueue* reactions = m_map.get(element)) {
            reactions->invokeReactions(element);
            CHECK(reactions->isEmpty());
            m_map.remove(element);
        }
    }
}

void CustomElementReactionStack::enqueue(
    Element* element,
    CustomElementReaction* reaction)
{
    ElementQueue* queue = m_stack.last();
    if (!queue)
        m_stack.last() = queue = new ElementQueue();
    queue->append(element);

    CustomElementReactionQueue* reactions = m_map.get(element);
    if (!reactions) {
        reactions = new CustomElementReactionQueue();
        m_map.add(element, reactions);
    }

    reactions->add(reaction);
}

} // namespace blink
