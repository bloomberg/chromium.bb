// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationDispatcher_h
#define ScreenOrientationDispatcher_h

#include "platform/heap/Handle.h"
#include "public/platform/WebScreenOrientationListener.h"

#if ENABLE(OILPAN)
#include "wtf/HashSet.h"
#else
#include "wtf/Vector.h"
#endif

namespace WebCore {

class ScreenOrientationController;

class ScreenOrientationDispatcher FINAL : public NoBaseWillBeGarbageCollectedFinalized<ScreenOrientationDispatcher>, public blink::WebScreenOrientationListener {
public:
    static ScreenOrientationDispatcher& instance();

    void addController(ScreenOrientationController*);

#if ENABLE(OILPAN)
    void trace(Visitor*);
#else
    void removeController(ScreenOrientationController*);
#endif

private:
    ScreenOrientationDispatcher();

#if !ENABLE(OILPAN)
    void purgeControllers();
#endif

    void startListening();
    void stopListening();

    // WebScreenOrientationListener API.
    virtual void didChangeScreenOrientation(blink::WebScreenOrientationType) OVERRIDE;

#if ENABLE(OILPAN)
    HeapHashSet<WeakMember<ScreenOrientationController> > m_controllers;
#else
    Vector<ScreenOrientationController*> m_controllers;
    bool m_needsPurge;
    bool m_isDispatching;
#endif

};

} // namespace WebCore

#endif // ScreenOrientationDispatcher_h
