// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WindowPaintWorklet_h
#define WindowPaintWorklet_h

#include "core/frame/DOMWindowProperty.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class DOMWindow;
class ExecutionContext;
class PaintWorklet;
class Worklet;

class WindowPaintWorklet final : public NoBaseWillBeGarbageCollected<WindowPaintWorklet>, public WillBeHeapSupplement<LocalDOMWindow>, public DOMWindowProperty {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(WindowPaintWorklet);
    DECLARE_EMPTY_VIRTUAL_DESTRUCTOR_WILL_BE_REMOVED(WindowPaintWorklet);
    USING_FAST_MALLOC_WILL_BE_REMOVED(WindowPaintWorklet);
public:
    static WindowPaintWorklet& from(LocalDOMWindow&);
    static Worklet* paintWorklet(ExecutionContext*, DOMWindow&);
    PaintWorklet* paintWorklet(ExecutionContext*) const;

    DECLARE_TRACE();

private:
    explicit WindowPaintWorklet(LocalDOMWindow&);
    static const char* supplementName();

    mutable PersistentWillBeMember<PaintWorklet> m_paintWorklet;
};

} // namespace blink

#endif // WindowPaintWorklet_h
