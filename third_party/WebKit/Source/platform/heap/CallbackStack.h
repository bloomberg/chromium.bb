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

    CallbackStack();
    ~CallbackStack();

    void clear();

    Item* allocateEntry();
    Item* pop();

    bool isEmpty() const;
    bool sizeExceeds(size_t) const;

    void append(CallbackStack*);
    void takeBlockFrom(CallbackStack*);

    void invokeEphemeronCallbacks(Visitor*);

#if ENABLE(ASSERT)
    bool hasCallbackForObject(const void*);
#endif

    static const size_t blockSize = 200;

private:
    class Block;

    void invokeOldestCallbacks(Block*, Block*, Visitor*);
    bool hasJustOneBlock() const;
    void swap(CallbackStack* other);

    Block* m_first;
    Block* m_last;
};

}

#endif // CallbackStack_h
