// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AmbientLightSensorReading_h
#define AmbientLightSensorReading_h

#include "modules/sensor/AmbientLightSensorReadingInit.h"
#include "modules/sensor/SensorReading.h"

namespace blink {

class AmbientLightSensorReading final
    : public GarbageCollectedFinalized<AmbientLightSensorReading>
    , public SensorReading {
    USING_GARBAGE_COLLECTED_MIXIN(AmbientLightSensorReading);
    DEFINE_WRAPPERTYPEINFO();
public:

    static AmbientLightSensorReading* create(const AmbientLightSensorReadingInit& init)
    {
        return new AmbientLightSensorReading(init);
    }

    static AmbientLightSensorReading* create(SensorProxy* proxy)
    {
        return new AmbientLightSensorReading(proxy);
    }

    ~AmbientLightSensorReading();

    double illuminance() const;

    bool isReadingUpdated(const SensorProxy::Reading&) const override;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit AmbientLightSensorReading(const AmbientLightSensorReadingInit&);
    explicit AmbientLightSensorReading(SensorProxy*);

private:
    AmbientLightSensorReadingInit mAmbientLightSensorReadingInit;
};

} // namepsace blink

#endif // AmbientLightSensorReading_h
