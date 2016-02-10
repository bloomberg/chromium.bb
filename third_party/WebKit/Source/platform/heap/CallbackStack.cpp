// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/CallbackStack.h"
#include "wtf/PageAllocator.h"

namespace blink {

CallbackStack::Block::Block(Block* next)
{
    static_assert((blockSize * sizeof(Item)) % WTF::kPageAllocationGranularity == 0, "CallbackStack::blockSize * sizeof(Item) must be a multiple of WTF::kPageAllocationGranularity");
    m_buffer = static_cast<Item*>(WTF::allocPages(nullptr, blockSize * sizeof(Item), WTF::kPageAllocationGranularity, WTF::PageAccessible));
    RELEASE_ASSERT(m_buffer);

#if ENABLE(ASSERT)
    for (size_t i = 0; i < blockSize; i++)
        m_buffer[i] = Item(0, 0);
#endif

    m_limit = &(m_buffer[blockSize]);
    m_current = &(m_buffer[0]);
    m_next = next;
}

CallbackStack::Block::~Block()
{
    WTF::freePages(m_buffer, blockSize * sizeof(Item));
    m_buffer = nullptr;
    m_limit = nullptr;
    m_current = nullptr;
    m_next = nullptr;
}

void CallbackStack::Block::decommit()
{
#if ENABLE(ASSERT)
    for (size_t i = 0; i < blockSize; i++)
        m_buffer[i] = Item(0, 0);
#endif

    WTF::discardSystemPages(m_buffer, blockSize * sizeof(Item));

    m_current = &m_buffer[0];
    m_next = nullptr;
}

void CallbackStack::Block::invokeEphemeronCallbacks(Visitor* visitor)
{
    // This loop can tolerate entries being added by the callbacks after
    // iteration starts.
    for (unsigned i = 0; m_buffer + i < m_current; i++) {
        Item& item = m_buffer[i];
        item.call(visitor);
    }
}

#if ENABLE(ASSERT)
bool CallbackStack::Block::hasCallbackForObject(const void* object)
{
    for (unsigned i = 0; m_buffer + i < m_current; i++) {
        Item* item = &m_buffer[i];
        if (item->object() == object)
            return true;
    }
    return false;
}
#endif

CallbackStack::CallbackStack()
    : m_first(new Block(0))
    , m_last(m_first)
{
}

CallbackStack::~CallbackStack()
{
    RELEASE_ASSERT(isEmpty());
    delete m_first;
    m_first = nullptr;
    m_last = nullptr;
}

void CallbackStack::decommit()
{
    Block* next;
    for (Block* current = m_first->next(); current; current = next) {
        next = current->next();
        delete current;
    }
    m_first->decommit();
    m_last = m_first;
}

bool CallbackStack::isEmpty() const
{
    return hasJustOneBlock() && m_first->isEmptyBlock();
}

CallbackStack::Item* CallbackStack::allocateEntrySlow()
{
    ASSERT(!m_first->allocateEntry());
    m_first = new Block(m_first);
    return m_first->allocateEntry();
}

CallbackStack::Item* CallbackStack::popSlow()
{
    ASSERT(m_first->isEmptyBlock());

    for (;;) {
        Block* next = m_first->next();
        if (!next) {
#if ENABLE(ASSERT)
            m_first->decommit();
#endif
            return nullptr;
        }
        delete m_first;
        m_first = next;
        if (Item* item = m_first->pop())
            return item;
    }
}

void CallbackStack::invokeEphemeronCallbacks(Visitor* visitor)
{
    // The first block is the only one where new ephemerons are added, so we
    // call the callbacks on that last, to catch any new ephemerons discovered
    // in the callbacks.
    // However, if enough ephemerons were added, we may have a new block that
    // has been prepended to the chain. This will be very rare, but we can
    // handle the situation by starting again and calling all the callbacks
    // on the prepended blocks.
    Block* from = nullptr;
    Block* upto = nullptr;
    while (from != m_first) {
        upto = from;
        from = m_first;
        invokeOldestCallbacks(from, upto, visitor);
    }
}

void CallbackStack::invokeOldestCallbacks(Block* from, Block* upto, Visitor* visitor)
{
    if (from == upto)
        return;
    ASSERT(from);
    // Recurse first (blockSize at a time) so we get to the newly added entries last.
    invokeOldestCallbacks(from->next(), upto, visitor);
    from->invokeEphemeronCallbacks(visitor);
}

bool CallbackStack::hasJustOneBlock() const
{
    return !m_first->next();
}

#if ENABLE(ASSERT)
bool CallbackStack::hasCallbackForObject(const void* object)
{
    for (Block* current = m_first; current; current = current->next()) {
        if (current->hasCallbackForObject(object))
            return true;
    }
    return false;
}
#endif

} // namespace blink
