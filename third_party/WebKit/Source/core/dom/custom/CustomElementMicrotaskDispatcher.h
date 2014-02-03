// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementMicrotaskDispatcher_h
#define CustomElementMicrotaskDispatcher_h

#include "core/dom/custom/CustomElementMicrotaskQueue.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class CustomElementCallbackQueue;
class CustomElementMicrotaskStep;
class HTMLImport;

class CustomElementMicrotaskDispatcher {
    WTF_MAKE_NONCOPYABLE(CustomElementMicrotaskDispatcher);
public:
    CustomElementMicrotaskDispatcher() : m_phase(Quiescent) { }
    ~CustomElementMicrotaskDispatcher() { }

    void enqueue(HTMLImport*, PassOwnPtr<CustomElementMicrotaskStep>);
    void enqueue(CustomElementCallbackQueue*);
    bool dispatch();
    bool elementQueueIsEmpty() { return m_elements.isEmpty(); }

private:
    enum {
        Quiescent,
        Resolving,
        DispatchingCallbacks
    } m_phase;

    CustomElementMicrotaskQueue m_resolutionAndImports;
    Vector<CustomElementCallbackQueue*> m_elements;
};

}

#endif // CustomElementMicrotaskDispatcher_h
