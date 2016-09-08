// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorPollingStrategy_h
#define SensorPollingStrategy_h

#include "device/generic_sensor/public/interfaces/sensor_provider.mojom-blink.h"
#include "platform/heap/Handle.h"
#include "wtf/Functional.h"

namespace blink {

// This class provides different polling behaviour depending on
// the given 'ReportingMode':
// - for 'CONTINUOUS' mode the polling function is invoked periodically
//   with the given frequency (on timer event).
// - for 'ONCHANGE' mode the polling function is invoked only after client
//   calls 'onSensorReadingChanged()' however considering the given frequency:
//   guaranteed not to be called more often than expected.
class SensorPollingStrategy {
public:
    static std::unique_ptr<SensorPollingStrategy> create(double frequency, std::unique_ptr<Function<void()>> pollFunc, device::mojom::blink::ReportingMode);

    virtual void onSensorReadingChanged() {}
    virtual void startPolling() = 0;
    virtual void stopPolling() = 0;

    virtual ~SensorPollingStrategy() {}
};

} // namespace blink

#endif // SensorPollingStrategy_h
