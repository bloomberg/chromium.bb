// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/AmbientLightSensor.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/sensor/AmbientLightSensorReading.h"

using device::mojom::blink::SensorType;

namespace blink {

// static
AmbientLightSensor* AmbientLightSensor::create(ExecutionContext* context, const SensorOptions& sensorOptions)
{
    return new AmbientLightSensor(context, sensorOptions);
}

// static
AmbientLightSensor* AmbientLightSensor::create(ExecutionContext* context)
{
    return create(context, SensorOptions());
}

AmbientLightSensor::AmbientLightSensor(ExecutionContext* executionContext, const SensorOptions& sensorOptions)
    : Sensor(executionContext, sensorOptions, SensorType::AMBIENT_LIGHT)
{
}

AmbientLightSensorReading* AmbientLightSensor::reading() const
{
    return static_cast<AmbientLightSensorReading*>(Sensor::reading());
}

SensorReading* AmbientLightSensor::createSensorReading(SensorProxy* proxy)
{
    return AmbientLightSensorReading::create(proxy);
}

auto AmbientLightSensor::createSensorConfig(const SensorOptions& options, const SensorConfiguration& defaultConfig) -> SensorConfigurationPtr
{
    auto result = device::mojom::blink::SensorConfiguration::New();
    result->frequency = options.hasFrequency() ? options.frequency() : defaultConfig.frequency;
    return result;
}

DEFINE_TRACE(AmbientLightSensor)
{
    Sensor::trace(visitor);
}

} // namespace blink
