/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_acceleration.h"
#include "third_party/blink/renderer/modules/device_orientation/device_motion_event_acceleration_init.h"

namespace blink {

DeviceMotionEventAcceleration* DeviceMotionEventAcceleration::Create(double x,
                                                                     double y,
                                                                     double z) {
  return MakeGarbageCollected<DeviceMotionEventAcceleration>(x, y, z);
}

DeviceMotionEventAcceleration* DeviceMotionEventAcceleration::Create(
    const DeviceMotionEventAccelerationInit* init) {
  double x = init->hasX() ? init->x() : NAN;
  double y = init->hasY() ? init->y() : NAN;
  double z = init->hasZ() ? init->z() : NAN;
  return DeviceMotionEventAcceleration::Create(x, y, z);
}

DeviceMotionEventAcceleration::DeviceMotionEventAcceleration(double x,
                                                             double y,
                                                             double z)
    : x_(x), y_(y), z_(z) {}

bool DeviceMotionEventAcceleration::HasAccelerationData() const {
  return !std::isnan(x_) || !std::isnan(y_) || !std::isnan(z_);
}

double DeviceMotionEventAcceleration::x(bool& is_null) const {
  is_null = std::isnan(x_);
  return x_;
}

double DeviceMotionEventAcceleration::y(bool& is_null) const {
  is_null = std::isnan(y_);
  return y_;
}

double DeviceMotionEventAcceleration::z(bool& is_null) const {
  is_null = std::isnan(z_);
  return z_;
}

}  // namespace blink
