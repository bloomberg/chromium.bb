// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_change_observer.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/math_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_features.h"
#include "ui/display/display_switches.h"
#include "ui/display/fake/fake_display_snapshot.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/types/display_mode.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace display {

namespace {

float ComputeDeviceScaleFactor(float diagonal_inch,
                               const gfx::Size& resolution) {
  // We assume that displays have square pixel.
  float diagonal_pixel = std::sqrt(std::pow(resolution.width(), 2) +
                                   std::pow(resolution.height(), 2));
  float dpi = diagonal_pixel / diagonal_inch;
  return DisplayChangeObserver::FindDeviceScaleFactor(dpi);
}

std::unique_ptr<DisplayMode> MakeDisplayMode(int width,
                                             int height,
                                             bool is_interlaced,
                                             float refresh_rate) {
  return std::make_unique<DisplayMode>(gfx::Size(width, height), is_interlaced,
                                       refresh_rate);
}

}  // namespace

class DisplayChangeObserverTest : public testing::Test,
                                  public testing::WithParamInterface<bool> {
 public:
  DisplayChangeObserverTest() = default;
  ~DisplayChangeObserverTest() override = default;

  // testing::Test:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(features::kListAllDisplayModes);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kListAllDisplayModes);
    }

    Test::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DisplayChangeObserverTest);
};

TEST_P(DisplayChangeObserverTest, GetExternalManagedDisplayModeList) {
  std::unique_ptr<DisplaySnapshot> display_snapshot =
      FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(MakeDisplayMode(1920, 1200, false, 60))
          // Same size as native mode but with higher refresh rate.
          .AddMode(MakeDisplayMode(1920, 1200, false, 75))
          // All non-interlaced (as would be seen with different refresh rates).
          .AddMode(MakeDisplayMode(1920, 1080, false, 80))
          .AddMode(MakeDisplayMode(1920, 1080, false, 70))
          .AddMode(MakeDisplayMode(1920, 1080, false, 60))
          // Interlaced vs non-interlaced.
          .AddMode(MakeDisplayMode(1280, 720, true, 60))
          .AddMode(MakeDisplayMode(1280, 720, false, 60))
          // Interlaced only.
          .AddMode(MakeDisplayMode(1024, 768, true, 70))
          .AddMode(MakeDisplayMode(1024, 768, true, 60))
          // Mixed.
          .AddMode(MakeDisplayMode(1024, 600, true, 60))
          .AddMode(MakeDisplayMode(1024, 600, false, 70))
          .AddMode(MakeDisplayMode(1024, 600, false, 60))
          // Just one interlaced mode.
          .AddMode(MakeDisplayMode(640, 480, true, 60))
          .Build();

  ManagedDisplayInfo::ManagedDisplayModeList display_modes =
      DisplayChangeObserver::GetExternalManagedDisplayModeList(
          *display_snapshot);

  const bool listing_all_modes = GetParam();
  if (listing_all_modes) {
    ASSERT_EQ(13u, display_modes.size());
    EXPECT_EQ("640x480", display_modes[0].size().ToString());
    EXPECT_TRUE(display_modes[0].is_interlaced());
    EXPECT_EQ(display_modes[0].refresh_rate(), 60);

    EXPECT_EQ("1024x600", display_modes[1].size().ToString());
    EXPECT_FALSE(display_modes[1].is_interlaced());
    EXPECT_EQ(display_modes[1].refresh_rate(), 60);
    EXPECT_EQ("1024x600", display_modes[2].size().ToString());
    EXPECT_TRUE(display_modes[2].is_interlaced());
    EXPECT_EQ(display_modes[2].refresh_rate(), 60);
    EXPECT_EQ("1024x600", display_modes[3].size().ToString());
    EXPECT_FALSE(display_modes[3].is_interlaced());
    EXPECT_EQ(display_modes[3].refresh_rate(), 70);

    EXPECT_EQ("1024x768", display_modes[4].size().ToString());
    EXPECT_TRUE(display_modes[4].is_interlaced());
    EXPECT_EQ(display_modes[4].refresh_rate(), 60);
    EXPECT_EQ("1024x768", display_modes[5].size().ToString());
    EXPECT_TRUE(display_modes[5].is_interlaced());
    EXPECT_EQ(display_modes[5].refresh_rate(), 70);

    EXPECT_EQ("1280x720", display_modes[6].size().ToString());
    EXPECT_FALSE(display_modes[6].is_interlaced());
    EXPECT_EQ(display_modes[6].refresh_rate(), 60);
    EXPECT_EQ("1280x720", display_modes[7].size().ToString());
    EXPECT_TRUE(display_modes[7].is_interlaced());
    EXPECT_EQ(display_modes[7].refresh_rate(), 60);

    EXPECT_EQ("1920x1080", display_modes[8].size().ToString());
    EXPECT_FALSE(display_modes[8].is_interlaced());
    EXPECT_EQ(display_modes[8].refresh_rate(), 60);
    EXPECT_EQ("1920x1080", display_modes[9].size().ToString());
    EXPECT_FALSE(display_modes[9].is_interlaced());
    EXPECT_EQ(display_modes[9].refresh_rate(), 70);
    EXPECT_EQ("1920x1080", display_modes[10].size().ToString());
    EXPECT_FALSE(display_modes[10].is_interlaced());
    EXPECT_EQ(display_modes[10].refresh_rate(), 80);

    EXPECT_EQ("1920x1200", display_modes[11].size().ToString());
    EXPECT_FALSE(display_modes[11].is_interlaced());
    EXPECT_EQ(display_modes[11].refresh_rate(), 60);

    EXPECT_EQ("1920x1200", display_modes[12].size().ToString());
    EXPECT_FALSE(display_modes[12].is_interlaced());
    EXPECT_EQ(display_modes[12].refresh_rate(), 75);
  } else {
    ASSERT_EQ(6u, display_modes.size());
    EXPECT_EQ("640x480", display_modes[0].size().ToString());
    EXPECT_TRUE(display_modes[0].is_interlaced());
    EXPECT_EQ(display_modes[0].refresh_rate(), 60);

    EXPECT_EQ("1024x600", display_modes[1].size().ToString());
    EXPECT_FALSE(display_modes[1].is_interlaced());
    EXPECT_EQ(display_modes[1].refresh_rate(), 70);

    EXPECT_EQ("1024x768", display_modes[2].size().ToString());
    EXPECT_TRUE(display_modes[2].is_interlaced());
    EXPECT_EQ(display_modes[2].refresh_rate(), 70);

    EXPECT_EQ("1280x720", display_modes[3].size().ToString());
    EXPECT_FALSE(display_modes[3].is_interlaced());
    EXPECT_EQ(display_modes[3].refresh_rate(), 60);

    EXPECT_EQ("1920x1080", display_modes[4].size().ToString());
    EXPECT_FALSE(display_modes[4].is_interlaced());
    EXPECT_EQ(display_modes[4].refresh_rate(), 80);

    EXPECT_EQ("1920x1200", display_modes[5].size().ToString());
    EXPECT_FALSE(display_modes[5].is_interlaced());
    EXPECT_EQ(display_modes[5].refresh_rate(), 60);
  }
}

