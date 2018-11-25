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

#include "third_party/blink/renderer/modules/device_orientation/device_rotation_rate.h"
#include "third_party/blink/renderer/modules/device_orientation/device_rotation_rate_init.h"

namespace blink {

DeviceRotationRate* DeviceRotationRate::Create(double alpha,
                                               double beta,
                                               double gamma) {
  return MakeGarbageCollected<DeviceRotationRate>(alpha, beta, gamma);
}

DeviceRotationRate* DeviceRotationRate::Create(
    const DeviceRotationRateInit* init) {
  double alpha = init->hasAlpha() ? init->alpha() : NAN;
  double beta = init->hasBeta() ? init->beta() : NAN;
  double gamma = init->hasGamma() ? init->gamma() : NAN;
  return DeviceRotationRate::Create(alpha, beta, gamma);
}

DeviceRotationRate::DeviceRotationRate(double alpha, double beta, double gamma)
    : alpha_(alpha), beta_(beta), gamma_(gamma) {}

bool DeviceRotationRate::HasRotationData() const {
  return !std::isnan(alpha_) || !std::isnan(beta_) || !std::isnan(gamma_);
}

double DeviceRotationRate::alpha(bool& is_null) const {
  is_null = std::isnan(alpha_);
  return alpha_;
}

double DeviceRotationRate::beta(bool& is_null) const {
  is_null = std::isnan(beta_);
  return beta_;
}

double DeviceRotationRate::gamma(bool& is_null) const {
  is_null = std::isnan(gamma_);
  return gamma_;
}

}  // namespace blink
