/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/device_orientation/DeviceMotionData.h"

#include "device/sensors/public/cpp/motion_data.h"
#include "modules/device_orientation/DeviceAccelerationInit.h"
#include "modules/device_orientation/DeviceMotionEventInit.h"
#include "modules/device_orientation/DeviceRotationRateInit.h"

namespace blink {

DeviceMotionData::Acceleration* DeviceMotionData::Acceleration::create(
    bool canProvideX,
    double x,
    bool canProvideY,
    double y,
    bool canProvideZ,
    double z) {
  return new DeviceMotionData::Acceleration(canProvideX, x, canProvideY, y,
                                            canProvideZ, z);
}

DeviceMotionData::Acceleration* DeviceMotionData::Acceleration::create(
    const DeviceAccelerationInit& init) {
  return new DeviceMotionData::Acceleration(
      init.hasX(), init.hasX() ? init.x() : 0, init.hasY(),
      init.hasY() ? init.y() : 0, init.hasZ(), init.hasZ() ? init.z() : 0);
}

DeviceMotionData::Acceleration::Acceleration(bool canProvideX,
                                             double x,
                                             bool canProvideY,
                                             double y,
                                             bool canProvideZ,
                                             double z)
    : m_x(x),
      m_y(y),
      m_z(z),
      m_canProvideX(canProvideX),
      m_canProvideY(canProvideY),
      m_canProvideZ(canProvideZ)

{}

DeviceMotionData::RotationRate* DeviceMotionData::RotationRate::create(
    bool canProvideAlpha,
    double alpha,
    bool canProvideBeta,
    double beta,
    bool canProvideGamma,
    double gamma) {
  return new DeviceMotionData::RotationRate(
      canProvideAlpha, alpha, canProvideBeta, beta, canProvideGamma, gamma);
}

DeviceMotionData::RotationRate* DeviceMotionData::RotationRate::create(
    const DeviceRotationRateInit& init) {
  return new DeviceMotionData::RotationRate(
      init.hasAlpha(), init.hasAlpha() ? init.alpha() : 0, init.hasBeta(),
      init.hasBeta() ? init.beta() : 0, init.hasGamma(),
      init.hasGamma() ? init.gamma() : 0);
}

DeviceMotionData::RotationRate::RotationRate(bool canProvideAlpha,
                                             double alpha,
                                             bool canProvideBeta,
                                             double beta,
                                             bool canProvideGamma,
                                             double gamma)
    : m_alpha(alpha),
      m_beta(beta),
      m_gamma(gamma),
      m_canProvideAlpha(canProvideAlpha),
      m_canProvideBeta(canProvideBeta),
      m_canProvideGamma(canProvideGamma) {}

DeviceMotionData* DeviceMotionData::create() {
  return new DeviceMotionData;
}

DeviceMotionData* DeviceMotionData::create(
    Acceleration* acceleration,
    Acceleration* accelerationIncludingGravity,
    RotationRate* rotationRate,
    double interval) {
  return new DeviceMotionData(acceleration, accelerationIncludingGravity,
                              rotationRate, interval);
}

DeviceMotionData* DeviceMotionData::create(const DeviceMotionEventInit& init) {
  return DeviceMotionData::create(
      init.hasAcceleration()
          ? DeviceMotionData::Acceleration::create(init.acceleration())
          : nullptr,
      init.hasAccelerationIncludingGravity()
          ? DeviceMotionData::Acceleration::create(
                init.accelerationIncludingGravity())
          : nullptr,
      init.hasRotationRate()
          ? DeviceMotionData::RotationRate::create(init.rotationRate())
          : nullptr,
      init.interval());
}

DeviceMotionData* DeviceMotionData::create(const device::MotionData& data) {
  return DeviceMotionData::create(
      DeviceMotionData::Acceleration::create(
          data.has_acceleration_x, data.acceleration_x, data.has_acceleration_y,
          data.acceleration_y, data.has_acceleration_z, data.acceleration_z),
      DeviceMotionData::Acceleration::create(
          data.has_acceleration_including_gravity_x,
          data.acceleration_including_gravity_x,
          data.has_acceleration_including_gravity_y,
          data.acceleration_including_gravity_y,
          data.has_acceleration_including_gravity_z,
          data.acceleration_including_gravity_z),
      DeviceMotionData::RotationRate::create(
          data.has_rotation_rate_alpha, data.rotation_rate_alpha,
          data.has_rotation_rate_beta, data.rotation_rate_beta,
          data.has_rotation_rate_gamma, data.rotation_rate_gamma),
      data.interval);
}

DeviceMotionData::DeviceMotionData() : m_interval(0) {}

DeviceMotionData::DeviceMotionData(Acceleration* acceleration,
                                   Acceleration* accelerationIncludingGravity,
                                   RotationRate* rotationRate,
                                   double interval)
    : m_acceleration(acceleration),
      m_accelerationIncludingGravity(accelerationIncludingGravity),
      m_rotationRate(rotationRate),
      m_interval(interval) {}

DEFINE_TRACE(DeviceMotionData) {
  visitor->trace(m_acceleration);
  visitor->trace(m_accelerationIncludingGravity);
  visitor->trace(m_rotationRate);
}

bool DeviceMotionData::canProvideEventData() const {
  const bool hasAcceleration =
      m_acceleration &&
      (m_acceleration->canProvideX() || m_acceleration->canProvideY() ||
       m_acceleration->canProvideZ());
  const bool hasAccelerationIncludingGravity =
      m_accelerationIncludingGravity &&
      (m_accelerationIncludingGravity->canProvideX() ||
       m_accelerationIncludingGravity->canProvideY() ||
       m_accelerationIncludingGravity->canProvideZ());
  const bool hasRotationRate =
      m_rotationRate &&
      (m_rotationRate->canProvideAlpha() || m_rotationRate->canProvideBeta() ||
       m_rotationRate->canProvideGamma());

  return hasAcceleration || hasAccelerationIncludingGravity || hasRotationRate;
}

}  // namespace blink
