// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_util.h"

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

// Returns true if |a| and |b| are equal within the error limits.
bool WithinEpsilon(float a, float b) {
  return std::abs(a - b) < std::numeric_limits<float>::epsilon();
}

}  // namespace
using DisplayUtilTest = testing::Test;

TEST_F(DisplayUtilTest, DisplayZooms) {
  // A vector of pairs where each pair is the resolution width correspoinding
  // to its list of available display zoom values.
  const std::vector<std::pair<int, std::vector<float>>> expected_zoom_values{
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
      {2400, {0.75f, 1.f, 1.25f, 1.50f, 1.75f, 2.00f, 2.25f, 2.50f, 2.75f}},
      {2560, {0.75f, 1.f, 1.25f, 1.50f, 1.75f, 2.00f, 2.25f, 2.50f, 2.75f}},
      {2880, {0.75f, 1.f, 1.25f, 1.50f, 1.75f, 2.00f, 2.25f, 2.50f, 2.75f}},
      {3200, {1.f, 1.25f, 1.50f, 1.75f, 2.00f, 2.25f, 2.50f, 2.75f, 3.00f}},
      {3840, {1.f, 1.25f, 1.50f, 1.75f, 2.00f, 2.25f, 2.50f, 2.75f, 3.00f}},
      {4096, {1.f, 1.25f, 1.50f, 1.75f, 2.00f, 2.25f, 2.50f, 2.75f, 3.00f}},
      {5120, {1.f, 1.30f, 1.60f, 1.90f, 2.20f, 2.50f, 2.80f, 3.10f, 3.40f}},
      {7680, {1.f, 1.50f, 2.00f, 2.50f, 3.00f, 3.50f, 4.00f, 4.50f, 5.00f}},
      {8192, {1.f, 1.50f, 2.00f, 2.50f, 3.00f, 3.50f, 4.00f, 4.50f, 5.00f}},
  };

  for (const auto& pair : expected_zoom_values) {
    const int size = pair.first;
    ManagedDisplayMode mode(gfx::Size(size, size), 60, false, true, 1.f, 1.f);
    const std::vector<float> zoom_values = GetDisplayZoomFactors(mode);
    EXPECT_FLOAT_EQ(zoom_values.size(), kNumOfZoomFactors);
    for (std::size_t j = 0; j < kNumOfZoomFactors; j++) {
      EXPECT_FLOAT_EQ(zoom_values[j], pair.second[j]);

      // Display pref stores the zoom value as double. This check ensures that
      // the values dont lose precision.
      // Before changing this line please ensure that you have updated
      // chromeos/display/display_prefs.cc
      double double_value = static_cast<double>(zoom_values[j]);
      float float_value = static_cast<float>(double_value);
      EXPECT_FLOAT_EQ(zoom_values[j], float_value);
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
  const std::vector<int> sizes = {1280, 1366, 1440, 1600, 1920,
                                  2160, 2400, 2560, 2880, 3200,
                                  3840, 4096, 5120, 7680, 8192};

  const std::vector<float> dsfs = {1.25f, 1.5f, 1.6f, 1.8f, 2.f, 2.25f};

  for (float dsf : dsfs) {
    for (int size : sizes) {
      ManagedDisplayMode mode(gfx::Size(size, size), 60, false, true, 1.f, dsf);
      const std::vector<float> zoom_values = GetDisplayZoomFactors(mode);

      const int effective_size = std::round(static_cast<float>(size) / dsf);
      ManagedDisplayMode expected_mode(
          gfx::Size(effective_size, effective_size), 60, false, true, 1.f, 1.f);
      const std::vector<float> expected_zoom_values =
          GetDisplayZoomFactors(expected_mode);
      EXPECT_EQ(zoom_values.size(), kNumOfZoomFactors);

      // There should be exactly 1 entry for the inverse dsf in the list.
      const float inverse_dsf = 1.f / dsf;
      const std::size_t count_of_inverse_dsf =
          std::count_if(zoom_values.begin(), zoom_values.end(),
                        [inverse_dsf](float zoom_value) -> bool {
                          return WithinEpsilon(zoom_value, inverse_dsf);
                        });
      EXPECT_EQ(count_of_inverse_dsf, 1UL);

      // Check all values in |zoom_values| are present in |expected_zoom_values|
      // except for |inverse_dsf|.
      auto it = expected_zoom_values.begin();
      for (const auto& zoom_value : zoom_values) {
        // |inverse_dsf| is not expected to be in the expected list since it was
        // added later on.
        if (WithinEpsilon(zoom_value, inverse_dsf))
          continue;

        // Check if |zoom_value| is present in |expected_zoom_values|.
        it = std::find_if(it, expected_zoom_values.end(),
                          [&zoom_value](float expected_zoom) -> bool {
                            return WithinEpsilon(zoom_value, expected_zoom);
                          });
        EXPECT_TRUE(it != expected_zoom_values.end())
            << zoom_value << " not found";
      }

      const int effective_minimum_width_possible = size / zoom_values.back();
      const int effective_maximum_width_possible = size * zoom_values.front();

      const int allowed_minimum_width = std::min(kDefaultMinZoomWidth, size);
      const int allowed_maximum_width = std::max(kDefaultMaxZoomWidth, size);

      EXPECT_GE(effective_minimum_width_possible, allowed_minimum_width);
      EXPECT_LE(effective_maximum_width_possible, allowed_maximum_width);
    }
  }
}

TEST_F(DisplayUtilTest, InsertDsfIntoListLessThanUnity) {
  // list[0] -> actual
  // list[1] -> expected
  std::vector<float> list[2];
  float dsf;

  dsf = 0.6f;
  list[0] = {0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f};
  list[1] = {dsf, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 0.6f;
  list[0] = {0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f, 1.05f};
  list[1] = {dsf, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f, 1.05f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 0.67f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, dsf, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 0.9f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, 0.7f, 0.8f, dsf, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 0.99f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, 0.7f, 0.8f, dsf, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 0.99f;
  list[0] = {0.8f, 1.f, 1.2f, 1.4f, 1.6f, 1.8f, 2.f, 2.2f, 2.4f};
  list[1] = {dsf, 1.f, 1.2f, 1.4f, 1.6f, 1.8f, 2.f, 2.2f, 2.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 0.85f;
  list[0] = {1.f, 1.25f, 1.5f, 1.75f, 2.f, 2.25f, 2.5f, 2.75f, 3.f};
  list[1] = {dsf, 1.f, 1.25f, 1.5f, 1.75f, 2.f, 2.25f, 2.5f, 2.75f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);
}

TEST_F(DisplayUtilTest, InsertDsfIntoListGreaterThanUnity) {
  // list[0] -> actual
  // list[1] -> expected
  std::vector<float> list[2];
  float dsf;

  dsf = 1.f;
  list[0] = {0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f};
  list[1] = {0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 1.1f;
  list[0] = {0.6f, 0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f};
  list[1] = {0.65f, 0.7f, 0.75f, 0.8f, 0.85f, 0.9f, 0.95f, 1.f, dsf};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 1.01f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, dsf, 1.2f, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 1.1f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, dsf, 1.2f, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 1.13f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, dsf, 1.2f, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 1.17f;
  list[0] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, 1.2f, 1.3f, 1.4f};
  list[1] = {0.6f, 0.7f, 0.8f, 0.9f, 1.f, 1.1f, dsf, 1.3f, 1.4f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);

  dsf = 1.1f;
  list[0] = {1.f, 1.25f, 1.5f, 1.75f, 2.f, 2.25f, 2.5f, 2.75f, 3.f};
  list[1] = {1.f, dsf, 1.5f, 1.75f, 2.f, 2.25f, 2.5f, 2.75f, 3.f};
  InsertDsfIntoList(&list[0], dsf);
  EXPECT_EQ(list[1].size(), kNumOfZoomFactors);
  EXPECT_EQ(list[0], list[1]);
}

}  // namespace test
}  // namespace display
