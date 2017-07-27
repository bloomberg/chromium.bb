/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "platform/fonts/FontSelectionAlgorithm.h"

namespace blink {

auto FontSelectionAlgorithm::StretchDistance(
    FontSelectionCapabilities capabilities) const -> DistanceResult {
  auto width = capabilities.width;
  DCHECK(width.IsValid());
  if (width.Includes(request_.width))
    return {FontSelectionValue(), request_.width};

  if (request_.width > NormalWidthValue()) {
    if (width.minimum > request_.width)
      return {width.minimum - request_.width, width.minimum};
    DCHECK(width.maximum < request_.width);
    auto threshold =
        std::max(request_.width, capabilities_bounds_.width.maximum);
    return {threshold - width.maximum, width.maximum};
  }

  if (width.maximum < request_.width)
    return {request_.width - width.maximum, width.maximum};
  DCHECK(width.minimum > request_.width);
  auto threshold = std::min(request_.width, capabilities_bounds_.width.minimum);
  return {width.minimum - threshold, width.minimum};
}

auto FontSelectionAlgorithm::StyleDistance(
    FontSelectionCapabilities capabilities) const -> DistanceResult {
  auto slope = capabilities.slope;
  DCHECK(slope.IsValid());
  if (slope.Includes(request_.slope))
    return {FontSelectionValue(), request_.slope};

  if (request_.slope >= ItalicThreshold()) {
    if (slope.minimum > request_.slope)
      return {slope.minimum - request_.slope, slope.minimum};
    DCHECK(request_.slope > slope.maximum);
    auto threshold =
        std::max(request_.slope, capabilities_bounds_.slope.maximum);
    return {threshold - slope.maximum, slope.maximum};
  }

  if (request_.slope >= FontSelectionValue()) {
    if (slope.maximum >= FontSelectionValue() && slope.maximum < request_.slope)
      return {request_.slope - slope.maximum, slope.maximum};
    if (slope.minimum > request_.slope)
      return {slope.minimum, slope.minimum};
    DCHECK(slope.maximum < FontSelectionValue());
    auto threshold =
        std::max(request_.slope, capabilities_bounds_.slope.maximum);
    return {threshold - slope.maximum, slope.maximum};
  }

  if (request_.slope > -ItalicThreshold()) {
    if (slope.minimum > request_.slope && slope.minimum <= FontSelectionValue())
      return {slope.minimum - request_.slope, slope.minimum};
    if (slope.maximum < request_.slope)
      return {-slope.maximum, slope.maximum};
    DCHECK(slope.minimum > FontSelectionValue());
    auto threshold =
        std::min(request_.slope, capabilities_bounds_.slope.minimum);
    return {slope.minimum - threshold, slope.minimum};
  }

  if (slope.maximum < request_.slope)
    return {request_.slope - slope.maximum, slope.maximum};
  DCHECK(slope.minimum > request_.slope);
  auto threshold = std::min(request_.slope, capabilities_bounds_.slope.minimum);
  return {slope.minimum - threshold, slope.minimum};
}

auto FontSelectionAlgorithm::WeightDistance(
    FontSelectionCapabilities capabilities) const -> DistanceResult {
  auto weight = capabilities.weight;
  DCHECK(weight.IsValid());
  if (weight.Includes(request_.weight))
    return {FontSelectionValue(), request_.weight};

  // The spec states, for weights 400 and 500: "If the desired weight is
  // 400, 500 is checked first ... If the desired weight is 500, 400 is
  // checked first", section 4.c) of
  // https://drafts.csswg.org/css-fonts/#font-style-matching section
  FontSelectionValue offset(1);
  if (request_.weight == FontSelectionValue(400) &&
      weight.Includes(FontSelectionValue(500)))
    return {offset, FontSelectionValue(500)};
  if (request_.weight == FontSelectionValue(500) &&
      weight.Includes(FontSelectionValue(400)))
    return {offset, FontSelectionValue(400)};

  if (request_.weight <= WeightSearchThreshold()) {
    if (weight.maximum < request_.weight)
      return {request_.weight - weight.maximum + offset, weight.maximum};
    DCHECK(weight.minimum > request_.weight);
    auto threshold =
        std::min(request_.weight, capabilities_bounds_.weight.minimum);
    return {weight.minimum - threshold + offset, weight.minimum};
  }

  if (weight.minimum > request_.weight)
    return {weight.minimum - request_.weight + offset, weight.minimum};
  DCHECK(weight.maximum < request_.weight);
  auto threshold =
      std::max(request_.weight, capabilities_bounds_.weight.maximum);
  return {threshold - weight.maximum + offset, weight.maximum};
}

bool FontSelectionAlgorithm::IsBetterMatchForRequest(
    const FontSelectionCapabilities& firstCapabilities,
    const FontSelectionCapabilities& secondCapabilities) {
  auto stretchDistanceFirst = StretchDistance(firstCapabilities).distance;
  auto stretchDistanceSecond = StretchDistance(secondCapabilities).distance;
  if (stretchDistanceFirst < stretchDistanceSecond)
    return true;
  if (stretchDistanceFirst > stretchDistanceSecond)
    return false;

  auto styleDistanceFirst = StyleDistance(firstCapabilities).distance;
  auto styleDistanceSecond = StyleDistance(secondCapabilities).distance;
  if (styleDistanceFirst < styleDistanceSecond)
    return true;
  if (styleDistanceFirst > styleDistanceSecond)
    return false;

  auto weightDistanceFirst = WeightDistance(firstCapabilities).distance;
  auto weightDistanceSecond = WeightDistance(secondCapabilities).distance;
  if (weightDistanceFirst < weightDistanceSecond)
    return true;
  return false;
}

}  // namespace blink
