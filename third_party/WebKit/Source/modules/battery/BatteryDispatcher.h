// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BatteryDispatcher_h
#define BatteryDispatcher_h

#include "core/frame/PlatformEventDispatcher.h"
#include "modules/battery/BatteryManager.h"
#include "modules/battery/BatteryStatus.h"
#include "public/platform/WebBatteryStatusListener.h"

namespace blink {

class WebBatteryStatus;

class BatteryDispatcher FINAL : public PlatformEventDispatcher, public WebBatteryStatusListener {
public:
    static BatteryDispatcher& instance();
    virtual ~BatteryDispatcher();

    BatteryStatus* latestData();

    // Inherited from WebBatteryStatusListener.
    virtual void updateBatteryStatus(const WebBatteryStatus&) OVERRIDE;

private:
    BatteryDispatcher();

    // Inherited from PlatformEventDispatcher.
    virtual void startListening() OVERRIDE;
    virtual void stopListening() OVERRIDE;

    Persistent<BatteryStatus> m_batteryStatus;
};

} // namespace blink

#endif // BatteryDispatcher_h
