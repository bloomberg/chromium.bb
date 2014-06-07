// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceLightDispatcher_h
#define DeviceLightDispatcher_h

#include "core/frame/DeviceSensorEventDispatcher.h"
#include "public/platform/WebDeviceLightListener.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class DeviceLightController;

// This class listens to device light data and dispatches it to all
// listening controllers
class DeviceLightDispatcher FINAL : public DeviceSensorEventDispatcher, public blink::WebDeviceLightListener {
public:
    static DeviceLightDispatcher& instance();

    double latestDeviceLightData() const;

    // This method is called every time new device light data is available.
    virtual void didChangeDeviceLight(double) OVERRIDE;
    void addDeviceLightController(DeviceLightController*);
    void removeDeviceLightController(DeviceLightController*);

private:
    DeviceLightDispatcher();
    virtual ~DeviceLightDispatcher();

    virtual void startListening() OVERRIDE;
    virtual void stopListening() OVERRIDE;

    double m_lastDeviceLightData;
};

} // namespace WebCore

#endif // DeviceLightDispatcher_h
