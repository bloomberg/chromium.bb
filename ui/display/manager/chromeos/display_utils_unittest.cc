// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/display_util.h"

#include <vector>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/size.h"

namespace display {
namespace test {

namespace {
constexpr std::size_t kNumOfZoomFactors = 9;
constexpr int kDefaultMaxZoomWidth = 4096;
constexpr int kDefaultMinZoomWidth = 640;
}  // namespace
using DisplayUtilTest = testing::Test;

TEST_F(DisplayUtilTest, DisplayZooms) {
  // A vector of pairs where each pair is the resolution width correspoinding
  // to its list of available display zoom values.
  const std::vector<std::pair<int, std::vector<double>>> expected_zoom_values{
      {480, {0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.f}},
      {640, {0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.f}},
      {720, {0.65f, 0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.f, 1.05f}},
      {800, {0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.f, 1.10f, 1.20f}},
      {960, {0.60f, 0.70f, 0.80f, 0.90f, 1.f, 1.10f, 1.20f, 1.30f, 1.40f}},
      {1024, {0.60f, 0.70f, 0.80f, 0.90f, 1.f, 1.10f, 1.20f, 1.30f, 1.40f}},
      {1280, {0.55f, 0.70f, 0.85f, 1.f, 1.15f, 1.30f, 1.45f, 1.60f, 1.75f}},
      {1366, {0.55f, 0.70f, 0.85f, 1.f, 1.15f, 1.30f, 1.45f, 1.60f, 1.75f}},
      {1440, {0.55f, 0.70f, 0.85f, 1.f, 1.15f, 1.30f, 1.45f, 1.60f, 1.75f}},
      {1600, {0.55f, 0.70f, 0.85f, 1.f, 1.15f, 1.30f, 1.45f, 1.60f, 1.75f}},
      {1920, {0.55f, 0.70f, 0.85f, 1.f, 1.15f, 1.30f, 1.45f, 1.60f, 1.75f}},
      {2160, {0.60f, 0.80f, 1.f, 1.20f, 1.40f, 1.60f, 1.80f, 2.00f, 2.20f}},
      {2560, {0.75f, 1.f, 1.25f, 1.50f, 1.75f, 2.00f, 2.25f, 2.50f, 2.75f}},
      {2880, {0.75f, 1.f, 1.25f, 1.50f, 1.75f, 2.00f, 2.25f, 2.50f, 2.75f}},
      {3200, {1.f, 1.50f, 2.00f, 2.50f, 3.00f, 3.50f, 4.00f, 4.50f, 5.00f}},
      {3840, {1.f, 1.50f, 2.00f, 2.50f, 3.00f, 3.50f, 4.00f, 4.50f, 5.00f}},
      {4096, {1.f, 1.50f, 2.00f, 2.50f, 3.00f, 3.50f, 4.00f, 4.50f, 5.00f}},
      {5120, {1.f, 1.50f, 2.00f, 2.50f, 3.00f, 3.50f, 4.00f, 4.50f, 5.00f}},
      {7680, {1.f, 2.00f, 3.00f, 4.00f, 5.00f, 6.00f, 7.00f, 8.00f, 9.00f}},
      {8192, {1.f, 2.00f, 3.00f, 4.00f, 5.00f, 6.00f, 7.00f, 8.00f, 9.00f}},
  };

  for (const auto& pair : expected_zoom_values) {
    const int size = pair.first;
    ManagedDisplayMode mode(gfx::Size(size, size), 60, false, true, 1.f, 1.f);
    const std::vector<double> zoom_values = GetDisplayZoomFactors(mode);
    EXPECT_EQ(zoom_values.size(), kNumOfZoomFactors);
    for (std::size_t j = 0; j < kNumOfZoomFactors; j++) {
      EXPECT_NEAR(zoom_values[j], pair.second[j], 0.001);

      // Display pref stores the zoom value only upto 2 decimal places. This
      // check ensures that the expected precision is only upto 2 decimal
      // points. Before changing this line please ensure that you have updated
      // chromeos/display/display_prefs.cc
      int percentage_value = std::round(zoom_values[j] * 100.f);
      float fractional_value = static_cast<float>(percentage_value) / 100.f;
      EXPECT_NEAR(zoom_values[j], fractional_value, 0.0001f);
    }

    const int effective_minimum_width_possible = size / zoom_values.back();
    const int effective_maximum_width_possible = size * zoom_values.front();

    const int allowed_minimum_width = std::min(kDefaultMinZoomWidth, size);
    const int allowed_maximum_width = std::max(kDefaultMaxZoomWidth, size);

    EXPECT_GE(effective_minimum_width_possible, allowed_minimum_width);
    EXPECT_LE(effective_maximum_width_possible, allowed_maximum_width);
  }
}

TEST_F(DisplayUtilTest, DisplayZoomsWithInternalDsf) {
  const std::vector<int> sizes = {1280, 1366, 1440, 1600, 1920, 2160, 2560,
                                  2880, 3200, 3840, 4096, 5120, 7680, 8192};

  const std::vector<float> dsfs = {1.25f, 1.5f, 1.6f, 1.8f, 2.f, 2.25f};

  for (float dsf : dsfs) {
    for (int size : sizes) {
      ManagedDisplayMode mode(gfx::Size(size, size), 60, false, true, 1.f, dsf);
      const std::vector<double> zoom_values = GetDisplayZoomFactors(mode);

      const int effective_size = std::round(static_cast<float>(size) / dsf);
      ManagedDisplayMode expected_mode(
          gfx::Size(effective_size, effective_size), 60, false, true, 1.f, 1.f);
      const std::vector<double> expected_zoom_values =
          GetDisplayZoomFactors(expected_mode);
      EXPECT_EQ(zoom_values.size(), kNumOfZoomFactors);
      for (std::size_t i = 0; i < kNumOfZoomFactors; i++)
        EXPECT_NEAR(zoom_values[i], expected_zoom_values[i], 0.001);

      const int effective_minimum_width_possible = size / zoom_values.back();
      const int effective_maximum_width_possible = size * zoom_values.front();

      const int allowed_minimum_width = std::min(kDefaultMinZoomWidth, size);
      const int allowed_maximum_width = std::max(kDefaultMaxZoomWidth, size);

      EXPECT_GE(effective_minimum_width_possible, allowed_minimum_width);
      EXPECT_LE(effective_maximum_width_possible, allowed_maximum_width);
    }
  }
}

}  // namespace test
}  // namespace display
