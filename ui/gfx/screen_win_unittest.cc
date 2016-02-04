// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen_win.h"

#include <cwchar>
#include <string>
#include <vector>

#include <windows.h>
#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/win/dpi.h"

namespace gfx {

namespace {

MONITORINFOEX CreateMonitorInfo(gfx::Rect monitor,
                                gfx::Rect work,
                                std::wstring device_name) {
  MONITORINFOEX monitor_info;
  ::ZeroMemory(&monitor_info, sizeof(monitor_info));
  monitor_info.cbSize = sizeof(monitor_info);
  monitor_info.rcMonitor = monitor.ToRECT();
  monitor_info.rcWork = work.ToRECT();
  size_t device_char_count = ARRAYSIZE(monitor_info.szDevice);
  wcsncpy(monitor_info.szDevice, device_name.c_str(), device_char_count);
  monitor_info.szDevice[device_char_count-1] = L'\0';
  return monitor_info;
}

}  // namespace

class ScreenWinTest : public testing::Test {
 private:
  void SetUp() override {
    testing::Test::SetUp();
    gfx::SetDefaultDeviceScaleFactor(1.0);
  }

  void TearDown() override {
    gfx::SetDefaultDeviceScaleFactor(1.0);
    testing::Test::TearDown();
  }
};

TEST_F(ScreenWinTest, SingleDisplay1x) {
  std::vector<MONITORINFOEX> monitor_infos;
  monitor_infos.push_back(CreateMonitorInfo(gfx::Rect(0, 0, 1920, 1200),
                                            gfx::Rect(0, 0, 1920, 1100),
                                            L"primary"));
  std::vector<gfx::Display> displays =
      ScreenWin::GetDisplaysForMonitorInfos(monitor_infos);

  ASSERT_EQ(1u, displays.size());
  EXPECT_EQ(gfx::Rect(0, 0, 1920, 1200), displays[0].bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 1920, 1100), displays[0].work_area());
}

TEST_F(ScreenWinTest, SingleDisplay2x) {
  gfx::SetDefaultDeviceScaleFactor(2.0);

  std::vector<MONITORINFOEX> monitor_infos;
  monitor_infos.push_back(CreateMonitorInfo(gfx::Rect(0, 0, 1920, 1200),
                                            gfx::Rect(0, 0, 1920, 1100),
                                            L"primary"));
  std::vector<gfx::Display> displays =
      ScreenWin::GetDisplaysForMonitorInfos(monitor_infos);

  ASSERT_EQ(1u, displays.size());
  EXPECT_EQ(gfx::Rect(0, 0, 960, 600), displays[0].bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 960, 550), displays[0].work_area());
}

}  // namespace gfx
