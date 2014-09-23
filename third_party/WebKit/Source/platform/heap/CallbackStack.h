// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CallbackStack_h
#define CallbackStack_h

#include "platform/heap/ThreadState.h"

namespace blink {

// The CallbackStack contains all the visitor callbacks used to trace and mark
// objects. A specific CallbackStack instance contains at most bufferSize elements.
// If more space is needed a new CallbackStack instance is created and chained
// together with the former instance. I.e. a logical CallbackStack can be made of
// multiple chained CallbackStack object instances.
class CallbackStack {
public:
    CallbackStack(CallbackStack** first)
        : m_limit(&(m_buffer[bufferSize]))
        , m_current(&(m_buffer[0]))
        , m_next(*first)
    {
#if ENABLE(ASSERT)
        clearUnused();
#endif
        *first = this;
    }

    ~CallbackStack();
    void clearUnused();

    bool isEmpty();

    CallbackStack* takeCallbacks(CallbackStack** first);

    class Item {
    public:
        Item() { }
        Item(void* object, VisitorCallback callback)
            : m_object(object)
            , m_callback(callback)
        {
        }
        void* object() { return m_object; }
        VisitorCallback callback() { return m_callback; }
        void call(Visitor* visitor) { m_callback(visitor, m_object); }

    private:
        void* m_object;
        VisitorCallback m_callback;
    };

    static void init(CallbackStack** first);
    static void shutdown(CallbackStack** first);
    static void clear(CallbackStack** first)
    {
        if (!(*first)->isEmpty()) {
            shutdown(first);
            init(first);
        }
    }

    Item* pop(CallbackStack** first);
    static void invokeEphemeronCallbacks(CallbackStack** first, Visitor*);

    Item* allocateEntry(CallbackStack** first)
    {
        if (m_current < m_limit)
            return m_current++;
        return (new CallbackStack(first))->allocateEntry(first);
    }

#if ENABLE(ASSERT)
    bool hasCallbackForObject(const void*);
#endif

    bool numberOfBlocksExceeds(int blocks)
    {
        CallbackStack* current = this;
        for (int i = 0; i < blocks; ++i) {
            if (!current->m_next)
                return false;
            current = current->m_next;
        }
        return true;
    }

private:
    void invokeOldestCallbacks(Visitor*);
    bool currentBlockIsEmpty() { return m_current == &(m_buffer[0]); }

    static const size_t bufferSize = 200;
    Item m_buffer[bufferSize];
    Item* m_limit;
    Item* m_current;
    CallbackStack* m_next;
};

}

#endif // CallbackStack_h
