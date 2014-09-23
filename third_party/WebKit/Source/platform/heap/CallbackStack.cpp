// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/heap/CallbackStack.h"

#include "platform/heap/Heap.h"

namespace blink {

class CallbackStack::Block {
public:
    explicit Block(Block* next)
        : m_limit(&(m_buffer[blockSize]))
        , m_current(&(m_buffer[0]))
        , m_next(next)
    {
        clearUnused();
    }

    ~Block()
    {
        clearUnused();
    }

    void clear()
    {
        m_current = &m_buffer[0];
        m_next = 0;
        clearUnused();
    }

    Block* next() const { return m_next; }
    void setNext(Block* next) { m_next = next; }

    bool isEmptyBlock() const
    {
        return m_current == &(m_buffer[0]);
    }

    size_t size() const
    {
        return blockSize - (m_limit - m_current);
    }

    Item* allocateEntry()
    {
        if (m_current < m_limit)
            return m_current++;
        return 0;
    }

    Item* pop()
    {
        if (isEmptyBlock())
            return 0;
        return --m_current;
    }

    void invokeEphemeronCallbacks(Visitor* visitor)
    {
        // This loop can tolerate entries being added by the callbacks after
        // iteration starts.
        for (unsigned i = 0; m_buffer + i < m_current; i++) {
            Item& item = m_buffer[i];

            // We don't need to check for orphaned pages when popping an ephemeron
            // callback since the callback is only pushed after the object containing
            // it has been traced. There are basically three cases to consider:
            // 1. Member<EphemeronCollection>
            // 2. EphemeronCollection is part of a containing object
            // 3. EphemeronCollection is a value object in a collection
            //
            // Ad. 1. In this case we push the start of the ephemeron on the
            // marking stack and do the orphaned page check when popping it off
            // the marking stack.
            // Ad. 2. The containing object cannot be on an orphaned page since
            // in that case we wouldn't have traced its parts. This also means
            // the ephemeron collection is not on the orphaned page.
            // Ad. 3. Is the same as 2. The collection containing the ephemeron
            // collection as a value object cannot be on an orphaned page since
            // it would not have traced its values in that case.
            item.call(visitor);
        }
    }

#if ENABLE(ASSERT)
    bool hasCallbackForObject(const void* object)
    {
        for (unsigned i = 0; m_buffer + i < m_current; i++) {
            Item* item = &m_buffer[i];
            if (item->object() == object)
                return true;
        }
        return false;
    }
#endif

private:
    void clearUnused()
    {
#if ENABLE(ASSERT)
        for (size_t i = 0; i < blockSize; i++)
            m_buffer[i] = Item(0, 0);
#endif
    }

    Item m_buffer[blockSize];
    Item* m_limit;
    Item* m_current;
    Block* m_next;
};

CallbackStack::CallbackStack() : m_first(new Block(0)), m_last(m_first)
{
}

CallbackStack::~CallbackStack()
{
    clear();
    delete m_first;
    m_first = 0;
    m_last = 0;
}

void CallbackStack::clear()
{
    Block* next;
    for (Block* current = m_first->next(); current; current = next) {
        next = current->next();
        delete current;
    }
    m_first->clear();
    m_last = m_first;
}

bool CallbackStack::isEmpty() const
{
    return hasJustOneBlock() && m_first->isEmptyBlock();
}

void CallbackStack::takeBlockFrom(CallbackStack* other)
{
    // We assume the stealing stack is empty.
    ASSERT(isEmpty());

    if (other->isEmpty())
        return;

    if (other->hasJustOneBlock()) {
        swap(other);
        return;
    }

    // Delete our block and steal the first one from other.
    delete m_first;
    m_first = other->m_first;
    other->m_first = m_first->next();
    m_first->setNext(0);
    m_last = m_first;
}

CallbackStack::Item* CallbackStack::allocateEntry()
{
    if (Item* item = m_first->allocateEntry())
        return item;
    m_first = new Block(m_first);
    return m_first->allocateEntry();
}

CallbackStack::Item* CallbackStack::pop()
{
    Item* item = m_first->pop();
    while (!item) {
        if (hasJustOneBlock()) {
#if ENABLE(ASSERT)
            m_first->clear();
#endif
            return 0;
        }
        Block* next = m_first->next();
        delete m_first;
        m_first = next;
        item = m_first->pop();
    }
    return item;
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
    Block* from = 0;
    Block* upto = 0;
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

bool CallbackStack::sizeExceeds(size_t minSize) const
{
    Block* current = m_first;
    for (size_t size = m_first->size(); size < minSize; size += current->size()) {
        if (!current->next())
            return false;
        current = current->next();
    }
    return true;
}

void CallbackStack::append(CallbackStack* other)
{
    ASSERT(!m_last->next());
    ASSERT(!other->m_last->next());
    m_last->setNext(other->m_first);
    m_last = other->m_last;
    // After append, we mark |other| as ill-formed by clearing its pointers.
    other->m_first = 0;
    other->m_last = 0;
}

bool CallbackStack::hasJustOneBlock() const
{
    return !m_first->next();
}

void CallbackStack::swap(CallbackStack* other)
{
    Block* tmp = m_first;
    m_first = other->m_first;
    other->m_first = tmp;
    tmp = m_last;
    m_last = other->m_last;
    other->m_last = tmp;
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

}
