// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScreenOrientationDispatcher_h
#define ScreenOrientationDispatcher_h

#include "public/platform/WebScreenOrientationListener.h"
#include "wtf/Vector.h"

namespace WebCore {

class ScreenOrientationController;

class ScreenOrientationDispatcher FINAL : public blink::WebScreenOrientationListener {
public:
    static ScreenOrientationDispatcher& instance();

    void addController(ScreenOrientationController*);
    void removeController(ScreenOrientationController*);

private:
    ScreenOrientationDispatcher();

    void purgeControllers();
    void startListening();
    void stopListening();

    // WebScreenOrientationListener API.
    virtual void didChangeScreenOrientation(blink::WebScreenOrientationType) OVERRIDE;

    Vector<ScreenOrientationController*> m_controllers;
    bool m_needsPurge;
    bool m_isDispatching;
};

} // namespace WebCore

#endif // ScreenOrientationDispatcher_h
