// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/display_util.h"

#include <stddef.h>
#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/types/display_snapshot.h"

namespace display {
namespace {

// The list of deltas between two consecutive zoom level. Any display must have
// one of these values as the difference between two consecutive zoom level.
constexpr std::array<double, 7> kZoomFactorDeltas = {0.05f, 0.1f, 0.15f, 0.2f,
                                                     0.25f, 0.5f, 1.f};

// The maximum logical resolution width allowed when zooming out for a display.
constexpr int kDefaultMaxZoomWidth = 4096;

// The minimum logical resolution width allowed when zooming in for a display.
constexpr int kDefaultMinZoomWidth = 640;

// The total number of display zoom factors to enumerate.
constexpr int kNumOfZoomFactors = 9;

}  // namespace

std::string DisplayPowerStateToString(chromeos::DisplayPowerState state) {
  switch (state) {
    case chromeos::DISPLAY_POWER_ALL_ON:
      return "ALL_ON";
    case chromeos::DISPLAY_POWER_ALL_OFF:
      return "ALL_OFF";
    case chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON:
      return "INTERNAL_OFF_EXTERNAL_ON";
    case chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF:
      return "INTERNAL_ON_EXTERNAL_OFF";
    default:
      return "unknown (" + base::IntToString(state) + ")";
  }
}

std::string MultipleDisplayStateToString(MultipleDisplayState state) {
  switch (state) {
    case MULTIPLE_DISPLAY_STATE_INVALID:
      return "INVALID";
    case MULTIPLE_DISPLAY_STATE_HEADLESS:
      return "HEADLESS";
    case MULTIPLE_DISPLAY_STATE_SINGLE:
      return "SINGLE";
    case MULTIPLE_DISPLAY_STATE_DUAL_MIRROR:
      return "DUAL_MIRROR";
    case MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED:
      return "MULTI_EXTENDED";
  }
  NOTREACHED() << "Unknown state " << state;
  return "INVALID";
}

int GetDisplayPower(const std::vector<DisplaySnapshot*>& displays,
                    chromeos::DisplayPowerState state,
                    std::vector<bool>* display_power) {
  int num_on_displays = 0;
  if (display_power)
    display_power->resize(displays.size());

  for (size_t i = 0; i < displays.size(); ++i) {
    bool internal = displays[i]->type() == DISPLAY_CONNECTION_TYPE_INTERNAL;
    bool on =
        state == chromeos::DISPLAY_POWER_ALL_ON ||
        (state == chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON &&
         !internal) ||
        (state == chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF && internal);
    if (display_power)
      (*display_power)[i] = on;
    if (on)
      num_on_displays++;
  }
  return num_on_displays;
}

bool IsPhysicalDisplayType(DisplayConnectionType type) {
  return !(type & DISPLAY_CONNECTION_TYPE_NETWORK);
}

std::vector<double> GetDisplayZoomFactors(const ManagedDisplayMode& mode) {
  const int effective_width = std::round(
      static_cast<float>(mode.size().width()) / mode.device_scale_factor());

  // We want to support displays greater than 4K. This is added to ensure the
  // zoom does not break in such cases.
  const int max_width = std::max(effective_width, kDefaultMaxZoomWidth);
  const int min_width = std::min(effective_width, kDefaultMinZoomWidth);

  // The logical resolution will vary from half of the mode resolution to double
  // the mode resolution.
  int max_effective_width =
      std::min(static_cast<int>(std::round(effective_width * 2.f)), max_width);
  int min_effective_width =
      std::max(static_cast<int>(std::round(effective_width / 2.f)), min_width);

  // If either the maximum width or minimum width was reached in the above step
  // and clamping was performed, then update the total range of logical
  // resolutions and ensure that everything lies within the maximum and minimum
  // resolution range.
  const int interval = std::round(static_cast<double>(effective_width) * 1.5f);
  if (max_effective_width == max_width)
    min_effective_width = std::max(max_effective_width - interval, min_width);
  if (min_effective_width == min_width)
    max_effective_width = std::min(min_effective_width + interval, max_width);

  double max_zoom = static_cast<double>(effective_width) /
                    static_cast<double>(min_effective_width);
  double min_zoom = static_cast<double>(effective_width) /
                    static_cast<double>(max_effective_width);

  double delta =
      (max_zoom - min_zoom) / static_cast<double>(kNumOfZoomFactors - 1);

  // Number of zoom values above 100% zoom.
  const int zoom_in_count = std::round((max_zoom - 1.f) / delta);

  // Number of zoom values below 100% zoom.
  const int zoom_out_count = kNumOfZoomFactors - zoom_in_count - 1;

  // Clamp the delta between consecutive zoom factors to a user friendly and UI
  // friendly value.
  std::size_t delta_index = 0;
  while (delta_index < kZoomFactorDeltas.size() &&
         delta >= kZoomFactorDeltas[delta_index]) {
    delta_index++;
  }
  delta = kZoomFactorDeltas[delta_index - 1];

  min_zoom = 1.f - delta * zoom_out_count;

  std::vector<double> zoom_values;
  for (int i = 0; i < kNumOfZoomFactors; i++)
    zoom_values.push_back(min_zoom + i * delta);
  return zoom_values;
}

}  // namespace display