TEST_P(DisplayChangeObserverTest, GetEmptyExternalManagedDisplayModeList) {
  FakeDisplaySnapshot display_snapshot(
      123, gfx::Point(), gfx::Size(), DISPLAY_CONNECTION_TYPE_UNKNOWN, false,
      false, PrivacyScreenState::kNotSupported, false, false, std::string(), {},
      nullptr, nullptr, 0, gfx::Size());

  ManagedDisplayInfo::ManagedDisplayModeList display_modes =
      DisplayChangeObserver::GetExternalManagedDisplayModeList(
          display_snapshot);
  EXPECT_EQ(0u, display_modes.size());
}

TEST_P(DisplayChangeObserverTest, FindDeviceScaleFactor) {
  // sanity check
  EXPECT_EQ(1.25f, DisplayChangeObserver::FindDeviceScaleFactor(150));
  EXPECT_EQ(1.6f, DisplayChangeObserver::FindDeviceScaleFactor(180));
  EXPECT_EQ(kDsf_1_777, DisplayChangeObserver::FindDeviceScaleFactor(220));
  EXPECT_EQ(2.f, DisplayChangeObserver::FindDeviceScaleFactor(230));
  EXPECT_EQ(kDsf_2_252, DisplayChangeObserver::FindDeviceScaleFactor(270));
  EXPECT_EQ(kDsf_2_666, DisplayChangeObserver::FindDeviceScaleFactor(300));
  constexpr struct Data {
    const float diagonal_size;
    const gfx::Size resolution;
    const float expected_dsf;
    const gfx::Size expected_dp_size;
    const bool screenshot_size_error;
  } display_configs[] = {
      // clang-format off
      // inch,  resolution,  DSF,        size in DP,   screenshot size error
      {19.5,   {1600, 900},  1.f,        {1600, 900},  false},
      {21.5f,  {1920, 1080}, 1.f,        {1920, 1080}, false},
      {10.0f,  {1920, 1200}, kDsf_1_777, {1080, 675},  false},
      {12.1f,  {1280, 800},  1.0f,       {1280, 800},  false},
      {13.3f,  {1920, 1080}, 1.25f,      {1536, 864},  false},
      {14.0f,  {1920, 1080}, 1.25f,      {1536, 864},  false},
      {11.6f,  {1920, 1080}, 1.6f,       {1200, 675},  false},
      {12.02f, {2160, 1440}, 1.6f,       {1350, 900},  false},
      {9.7f,   {1536, 2048}, 2.0f,       {768, 1024},  false},
      {12.85f, {2560, 1700}, 2.0f,       {1280, 850},  false},
      {12.3f,  {2400, 1600}, 2.0f,       {1200, 800},  false},
      {10.1f,  {1920, 1200}, kDsf_1_777, {1080, 675},  false},
      {11.0f,  {2160, 1440}, 2.f,        {1080, 720},  false},
      {12.3f,  {3000, 2000}, kDsf_2_252, {1332, 888},  true},
      {13.1f,  {3840, 2160}, kDsf_2_666, {1440, 810},  false},
      // clang-format on
  };

  for (auto& entry : display_configs) {
    SCOPED_TRACE(base::StringPrintf(
        "%dx%d, diag=%1.3f inch, expected=%1.10f", entry.resolution.width(),
        entry.resolution.height(), entry.diagonal_size, entry.expected_dsf));
    // Check ScaleFactor.
    float scale_factor =
        ComputeDeviceScaleFactor(entry.diagonal_size, entry.resolution);
    EXPECT_EQ(entry.expected_dsf, scale_factor);

    // Check DP size.
    const gfx::Size dp_size =
        gfx::ScaleToCeiledSize(entry.resolution, 1.f / scale_factor);

    // Check Screenshot size.
    EXPECT_EQ(entry.expected_dp_size, dp_size);
    gfx::Transform transform;
    transform.Scale(scale_factor, scale_factor);
    const gfx::Size screenshot_size =
        cc::MathUtil::MapEnclosingClippedRect(transform, gfx::Rect(dp_size))
            .size();
    if (entry.screenshot_size_error) {
      EXPECT_NE(entry.resolution, screenshot_size);
      constexpr float kEpsilon = 0.001f;
      EXPECT_EQ(entry.resolution,
                cc::MathUtil::MapEnclosingClippedRectIgnoringError(
                    transform, gfx::Rect(dp_size), kEpsilon)
                    .size());
    } else {
      EXPECT_EQ(entry.resolution, screenshot_size);
    }
  }

  float max_scale_factor = kDsf_2_666;
  // Erroneous values should still work.
  EXPECT_EQ(1.0f, DisplayChangeObserver::FindDeviceScaleFactor(-100.0f));
  EXPECT_EQ(1.0f, DisplayChangeObserver::FindDeviceScaleFactor(0.0f));
  EXPECT_EQ(max_scale_factor,
            DisplayChangeObserver::FindDeviceScaleFactor(10000.0f));
}

