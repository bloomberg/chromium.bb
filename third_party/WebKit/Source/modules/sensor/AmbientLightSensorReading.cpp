// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/AmbientLightSensorReading.h"

#include "modules/sensor/SensorProxy.h"

namespace blink {

AmbientLightSensorReading::AmbientLightSensorReading(
    const AmbientLightSensorReadingInit& init)
    : SensorReading(nullptr), mAmbientLightSensorReadingInit(init) {}

AmbientLightSensorReading::AmbientLightSensorReading(SensorProxy* proxy)
    : SensorReading(proxy),
      mAmbientLightSensorReadingInit(AmbientLightSensorReadingInit()) {}

AmbientLightSensorReading::~AmbientLightSensorReading() = default;

double AmbientLightSensorReading::illuminance() const {
  if (mAmbientLightSensorReadingInit.hasIlluminance())
    return mAmbientLightSensorReadingInit.illuminance();

  if (!m_sensorProxy)
    return 0.0;
  return m_sensorProxy->reading().values[0];
}

bool AmbientLightSensorReading::isReadingUpdated(
    const SensorProxy::Reading& previous) const {
  if (!m_sensorProxy)
    return false;
  return previous.values[0] != illuminance();
}

DEFINE_TRACE(AmbientLightSensorReading) {
  SensorReading::trace(visitor);
}

}  // namespace blink
