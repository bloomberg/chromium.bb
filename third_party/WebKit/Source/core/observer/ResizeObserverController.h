// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResizeObserverController_h
#define ResizeObserverController_h

#include "platform/heap/Handle.h"

namespace blink {

class ResizeObserver;

// ResizeObserverController keeps track of all ResizeObservers
// in a single Document.
class ResizeObserverController final : public GarbageCollected<ResizeObserverController> {
public:
    ResizeObserverController();

    void addObserver(ResizeObserver&);

    DECLARE_TRACE();

private:
    // Active observers
    HeapHashSet<WeakMember<ResizeObserver>> m_observers;
};

} // namespace blink

#endif
