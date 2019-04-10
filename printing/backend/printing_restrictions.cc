// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/printing_restrictions.h"

namespace printing {

const char kAllowedColorModes[] = "allowedColorModes";
const char kAllowedDuplexModes[] = "allowedDuplexModes";
const char kAllowedPinModes[] = "allowedPinModes";
const char kDefaultColorMode[] = "defaultColorMode";
const char kDefaultDuplexMode[] = "defaultDuplexMode";
const char kDefaultPinMode[] = "defaultPinMode";
const char kPageWidthUm[] = "WidthUm";
const char kPageHeightUm[] = "HeightUm";

PrintingRestrictions::PrintingRestrictions() {}

PrintingRestrictions::~PrintingRestrictions() {}

base::Optional<ColorModeRestriction> GetColorModeForName(
    const std::string& mode_name) {
  if (mode_name == "monochrome")
    return ColorModeRestriction::kMonochrome;

  if (mode_name == "color")
    return ColorModeRestriction::kColor;

  return base::nullopt;
}

base::Optional<ColorModeRestriction> GetAllowedColorModesForName(
    const std::string& mode_name) {
  if (mode_name == "any")
    return ColorModeRestriction::kUnset;

  return GetColorModeForName(mode_name);
}

base::Optional<DuplexModeRestriction> GetDuplexModeForName(
    const std::string& mode_name) {
  if (mode_name == "simplex")
    return DuplexModeRestriction::kSimplex;

  if (mode_name == "long-edge")
    return DuplexModeRestriction::kLongEdge;

  if (mode_name == "short-edge")
    return DuplexModeRestriction::kShortEdge;

  return base::nullopt;
}

base::Optional<DuplexModeRestriction> GetAllowedDuplexModesForName(
    const std::string& mode_name) {
  if (mode_name == "any")
    return DuplexModeRestriction::kUnset;

  if (mode_name == "simplex")
    return DuplexModeRestriction::kSimplex;

  if (mode_name == "duplex")
    return DuplexModeRestriction::kDuplex;

  return base::nullopt;
}

base::Optional<PinModeRestriction> GetPinModeForName(
    const std::string& mode_name) {
  if (mode_name == "pin")
    return PinModeRestriction::kPin;

  if (mode_name == "no_pin")
    return PinModeRestriction::kNoPin;

  return base::nullopt;
}

base::Optional<PinModeRestriction> GetAllowedPinModesForName(
    const std::string& mode_name) {
  if (mode_name == "any")
    return PinModeRestriction::kUnset;

  return GetPinModeForName(mode_name);
}

}  // namespace printing
