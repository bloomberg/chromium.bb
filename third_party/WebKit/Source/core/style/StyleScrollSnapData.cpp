/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/style/StyleScrollSnapData.h"

#include "core/style/ComputedStyle.h"

namespace blink {

ScrollSnapPoints::ScrollSnapPoints()
    : repeat_offset(100, kPercent), has_repeat(false), uses_elements(false) {}

bool operator==(const ScrollSnapPoints& a, const ScrollSnapPoints& b) {
  return a.repeat_offset == b.repeat_offset && a.has_repeat == b.has_repeat &&
         a.uses_elements == b.uses_elements;
}

StyleScrollSnapData::StyleScrollSnapData()
    : x_points_(ComputedStyle::InitialScrollSnapPointsX()),
      y_points_(ComputedStyle::InitialScrollSnapPointsY()),
      destination_(ComputedStyle::InitialScrollSnapDestination()),
      coordinates_(ComputedStyle::InitialScrollSnapCoordinate()) {}

StyleScrollSnapData::StyleScrollSnapData(const StyleScrollSnapData& other)
    : x_points_(other.x_points_),
      y_points_(other.y_points_),
      destination_(other.destination_),
      coordinates_(other.coordinates_) {}

bool operator==(const StyleScrollSnapData& a, const StyleScrollSnapData& b) {
  return a.x_points_ == b.x_points_ && a.y_points_ == b.y_points_ &&
         a.destination_ == b.destination_ && a.coordinates_ == b.coordinates_;
}

}  // namespace blink
