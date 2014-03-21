// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GamepadDispatcher_h
#define GamepadDispatcher_h

#include "core/frame/DeviceSensorEventDispatcher.h"
#include "public/platform/WebGamepadListener.h"

namespace blink {
class WebGamepad;
class WebGamepads;
}

namespace WebCore {

class NavigatorGamepad;

class GamepadDispatcher : public DeviceSensorEventDispatcher, public blink::WebGamepadListener {
public:
    static GamepadDispatcher& instance();

    void addClient(NavigatorGamepad*);
    void removeClient(NavigatorGamepad*);
    void sampleGamepads(blink::WebGamepads&);

private:
    GamepadDispatcher();
    virtual ~GamepadDispatcher();

    virtual void didConnectGamepad(unsigned index, const blink::WebGamepad&) OVERRIDE;
    virtual void didDisconnectGamepad(unsigned index, const blink::WebGamepad&) OVERRIDE;
    void dispatchDidConnectOrDisconnectGamepad(unsigned index, const blink::WebGamepad&, bool connected);

    virtual void startListening() OVERRIDE;
    virtual void stopListening() OVERRIDE;
};

} // namespace WebCore

#endif
