// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementMicrotaskRunQueue_h
#define CustomElementMicrotaskRunQueue_h

#include "platform/heap/Handle.h"
#include "wtf/RefCounted.h"
#include "wtf/WeakPtr.h"

namespace blink {

class CustomElementSyncMicrotaskQueue;
class CustomElementAsyncImportMicrotaskQueue;
class CustomElementMicrotaskStep;
class HTMLImportLoader;

class CustomElementMicrotaskRunQueue : public RefCountedWillBeGarbageCollectedFinalized<CustomElementMicrotaskRunQueue> {
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(CustomElementMicrotaskRunQueue)
public:
    static PassRefPtrWillBeRawPtr<CustomElementMicrotaskRunQueue> create() { return adoptRefWillBeNoop(new CustomElementMicrotaskRunQueue()); }

    void enqueue(HTMLImportLoader* parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskStep>, bool importIsSync);
    void requestDispatchIfNeeded();
    bool isEmpty() const;

    DECLARE_TRACE();

private:
    static void dispatchIfAlive(WeakPtrWillBeWeakPersistent<CustomElementMicrotaskRunQueue>);

    CustomElementMicrotaskRunQueue();

    void dispatch();

    RefPtrWillBeMember<CustomElementSyncMicrotaskQueue> m_syncQueue;
    RefPtrWillBeMember<CustomElementAsyncImportMicrotaskQueue> m_asyncQueue;
    bool m_dispatchIsPending;
#if !ENABLE(OILPAN)
    WeakPtrFactory<CustomElementMicrotaskRunQueue> m_weakFactory;
#endif
};

} // namespace blink

#endif // CustomElementMicrotaskRunQueue_h
