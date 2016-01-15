// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMWindowWorklet_h
#define DOMWindowWorklet_h

#include "core/frame/DOMWindowProperty.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class DOMWindow;
class ExecutionContext;
class Worklet;

class DOMWindowWorklet final : public NoBaseWillBeGarbageCollected<DOMWindowWorklet>, public WillBeHeapSupplement<LocalDOMWindow>, public DOMWindowProperty {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(DOMWindowWorklet);
    DECLARE_EMPTY_VIRTUAL_DESTRUCTOR_WILL_BE_REMOVED(DOMWindowWorklet);
    USING_FAST_MALLOC_WILL_BE_REMOVED(DOMWindowWorklet);
public:
    static DOMWindowWorklet& from(LocalDOMWindow&);
    static Worklet* renderWorklet(ExecutionContext*, DOMWindow&);
    Worklet* renderWorklet(ExecutionContext*) const;

    DECLARE_TRACE();

private:
    explicit DOMWindowWorklet(LocalDOMWindow&);
    static const char* supplementName();

    mutable PersistentWillBeMember<Worklet> m_renderWorklet;
};

} // namespace blink

#endif // DOMWindowWorklet_h
