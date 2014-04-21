// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BatteryStatus_h
#define BatteryStatus_h

#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class BatteryStatus : public RefCounted<BatteryStatus> {
public:
    static PassRefPtr<BatteryStatus> create();
    static PassRefPtr<BatteryStatus> create(bool charging, double chargingTime, double dischargingTime, double level);

    bool charging() const { return m_charging; }
    double chargingTime() const  { return m_chargingTime; }
    double dischargingTime() const  { return m_dischargingTime; }
    double level() const  { return m_level; }

private:
    BatteryStatus();
    BatteryStatus(bool charging, double chargingTime, double dischargingTime, double level);

    bool m_charging;
    double m_chargingTime;
    double m_dischargingTime;
    double m_level;
};

} // namespace WebCore

#endif // BatteryStatus_h

