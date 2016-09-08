// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorPollingStrategy.h"

namespace blink {

// static
std::unique_ptr<SensorPollingStrategy> SensorPollingStrategy::create(double frequency, std::unique_ptr<Function<void()>> func, device::mojom::blink::ReportingMode mode)
{
    // TODO(Mikhail): Implement.
    return nullptr;
}

} // namespace blink
