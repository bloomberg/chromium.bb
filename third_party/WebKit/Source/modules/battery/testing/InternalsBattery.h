// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InternalsBattery_h
#define InternalsBattery_h

#include "wtf/Allocator.h"

namespace blink {

class Internals;

// TODO(yukishiino): Remove this API once JS bindings of Mojo services get
// available.
class InternalsBattery {
    STATIC_ONLY(InternalsBattery);

public:
    static void updateBatteryStatus(Internals&, bool charging, double chargingTime, double dischargingTime, double level);
};

} // namespace blink

#endif // InternalsBattery_h
