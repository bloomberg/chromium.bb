// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BatteryDispatcher_h
#define BatteryDispatcher_h

#include "core/frame/PlatformEventDispatcher.h"
#include "modules/ModulesExport.h"
#include "modules/battery/BatteryManager.h"
#include "platform/battery/battery_dispatcher_proxy.h"
#include "wtf/OwnPtr.h"

namespace blink {

class MODULES_EXPORT BatteryDispatcher final : public GarbageCollectedFinalized<BatteryDispatcher>, public PlatformEventDispatcher, public BatteryDispatcherProxy::Listener {
    USING_GARBAGE_COLLECTED_MIXIN(BatteryDispatcher);
public:
    static BatteryDispatcher& instance();
    ~BatteryDispatcher() override;

    const BatteryStatus* latestData() const
    {
        return m_hasLatestData ? &m_batteryStatus : nullptr;
    }

    // Inherited from BatteryDispatcherProxy::Listener.
    void OnUpdateBatteryStatus(const BatteryStatus&) override;

private:
    BatteryDispatcher();

    // Inherited from PlatformEventDispatcher.
    void startListening() override;
    void stopListening() override;

    BatteryStatus m_batteryStatus;
    bool m_hasLatestData;
    OwnPtr<BatteryDispatcherProxy> m_batteryDispatcherProxy;
};

} // namespace blink

#endif // BatteryDispatcher_h