TEST_P(DisplayChangeObserverTest,
       FindExternalDisplayNativeModeWhenOverwritten) {
  std::unique_ptr<DisplaySnapshot> display_snapshot =
      FakeDisplaySnapshot::Builder()
          .SetId(123)
          .SetNativeMode(MakeDisplayMode(1920, 1080, true, 60))
          .AddMode(MakeDisplayMode(1920, 1080, false, 60))
          .Build();

  ManagedDisplayInfo::ManagedDisplayModeList display_modes =
      DisplayChangeObserver::GetExternalManagedDisplayModeList(
          *display_snapshot);

  const bool listing_all_modes = GetParam();

  if (listing_all_modes) {
    ASSERT_EQ(2u, display_modes.size());
    EXPECT_EQ("1920x1080", display_modes[0].size().ToString());
    EXPECT_FALSE(display_modes[0].is_interlaced());
    EXPECT_FALSE(display_modes[0].native());
    EXPECT_EQ(display_modes[0].refresh_rate(), 60);

    EXPECT_EQ("1920x1080", display_modes[1].size().ToString());
    EXPECT_TRUE(display_modes[1].is_interlaced());
    EXPECT_TRUE(display_modes[1].native());
    EXPECT_EQ(display_modes[1].refresh_rate(), 60);
  } else {
    // Only the native mode will be listed.
    ASSERT_EQ(1u, display_modes.size());
    EXPECT_EQ("1920x1080", display_modes[0].size().ToString());
    EXPECT_TRUE(display_modes[0].is_interlaced());
    EXPECT_TRUE(display_modes[0].native());
    EXPECT_EQ(display_modes[0].refresh_rate(), 60);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         DisplayChangeObserverTest,
                         ::testing::Values(false, true));

}  // namespace display
