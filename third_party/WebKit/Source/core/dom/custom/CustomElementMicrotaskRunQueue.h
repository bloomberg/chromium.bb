// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementMicrotaskRunQueue_h
#define CustomElementMicrotaskRunQueue_h

#include "platform/heap/Handle.h"
#include "wtf/WeakPtr.h"

namespace blink {

class CustomElementSyncMicrotaskQueue;
class CustomElementAsyncImportMicrotaskQueue;
class CustomElementMicrotaskStep;
class HTMLImportLoader;

class CustomElementMicrotaskRunQueue : public GarbageCollectedFinalized<CustomElementMicrotaskRunQueue> {
public:
    static RawPtr<CustomElementMicrotaskRunQueue> create() { return new CustomElementMicrotaskRunQueue(); }

    void enqueue(HTMLImportLoader* parentLoader, RawPtr<CustomElementMicrotaskStep>, bool importIsSync);
    void requestDispatchIfNeeded();
    bool isEmpty() const;

    DECLARE_TRACE();

private:
    CustomElementMicrotaskRunQueue();

    void dispatch();

    Member<CustomElementSyncMicrotaskQueue> m_syncQueue;
    Member<CustomElementAsyncImportMicrotaskQueue> m_asyncQueue;
    bool m_dispatchIsPending;
#if !ENABLE(OILPAN)
    WeakPtrFactory<CustomElementMicrotaskRunQueue> m_weakFactory;
#endif
};

} // namespace blink

#endif // CustomElementMicrotaskRunQueue_h
