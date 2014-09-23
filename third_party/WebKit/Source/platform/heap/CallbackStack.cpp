// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/heap/CallbackStack.h"

#include "platform/heap/Heap.h"

namespace blink {

void CallbackStack::init(CallbackStack** first)
{
    // The stacks are chained, so we start by setting this to null as terminator.
    *first = 0;
    *first = new CallbackStack(first);
}

void CallbackStack::shutdown(CallbackStack** first)
{
    CallbackStack* next;
    for (CallbackStack* current = *first; current; current = next) {
        next = current->m_next;
        delete current;
    }
    *first = 0;
}

CallbackStack::~CallbackStack()
{
#if ENABLE(ASSERT)
    clearUnused();
#endif
}

void CallbackStack::clearUnused()
{
    for (size_t i = 0; i < bufferSize; i++)
        m_buffer[i] = Item(0, 0);
}

bool CallbackStack::isEmpty()
{
    return currentBlockIsEmpty() && !m_next;
}

CallbackStack* CallbackStack::takeCallbacks(CallbackStack** first)
{
    // If there is a full next block unlink and return it.
    if (m_next) {
        CallbackStack* result = m_next;
        m_next = result->m_next;
        result->m_next = 0;
        return result;
    }
    // Only the current block is in the stack. If the current block is
    // empty return 0.
    if (currentBlockIsEmpty())
        return 0;
    // The current block is not empty. Return this block and insert a
    // new empty block as the marking stack.
    *first = 0;
    *first = new CallbackStack(first);
    return this;
}

CallbackStack::Item* CallbackStack::pop(CallbackStack** first)
{
    if (currentBlockIsEmpty()) {
        if (!m_next) {
#if ENABLE(ASSERT)
            clearUnused();
#endif
            return 0;
        }
        CallbackStack* nextStack = m_next;
        *first = nextStack;
        delete this;
        return nextStack->pop(first);
    }
    return --m_current;
}

void CallbackStack::invokeEphemeronCallbacks(CallbackStack** first, Visitor* visitor)
{
    CallbackStack* stack = 0;
    // The first block is the only one where new ephemerons are added, so we
    // call the callbacks on that last, to catch any new ephemerons discovered
    // in the callbacks.
    // However, if enough ephemerons were added, we may have a new block that
    // has been prepended to the chain. This will be very rare, but we can
    // handle the situation by starting again and calling all the callbacks
    // a second time.
    while (stack != *first) {
        stack = *first;
        stack->invokeOldestCallbacks(visitor);
    }
}

void CallbackStack::invokeOldestCallbacks(Visitor* visitor)
{
    // Recurse first (bufferSize at a time) so we get to the newly added entries
    // last.
    if (m_next)
        m_next->invokeOldestCallbacks(visitor);

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
bool CallbackStack::hasCallbackForObject(const void* object)
{
    for (unsigned i = 0; m_buffer + i < m_current; i++) {
        Item* item = &m_buffer[i];
        if (item->object() == object) {
            return true;
        }
    }
    if (m_next)
        return m_next->hasCallbackForObject(object);

    return false;
}
#endif

}
