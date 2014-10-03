// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GamepadDispatcher_h
#define GamepadDispatcher_h

#include "core/frame/PlatformEventDispatcher.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebGamepad.h"
#include "public/platform/WebGamepadListener.h"

namespace blink {

class NavigatorGamepad;
class WebGamepads;

class GamepadDispatcher final : public GarbageCollectedFinalized<GamepadDispatcher>, public PlatformEventDispatcher, public WebGamepadListener {
    USING_GARBAGE_COLLECTED_MIXIN(GamepadDispatcher);
public:
    static GamepadDispatcher& instance();
    virtual ~GamepadDispatcher();

    void sampleGamepads(WebGamepads&);

    struct ConnectionChange {
        WebGamepad pad;
        unsigned index;
    };

    const ConnectionChange& latestConnectionChange() const { return m_latestChange; }

    virtual void trace(Visitor*) override;

private:
    GamepadDispatcher();

    // WebGamepadListener
    virtual void didConnectGamepad(unsigned index, const WebGamepad&) override;
    virtual void didDisconnectGamepad(unsigned index, const WebGamepad&) override;

    // PlatformEventDispatcher
    virtual void startListening() override;
    virtual void stopListening() override;

    void dispatchDidConnectOrDisconnectGamepad(unsigned index, const WebGamepad&, bool connected);

    ConnectionChange m_latestChange;
};

} // namespace blink

#endif
