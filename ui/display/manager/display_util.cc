// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_util.h"

#include <stddef.h>
#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/types/display_snapshot.h"

namespace display {
namespace {

// The maximum logical resolution width allowed when zooming out for a display.
constexpr int kDefaultMaxZoomWidth = 4096;

// The minimum logical resolution width allowed when zooming in for a display.
constexpr int kDefaultMinZoomWidth = 640;

// The total number of display zoom factors to enumerate.
constexpr int kNumOfZoomFactors = 9;

bool WithinEpsilon(float a, float b) {
  return std::abs(a - b) < std::numeric_limits<float>::epsilon();
}

// Clamps the delta between consecutive zoom factors to a user friendly and UI
// friendly value.
float ClampToUserFriendlyDelta(float delta) {
  // NOTE: If these thresholds are updated, please also update aura-shell.xml.
  // The list of deltas between two consecutive zoom level. Any display must
  // have one of these values as the difference between two consecutive zoom
  // level.
  // The array of pair represents which user friendly delta value must the given
  // raw |delta| be clamped to.
  // std::pair::first - represents the threshold.
  // std::pair::second - represents the user friendly clamped delta
  constexpr std::array<std::pair<float, float>, 7> kZoomFactorDeltas = {
      {{0.05f, 0.05f},
       {0.1f, 0.1f},
       {0.15f, 0.15f},
       {0.2f, 0.2f},
       {0.25f, 0.25f},
       {0.7f, 0.3f},
       {1.f, 0.5f}}};
  std::size_t delta_index = 0;
  while (delta_index < kZoomFactorDeltas.size() &&
         delta >= kZoomFactorDeltas[delta_index].first) {
    delta_index++;
  }
  return kZoomFactorDeltas[delta_index - 1].second;
}

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

std::vector<float> GetDisplayZoomFactors(const ManagedDisplayMode& mode) {
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
  const int interval = std::round(static_cast<float>(effective_width) * 1.5f);
  if (max_effective_width == max_width)
    min_effective_width = std::max(max_effective_width - interval, min_width);
  if (min_effective_width == min_width)
    max_effective_width = std::min(min_effective_width + interval, max_width);

  float max_zoom = static_cast<float>(effective_width) /
                   static_cast<float>(min_effective_width);
  float min_zoom = static_cast<float>(effective_width) /
                   static_cast<float>(max_effective_width);

  float delta =
      (max_zoom - min_zoom) / static_cast<float>(kNumOfZoomFactors - 1);

  // Number of zoom values above 100% zoom.
  const int zoom_in_count = std::round((max_zoom - 1.f) / delta);

  // Number of zoom values below 100% zoom.
  const int zoom_out_count = kNumOfZoomFactors - zoom_in_count - 1;

  delta = ClampToUserFriendlyDelta(delta);

  // Populate the zoom values list.
  min_zoom = 1.f - delta * zoom_out_count;
  std::vector<float> zoom_values;
  for (int i = 0; i < kNumOfZoomFactors; i++)
    zoom_values.push_back(min_zoom + i * delta);

  // Make sure the inverse of the internal device scale factor is included in
  // the list. This ensures that the users have a way to go to the native
  // resolution and 1.0 effective device scale factor.
  InsertDsfIntoList(&zoom_values, 1.f / mode.device_scale_factor());

  DCHECK_EQ(zoom_values.size(), static_cast<std::size_t>(kNumOfZoomFactors));
  return zoom_values;
}

void InsertDsfIntoList(std::vector<float>* zoom_values, float dsf) {
  // 1.0 is already in the list of |zoom_values|. We do not need to add it.
  if (WithinEpsilon(dsf, 1.f))
    return;

  if (dsf > 1.f && WithinEpsilon(*(zoom_values->rbegin()), 1.f)) {
    // If the last element of the vector is 1 then |dsf|, which is greater than
    // 1, will simply be inserted after that.
    zoom_values->push_back(dsf);
    zoom_values->erase(zoom_values->begin());
    return;
  } else if (dsf < 1.f && WithinEpsilon(*(zoom_values->begin()), 1.f)) {
    // If the first element in the list is 1 then |dsf|, which is less than 1,
    // will simply be inseted before that.
    zoom_values->insert(zoom_values->begin(), dsf);
    zoom_values->pop_back();
    return;
  }

  // We dont need to add |dsf| to the list if it is already in the list.
  auto it = std::lower_bound(zoom_values->begin(), zoom_values->end(), dsf);
  if (WithinEpsilon(*it, dsf))
    return;

  if (it == zoom_values->begin()) {
    DCHECK_LT(dsf, 1.f);
    *(zoom_values->begin()) = dsf;
  } else if (it == zoom_values->end()) {
    DCHECK_LT(dsf, 1.f);
    *(zoom_values->rbegin()) = dsf;
  } else {
    // There can only be 1 entry for 1.f value.
    DCHECK(!(WithinEpsilon(*(it - 1), 1.f) && WithinEpsilon(*it, 1.f)));

    // True if |dsf| is closer to |it| than it is to |it-1|.
    const bool dsf_closer_to_it =
        std::abs(*it - dsf) < std::abs(*(it - 1) - dsf);
    if (WithinEpsilon(*(it - 1), 1.f) ||
        (dsf_closer_to_it && !WithinEpsilon(*it, 1.f))) {
      *it = dsf;
    } else {
      *(it - 1) = dsf;
    }
  }
}

}  // namespace display
