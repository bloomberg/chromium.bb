/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "modules/device_orientation/DeviceOrientationData.h"

#include "device/sensors/public/cpp/orientation_data.h"
#include "modules/device_orientation/DeviceOrientationEventInit.h"

namespace blink {

DeviceOrientationData* DeviceOrientationData::Create() {
  return new DeviceOrientationData;
}

DeviceOrientationData* DeviceOrientationData::Create(
    const Nullable<double>& alpha,
    const Nullable<double>& beta,
    const Nullable<double>& gamma,
    bool absolute) {
  return new DeviceOrientationData(alpha, beta, gamma, absolute);
}

DeviceOrientationData* DeviceOrientationData::Create(
    const DeviceOrientationEventInit& init) {
  Nullable<double> alpha;
  Nullable<double> beta;
  Nullable<double> gamma;
  if (init.hasAlpha())
    alpha = init.alpha();
  if (init.hasBeta())
    beta = init.beta();
  if (init.hasGamma())
    gamma = init.gamma();
  return DeviceOrientationData::Create(alpha, beta, gamma, init.absolute());
}

DeviceOrientationData* DeviceOrientationData::Create(
    const device::OrientationData& data) {
  Nullable<double> alpha;
  Nullable<double> beta;
  Nullable<double> gamma;
  if (data.has_alpha)
    alpha = data.alpha;
  if (data.has_beta)
    beta = data.beta;
  if (data.has_gamma)
    gamma = data.gamma;
  return DeviceOrientationData::Create(alpha, beta, gamma, data.absolute);
}

DeviceOrientationData::DeviceOrientationData() : absolute_(false) {}

DeviceOrientationData::DeviceOrientationData(const Nullable<double>& alpha,
                                             const Nullable<double>& beta,
                                             const Nullable<double>& gamma,
                                             bool absolute)
    : alpha_(alpha), beta_(beta), gamma_(gamma), absolute_(absolute) {}

double DeviceOrientationData::Alpha() const {
  return alpha_.Get();
}

double DeviceOrientationData::Beta() const {
  return beta_.Get();
}

double DeviceOrientationData::Gamma() const {
  return gamma_.Get();
}

bool DeviceOrientationData::Absolute() const {
  return absolute_;
}

bool DeviceOrientationData::CanProvideAlpha() const {
  return !alpha_.IsNull();
}

bool DeviceOrientationData::CanProvideBeta() const {
  return !beta_.IsNull();
}

bool DeviceOrientationData::CanProvideGamma() const {
  return !gamma_.IsNull();
}

bool DeviceOrientationData::CanProvideEventData() const {
  return CanProvideAlpha() || CanProvideBeta() || CanProvideGamma();
}

}  // namespace blink
