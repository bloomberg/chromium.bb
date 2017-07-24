/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#ifndef Cone_h
#define Cone_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatPoint3D.h"
#include "platform/wtf/Allocator.h"

namespace blink {

// Cone gain is defined according to the OpenAL specification

class PLATFORM_EXPORT ConeEffect {
  DISALLOW_NEW();

 public:
  ConeEffect();

  // Returns scalar gain for the given source/listener positions/orientations
  double Gain(FloatPoint3D source_position,
              FloatPoint3D source_orientation,
              FloatPoint3D listener_position);

  // Angles in degrees
  void SetInnerAngle(double inner_angle) { inner_angle_ = inner_angle; }
  double InnerAngle() const { return inner_angle_; }

  void SetOuterAngle(double outer_angle) { outer_angle_ = outer_angle; }
  double OuterAngle() const { return outer_angle_; }

  void SetOuterGain(double outer_gain) { outer_gain_ = outer_gain; }
  double OuterGain() const { return outer_gain_; }

 protected:
  double inner_angle_;
  double outer_angle_;
  double outer_gain_;
};

}  // namespace blink

#endif  // Cone_h
