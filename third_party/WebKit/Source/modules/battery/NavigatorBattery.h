// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorBattery_h
#define NavigatorBattery_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"

namespace WebCore {

class BatteryManager;
class Navigator;

class NavigatorBattery : public Supplement<Navigator> {
public:
    virtual ~NavigatorBattery();

    static NavigatorBattery& from(Navigator&);

    static BatteryManager* battery(Navigator&);
    BatteryManager* batteryManager(Navigator&);

private:
    explicit NavigatorBattery();
    static const char* supplementName();

    RefPtr<BatteryManager> m_batteryManager;
};

} // namespace WebCore

#endif // NavigatorBattery_h
