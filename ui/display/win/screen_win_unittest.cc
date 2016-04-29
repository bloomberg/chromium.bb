// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/screen_win.h"

#include <windows.h>
#include <inttypes.h>
#include <stddef.h>

#include <cwchar>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/win/display_info.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/screen_win_display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/switches.h"
#include "ui/gfx/test/display_util.h"

namespace display {
namespace win {
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

class TestScreenWin : public ScreenWin {
 public:
  TestScreenWin(const std::vector<DisplayInfo>& display_infos,
                const std::vector<MONITORINFOEX>& monitor_infos,
                const std::unordered_map<HWND, gfx::Rect>& hwnd_map)
      : monitor_infos_(monitor_infos),
        hwnd_map_(hwnd_map) {
    UpdateFromDisplayInfos(display_infos);
  }

  ~TestScreenWin() override = default;

 protected:
  // display::win::ScreenWin:
  HWND GetHWNDFromNativeView(gfx::NativeView window) const override {
    // NativeView is only used as an identifier in this tests, so interchange
    // NativeView with an HWND for convenience.
    return reinterpret_cast<HWND>(window);
  }

  gfx::NativeWindow GetNativeWindowFromHWND(HWND hwnd) const override {
    // NativeWindow is only used as an identifier in this tests, so interchange
    // an HWND for a NativeWindow for convenience.
    return reinterpret_cast<gfx::NativeWindow>(hwnd);
  }

 private:
  void Initialize() override {}

  // Finding the corresponding monitor from a point is generally handled by
  // Windows's MonitorFromPoint. This mocked function requires that the provided
  // point is contained entirely in the monitor.
  MONITORINFOEX MonitorInfoFromScreenPoint(const gfx::Point& screen_point) const
      override {
    for (const MONITORINFOEX& monitor_info : monitor_infos_) {
      if (gfx::Rect(monitor_info.rcMonitor).Contains(screen_point))
        return monitor_info;
    }
    NOTREACHED();
    return monitor_infos_[0];
  }

  // Finding the corresponding monitor from a rect is generally handled by
  // Windows's MonitorFromRect. This mocked function requires that the provided
  // rectangle overlap at least part of the monitor.
  MONITORINFOEX MonitorInfoFromScreenRect(const gfx::Rect& screen_rect) const
      override {
    MONITORINFOEX candidate = monitor_infos_[0];
    int largest_area = 0;
    for (const MONITORINFOEX& monitor_info : monitor_infos_) {
      gfx::Rect bounds(monitor_info.rcMonitor);
      if (bounds.Intersects(screen_rect)) {
        bounds.Intersect(screen_rect);
        int area = bounds.height() * bounds.width();
        if (largest_area < area) {
          candidate = monitor_info;
          largest_area = area;
        }
      }
    }
    EXPECT_NE(largest_area, 0);
    return candidate;
  }

  // Finding the corresponding monitor from an HWND is generally handled by
  // Windows's MonitorFromWindow. Because we're mocking MonitorFromWindow here,
  // it's important that the HWND fully reside in the bounds of the display,
  // otherwise this could cause MonitorInfoFromScreenRect or
  // MonitorInfoFromScreenPoint to fail to find the monitor based off of a rect
  // or point within the HWND.
  MONITORINFOEX MonitorInfoFromWindow(HWND hwnd, DWORD default_options)
      const override {
    auto search = hwnd_map_.find(hwnd);
    if (search != hwnd_map_.end())
      return MonitorInfoFromScreenRect(search->second);

    EXPECT_EQ(default_options, static_cast<DWORD>(MONITOR_DEFAULTTOPRIMARY));
    for (const auto& monitor_info : monitor_infos_) {
      if (monitor_info.rcMonitor.left == 0 &&
          monitor_info.rcMonitor.top == 0) {
        return monitor_info;
      }
    }
    NOTREACHED();
    return monitor_infos_[0];
  }

  HWND GetRootWindow(HWND hwnd) const override {
    return hwnd;
  }

  std::vector<MONITORINFOEX> monitor_infos_;
  std::unordered_map<HWND, gfx::Rect> hwnd_map_;

  DISALLOW_COPY_AND_ASSIGN(TestScreenWin);
};

Screen* GetScreen() {
  return Screen::GetScreen();
}

// Allows tests to specify the screen and associated state.
class TestScreenWinInitializer {
 public:
  virtual void AddMonitor(const gfx::Rect& pixel_bounds,
                          const gfx::Rect& pixel_work,
                          const wchar_t* device_name,
                          float device_scale_factor) = 0;

  virtual HWND CreateFakeHwnd(const gfx::Rect& bounds) = 0;
};

class TestScreenWinManager : public TestScreenWinInitializer {
 public:
  TestScreenWinManager() = default;

  ~TestScreenWinManager() { Screen::SetScreenInstance(nullptr); }

  void AddMonitor(const gfx::Rect& pixel_bounds,
                  const gfx::Rect& pixel_work,
                  const wchar_t* device_name,
                  float device_scale_factor) override {
    MONITORINFOEX monitor_info = CreateMonitorInfo(pixel_bounds,
                                                   pixel_work,
                                                   device_name);
    monitor_infos_.push_back(monitor_info);
    display_infos_.push_back(DisplayInfo(monitor_info, device_scale_factor,
                                         Display::ROTATE_0));
  }

  HWND CreateFakeHwnd(const gfx::Rect& bounds) override {
    EXPECT_EQ(screen_win_, nullptr);
    hwnd_map_.insert(std::pair<HWND, gfx::Rect>(++hwndLast_, bounds));
    return hwndLast_;
  }

  void InitializeScreenWin() {
    ASSERT_EQ(screen_win_, nullptr);
    screen_win_.reset(new TestScreenWin(display_infos_,
                                        monitor_infos_,
                                        hwnd_map_));
    Screen::SetScreenInstance(screen_win_.get());
  }

  ScreenWin* GetScreenWin() {
    return screen_win_.get();
  }

 private:
  HWND hwndLast_ = nullptr;
  std::unique_ptr<ScreenWin> screen_win_;
  std::vector<MONITORINFOEX> monitor_infos_;
  std::vector<DisplayInfo> display_infos_;
  std::unordered_map<HWND, gfx::Rect> hwnd_map_;

  DISALLOW_COPY_AND_ASSIGN(TestScreenWinManager);
};

class ScreenWinTest : public testing::Test {
 protected:
  ScreenWinTest() = default;

  void SetUp() override {
    testing::Test::SetUp();
    SetDefaultDeviceScaleFactor(1.0);
    screen_win_initializer_.reset(new TestScreenWinManager());
    SetUpScreen(screen_win_initializer_.get());
    screen_win_initializer_->InitializeScreenWin();
  }

  void TearDown() override {
    screen_win_initializer_.reset();
    SetDefaultDeviceScaleFactor(1.0);
    testing::Test::TearDown();
  }

  virtual void SetUpScreen(TestScreenWinInitializer* initializer) = 0;

  gfx::NativeWindow GetNativeWindowFromHWND(HWND hwnd) const {
    ScreenWin* screen_win = screen_win_initializer_->GetScreenWin();
    return screen_win->GetNativeWindowFromHWND(hwnd);;
  }

 private:
  std::unique_ptr<TestScreenWinManager> screen_win_initializer_;

  DISALLOW_COPY_AND_ASSIGN(ScreenWinTest);
};

// Single Display of 1.0 Device Scale Factor.
class ScreenWinTestSingleDisplay1x : public ScreenWinTest {
 public:
  ScreenWinTestSingleDisplay1x() = default;

  void SetUpScreen(TestScreenWinInitializer* initializer) override {
    initializer->AddMonitor(gfx::Rect(0, 0, 1920, 1200),
                            gfx::Rect(0, 0, 1920, 1100),
                            L"primary",
                            1.0);
    fake_hwnd_ = initializer->CreateFakeHwnd(gfx::Rect(0, 0, 1920, 1100));
  }

  HWND GetFakeHwnd() {
    return fake_hwnd_;
  }

 private:
  HWND fake_hwnd_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScreenWinTestSingleDisplay1x);
};

}  // namespace

TEST_F(ScreenWinTestSingleDisplay1x, ScreenToDIPPoints) {
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::ScreenToDIPPoint(origin));
  EXPECT_EQ(middle, ScreenWin::ScreenToDIPPoint(middle));
  EXPECT_EQ(lower_right, ScreenWin::ScreenToDIPPoint(lower_right));
}

TEST_F(ScreenWinTestSingleDisplay1x, DIPToScreenPoints) {
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::DIPToScreenPoint(origin));
  EXPECT_EQ(middle, ScreenWin::DIPToScreenPoint(middle));
  EXPECT_EQ(lower_right, ScreenWin::DIPToScreenPoint(lower_right));
}

TEST_F(ScreenWinTestSingleDisplay1x, ClientToDIPPoints) {
  HWND hwnd = GetFakeHwnd();
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::ClientToDIPPoint(hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::ClientToDIPPoint(hwnd, middle));
  EXPECT_EQ(lower_right, ScreenWin::ClientToDIPPoint(hwnd, lower_right));
}

TEST_F(ScreenWinTestSingleDisplay1x, DIPToClientPoints) {
  HWND hwnd = GetFakeHwnd();
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::DIPToClientPoint(hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::DIPToClientPoint(hwnd, middle));
  EXPECT_EQ(lower_right, ScreenWin::DIPToClientPoint(hwnd, lower_right));
}

TEST_F(ScreenWinTestSingleDisplay1x, ScreenToDIPRects) {
  HWND hwnd = GetFakeHwnd();
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::ScreenToDIPRect(hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::ScreenToDIPRect(hwnd, middle));
}

TEST_F(ScreenWinTestSingleDisplay1x, DIPToScreenRects) {
  HWND hwnd = GetFakeHwnd();
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::DIPToScreenRect(hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::DIPToScreenRect(hwnd, middle));
}

TEST_F(ScreenWinTestSingleDisplay1x, ClientToDIPRects) {
  HWND hwnd = GetFakeHwnd();
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::ClientToDIPRect(hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::ClientToDIPRect(hwnd, middle));
}

TEST_F(ScreenWinTestSingleDisplay1x, DIPToClientRects) {
  HWND hwnd = GetFakeHwnd();
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::DIPToClientRect(hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::DIPToClientRect(hwnd, middle));
}

TEST_F(ScreenWinTestSingleDisplay1x, ScreenToDIPSize) {
  HWND hwnd = GetFakeHwnd();
  gfx::Size size(42, 131);
  EXPECT_EQ(size, ScreenWin::ScreenToDIPSize(hwnd, size));
}

TEST_F(ScreenWinTestSingleDisplay1x, DIPToScreenSize) {
  HWND hwnd = GetFakeHwnd();
  gfx::Size size(42, 131);
  EXPECT_EQ(size, ScreenWin::DIPToScreenSize(hwnd, size));
}

TEST_F(ScreenWinTestSingleDisplay1x, GetDisplays) {
  std::vector<Display> displays = GetScreen()->GetAllDisplays();
  ASSERT_EQ(1u, displays.size());
  EXPECT_EQ(gfx::Rect(0, 0, 1920, 1200), displays[0].bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 1920, 1100), displays[0].work_area());
}

TEST_F(ScreenWinTestSingleDisplay1x, GetNumDisplays) {
  EXPECT_EQ(1, GetScreen()->GetNumDisplays());
}

TEST_F(ScreenWinTestSingleDisplay1x, GetDisplayNearestWindowPrimaryDisplay) {
  Screen* screen = GetScreen();
  EXPECT_EQ(screen->GetPrimaryDisplay(),
            screen->GetDisplayNearestWindow(nullptr));
}

TEST_F(ScreenWinTestSingleDisplay1x, GetDisplayNearestWindow) {
  Screen* screen = GetScreen();
  gfx::NativeWindow native_window = GetNativeWindowFromHWND(GetFakeHwnd());
  EXPECT_EQ(screen->GetAllDisplays()[0],
            screen->GetDisplayNearestWindow(native_window));
}

TEST_F(ScreenWinTestSingleDisplay1x, GetDisplayNearestPoint) {
  Screen* screen = GetScreen();
  Display display = screen->GetAllDisplays()[0];
  EXPECT_EQ(display, screen->GetDisplayNearestPoint(gfx::Point(0, 0)));
  EXPECT_EQ(display, screen->GetDisplayNearestPoint(gfx::Point(250, 952)));
  EXPECT_EQ(display, screen->GetDisplayNearestPoint(gfx::Point(1919, 1199)));
}

TEST_F(ScreenWinTestSingleDisplay1x, GetDisplayMatching) {
  Screen* screen = GetScreen();
  Display display = screen->GetAllDisplays()[0];
  EXPECT_EQ(display, screen->GetDisplayMatching(gfx::Rect(0, 0, 100, 100)));
  EXPECT_EQ(display,
            screen->GetDisplayMatching(gfx::Rect(1819, 1099, 100, 100)));
}

TEST_F(ScreenWinTestSingleDisplay1x, GetPrimaryDisplay) {
  Screen* screen = GetScreen();
  EXPECT_EQ(gfx::Point(0, 0), screen->GetPrimaryDisplay().bounds().origin());
}

namespace {

// Single Display of 1.25 Device Scale Factor.
class ScreenWinTestSingleDisplay1_25x : public ScreenWinTest {
 public:
  ScreenWinTestSingleDisplay1_25x() = default;

  void SetUpScreen(TestScreenWinInitializer* initializer) override {
    SetDefaultDeviceScaleFactor(1.25);
    // Add Monitor of Scale Factor 1.0 since display::GetDPIScale performs the
    // clamping and not ScreenWin.
    initializer->AddMonitor(gfx::Rect(0, 0, 1920, 1200),
                            gfx::Rect(0, 0, 1920, 1100),
                            L"primary",
                            1.0);
    fake_hwnd_ = initializer->CreateFakeHwnd(gfx::Rect(0, 0, 1920, 1100));
  }

  HWND GetFakeHwnd() {
    return fake_hwnd_;
  }

 private:
  HWND fake_hwnd_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScreenWinTestSingleDisplay1_25x);
};

}  // namespace

TEST_F(ScreenWinTestSingleDisplay1_25x, ScreenToDIPPoints) {
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::ScreenToDIPPoint(origin));
  EXPECT_EQ(middle, ScreenWin::ScreenToDIPPoint(middle));
  EXPECT_EQ(lower_right, ScreenWin::ScreenToDIPPoint(lower_right));
}

TEST_F(ScreenWinTestSingleDisplay1_25x, DIPToScreenPoints) {
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::DIPToScreenPoint(origin));
  EXPECT_EQ(middle, ScreenWin::DIPToScreenPoint(middle));
  EXPECT_EQ(lower_right, ScreenWin::DIPToScreenPoint(lower_right));
}

TEST_F(ScreenWinTestSingleDisplay1_25x, ClientToDIPPoints) {
  HWND hwnd = GetFakeHwnd();
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::ClientToDIPPoint(hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::ClientToDIPPoint(hwnd, middle));
  EXPECT_EQ(lower_right, ScreenWin::ClientToDIPPoint(hwnd, lower_right));
}

TEST_F(ScreenWinTestSingleDisplay1_25x, DIPToClientPoints) {
  HWND hwnd = GetFakeHwnd();
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::DIPToClientPoint(hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::DIPToClientPoint(hwnd, middle));
  EXPECT_EQ(lower_right, ScreenWin::DIPToClientPoint(hwnd, lower_right));
}

TEST_F(ScreenWinTestSingleDisplay1_25x, ScreenToDIPRects) {
  HWND hwnd = GetFakeHwnd();
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::ScreenToDIPRect(hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::ScreenToDIPRect(hwnd, middle));
}

TEST_F(ScreenWinTestSingleDisplay1_25x, DIPToScreenRects) {
  HWND hwnd = GetFakeHwnd();
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::DIPToScreenRect(hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::DIPToScreenRect(hwnd, middle));
}

TEST_F(ScreenWinTestSingleDisplay1_25x, ClientToDIPRects) {
  HWND hwnd = GetFakeHwnd();
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::ClientToDIPRect(hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::ClientToDIPRect(hwnd, middle));
}

TEST_F(ScreenWinTestSingleDisplay1_25x, DIPToClientRects) {
  HWND hwnd = GetFakeHwnd();
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::DIPToClientRect(hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::DIPToClientRect(hwnd, middle));
}

TEST_F(ScreenWinTestSingleDisplay1_25x, ScreenToDIPSize) {
  HWND hwnd = GetFakeHwnd();
  gfx::Size size(42, 131);
  EXPECT_EQ(size, ScreenWin::ScreenToDIPSize(hwnd, size));
}

TEST_F(ScreenWinTestSingleDisplay1_25x, DIPToScreenSize) {
  HWND hwnd = GetFakeHwnd();
  gfx::Size size(42, 131);
  EXPECT_EQ(size, ScreenWin::DIPToScreenSize(hwnd, size));
}

TEST_F(ScreenWinTestSingleDisplay1_25x, GetDisplays) {
  std::vector<Display> displays = GetScreen()->GetAllDisplays();
  ASSERT_EQ(1u, displays.size());
  // On Windows, scale factors of 1.25 or lower are clamped to 1.0.
  EXPECT_EQ(gfx::Rect(0, 0, 1920, 1200), displays[0].bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 1920, 1100), displays[0].work_area());
}

TEST_F(ScreenWinTestSingleDisplay1_25x, GetDisplayNearestWindow) {
  Screen* screen = GetScreen();
  gfx::NativeWindow native_window = GetNativeWindowFromHWND(GetFakeHwnd());
  EXPECT_EQ(screen->GetAllDisplays()[0],
            screen->GetDisplayNearestWindow(native_window));
}

TEST_F(ScreenWinTestSingleDisplay1_25x, GetDisplayNearestPoint) {
  Screen* screen = GetScreen();
  Display display = screen->GetAllDisplays()[0];
  EXPECT_EQ(display, screen->GetDisplayNearestPoint(gfx::Point(0, 0)));
  EXPECT_EQ(display, screen->GetDisplayNearestPoint(gfx::Point(250, 952)));
  EXPECT_EQ(display, screen->GetDisplayNearestPoint(gfx::Point(1919, 1199)));
}

TEST_F(ScreenWinTestSingleDisplay1_25x, GetDisplayMatching) {
  Screen* screen = GetScreen();
  Display display = screen->GetAllDisplays()[0];
  EXPECT_EQ(display, screen->GetDisplayMatching(gfx::Rect(0, 0, 100, 100)));
  EXPECT_EQ(display,
            screen->GetDisplayMatching(gfx::Rect(1819, 1099, 100, 100)));
}
TEST_F(ScreenWinTestSingleDisplay1_25x, GetPrimaryDisplay) {
  Screen* screen = GetScreen();
  EXPECT_EQ(gfx::Point(0, 0), screen->GetPrimaryDisplay().bounds().origin());
}

namespace {

// Single Display of 1.25 Device Scale Factor.
class ScreenWinTestSingleDisplay1_5x : public ScreenWinTest {
 public:
  ScreenWinTestSingleDisplay1_5x() = default;

  void SetUpScreen(TestScreenWinInitializer* initializer) override {
    SetDefaultDeviceScaleFactor(1.5);
    initializer->AddMonitor(gfx::Rect(0, 0, 1920, 1200),
                            gfx::Rect(0, 0, 1920, 1100),
                            L"primary",
                            1.5);
    fake_hwnd_ = initializer->CreateFakeHwnd(gfx::Rect(0, 0, 1920, 1100));
  }

  HWND GetFakeHwnd() {
    return fake_hwnd_;
  }

 private:
  HWND fake_hwnd_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScreenWinTestSingleDisplay1_5x);
};

}  // namespace

TEST_F(ScreenWinTestSingleDisplay1_5x, ScreenToDIPPoints) {
  EXPECT_EQ(gfx::Point(0, 0), ScreenWin::ScreenToDIPPoint(gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(243, 462),
            ScreenWin::ScreenToDIPPoint(gfx::Point(365, 694)));
  EXPECT_EQ(gfx::Point(1279, 799),
            ScreenWin::ScreenToDIPPoint(gfx::Point(1919, 1199)));
}

TEST_F(ScreenWinTestSingleDisplay1_5x, DIPToScreenPoints) {
  EXPECT_EQ(gfx::Point(0, 0), ScreenWin::DIPToScreenPoint(gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(364, 693),
            ScreenWin::DIPToScreenPoint(gfx::Point(243, 462)));
  EXPECT_EQ(gfx::Point(1918, 1198),
            ScreenWin::DIPToScreenPoint(gfx::Point(1279, 799)));
}

TEST_F(ScreenWinTestSingleDisplay1_5x, ClientToDIPPoints) {
  HWND hwnd = GetFakeHwnd();
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::ClientToDIPPoint(hwnd, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(243, 462),
            ScreenWin::ClientToDIPPoint(hwnd, gfx::Point(365, 694)));
  EXPECT_EQ(gfx::Point(1279, 799),
            ScreenWin::ClientToDIPPoint(hwnd, gfx::Point(1919, 1199)));
}

TEST_F(ScreenWinTestSingleDisplay1_5x, DIPToClientPoints) {
  HWND hwnd = GetFakeHwnd();
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::DIPToClientPoint(hwnd, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(364, 693),
            ScreenWin::DIPToClientPoint(hwnd, gfx::Point(243, 462)));
  EXPECT_EQ(gfx::Point(1918, 1198),
            ScreenWin::DIPToClientPoint(hwnd, gfx::Point(1279, 799)));
}

TEST_F(ScreenWinTestSingleDisplay1_5x, ScreenToDIPRects) {
  HWND hwnd = GetFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 34, 67),
            ScreenWin::ScreenToDIPRect(hwnd, gfx::Rect(0, 0, 50, 100)));
  EXPECT_EQ(gfx::Rect(168, 330, 28, 35),
            ScreenWin::ScreenToDIPRect(hwnd, gfx::Rect(253, 496, 41, 52)));
}

TEST_F(ScreenWinTestSingleDisplay1_5x, DIPToScreenRects) {
  HWND hwnd = GetFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 51, 101),
            ScreenWin::DIPToScreenRect(hwnd, gfx::Rect(0, 0, 34, 67)));
  EXPECT_EQ(gfx::Rect(252, 495, 42, 54),
            ScreenWin::DIPToScreenRect(hwnd, gfx::Rect(168, 330, 28, 36)));
}

TEST_F(ScreenWinTestSingleDisplay1_5x, ClientToDIPRects) {
  HWND hwnd = GetFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 34, 67),
            ScreenWin::ClientToDIPRect(hwnd, gfx::Rect(0, 0, 50, 100)));
  EXPECT_EQ(gfx::Rect(168, 330, 28, 35),
            ScreenWin::ClientToDIPRect(hwnd, gfx::Rect(253, 496, 41, 52)));
}

TEST_F(ScreenWinTestSingleDisplay1_5x, DIPToClientRects) {
  HWND hwnd = GetFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 51, 101),
            ScreenWin::DIPToClientRect(hwnd, gfx::Rect(0, 0, 34, 67)));
  EXPECT_EQ(gfx::Rect(252, 495, 42, 54),
            ScreenWin::DIPToClientRect(hwnd, gfx::Rect(168, 330, 28, 36)));
}

TEST_F(ScreenWinTestSingleDisplay1_5x, ScreenToDIPSize) {
  EXPECT_EQ(gfx::Size(28, 88),
            ScreenWin::ScreenToDIPSize(GetFakeHwnd(), gfx::Size(42, 131)));
}

TEST_F(ScreenWinTestSingleDisplay1_5x, DIPToScreenSize) {
  EXPECT_EQ(gfx::Size(42, 132),
            ScreenWin::DIPToScreenSize(GetFakeHwnd(), gfx::Size(28, 88)));
}

TEST_F(ScreenWinTestSingleDisplay1_5x, GetDisplays) {
  std::vector<Display> displays = GetScreen()->GetAllDisplays();
  ASSERT_EQ(1u, displays.size());
  EXPECT_EQ(gfx::Rect(0, 0, 1280, 800), displays[0].bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 1280, 734), displays[0].work_area());
}

TEST_F(ScreenWinTestSingleDisplay1_5x, GetDisplayNearestWindow) {
  Screen* screen = GetScreen();
  gfx::NativeWindow native_window = GetNativeWindowFromHWND(GetFakeHwnd());
  EXPECT_EQ(screen->GetAllDisplays()[0],
            screen->GetDisplayNearestWindow(native_window));
}

TEST_F(ScreenWinTestSingleDisplay1_5x, GetDisplayNearestPoint) {
  Screen* screen = GetScreen();
  Display display = screen->GetAllDisplays()[0];
  EXPECT_EQ(display, screen->GetDisplayNearestPoint(gfx::Point(0, 0)));
  EXPECT_EQ(display, screen->GetDisplayNearestPoint(gfx::Point(250, 524)));
  EXPECT_EQ(display, screen->GetDisplayNearestPoint(gfx::Point(1279, 733)));
}

TEST_F(ScreenWinTestSingleDisplay1_5x, GetDisplayMatching) {
  Screen* screen = GetScreen();
  Display display = screen->GetAllDisplays()[0];
  EXPECT_EQ(display, screen->GetDisplayMatching(gfx::Rect(0, 0, 100, 100)));
  EXPECT_EQ(display,
            screen->GetDisplayMatching(gfx::Rect(1819, 1099, 100, 100)));
}
TEST_F(ScreenWinTestSingleDisplay1_5x, GetPrimaryDisplay) {
  Screen* screen = GetScreen();
  EXPECT_EQ(gfx::Point(0, 0), screen->GetPrimaryDisplay().bounds().origin());
}

namespace {

// Single Display of 2.0 Device Scale Factor.
class ScreenWinTestSingleDisplay2x : public ScreenWinTest {
 public:
  ScreenWinTestSingleDisplay2x() = default;

  void SetUpScreen(TestScreenWinInitializer* initializer) override {
    SetDefaultDeviceScaleFactor(2.0);
    initializer->AddMonitor(gfx::Rect(0, 0, 1920, 1200),
                            gfx::Rect(0, 0, 1920, 1100),
                            L"primary",
                            2.0);
    fake_hwnd_ = initializer->CreateFakeHwnd(gfx::Rect(0, 0, 1920, 1100));
  }

  HWND GetFakeHwnd() {
    return fake_hwnd_;
  }

 private:
  HWND fake_hwnd_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScreenWinTestSingleDisplay2x);
};

}  // namespace

TEST_F(ScreenWinTestSingleDisplay2x, ScreenToDIPPoints) {
  EXPECT_EQ(gfx::Point(0, 0), ScreenWin::ScreenToDIPPoint(gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(182, 347),
            ScreenWin::ScreenToDIPPoint(gfx::Point(365, 694)));
  EXPECT_EQ(gfx::Point(959, 599),
            ScreenWin::ScreenToDIPPoint(gfx::Point(1919, 1199)));
}

TEST_F(ScreenWinTestSingleDisplay2x, DIPToScreenPoints) {
  EXPECT_EQ(gfx::Point(0, 0), ScreenWin::DIPToScreenPoint(gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(364, 694),
            ScreenWin::DIPToScreenPoint(gfx::Point(182, 347)));
  EXPECT_EQ(gfx::Point(1918, 1198),
            ScreenWin::DIPToScreenPoint(gfx::Point(959, 599)));
}

TEST_F(ScreenWinTestSingleDisplay2x, ClientToDIPPoints) {
  HWND hwnd = GetFakeHwnd();
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::ClientToDIPPoint(hwnd, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(182, 347),
            ScreenWin::ClientToDIPPoint(hwnd, gfx::Point(365, 694)));
  EXPECT_EQ(gfx::Point(959, 599),
            ScreenWin::ClientToDIPPoint(hwnd, gfx::Point(1919, 1199)));
}

TEST_F(ScreenWinTestSingleDisplay2x, DIPToClientPoints) {
  HWND hwnd = GetFakeHwnd();
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::DIPToClientPoint(hwnd, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(364, 694),
            ScreenWin::DIPToClientPoint(hwnd, gfx::Point(182, 347)));
  EXPECT_EQ(gfx::Point(1918, 1198),
            ScreenWin::DIPToClientPoint(hwnd, gfx::Point(959, 599)));
}

TEST_F(ScreenWinTestSingleDisplay2x, ScreenToDIPRects) {
  HWND hwnd = GetFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 25, 50),
            ScreenWin::ScreenToDIPRect(hwnd, gfx::Rect(0, 0, 50, 100)));
  EXPECT_EQ(gfx::Rect(126, 248, 21, 26),
            ScreenWin::ScreenToDIPRect(hwnd, gfx::Rect(253, 496, 41, 52)));
}

TEST_F(ScreenWinTestSingleDisplay2x, DIPToScreenRects) {
  HWND hwnd = GetFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 50, 100),
            ScreenWin::DIPToScreenRect(hwnd, gfx::Rect(0, 0, 25, 50)));
  EXPECT_EQ(gfx::Rect(252, 496, 42, 52),
            ScreenWin::DIPToScreenRect(hwnd, gfx::Rect(126, 248, 21, 26)));
}

TEST_F(ScreenWinTestSingleDisplay2x, ClientToDIPRects) {
  HWND hwnd = GetFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 25, 50),
            ScreenWin::ClientToDIPRect(hwnd, gfx::Rect(0, 0, 50, 100)));
  EXPECT_EQ(gfx::Rect(126, 248, 21, 26),
            ScreenWin::ClientToDIPRect(hwnd, gfx::Rect(253, 496, 41, 52)));
}

TEST_F(ScreenWinTestSingleDisplay2x, DIPToClientRects) {
  HWND hwnd = GetFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 50, 100),
            ScreenWin::DIPToClientRect(hwnd, gfx::Rect(0, 0, 25, 50)));
  EXPECT_EQ(gfx::Rect(252, 496, 42, 52),
            ScreenWin::DIPToClientRect(hwnd, gfx::Rect(126, 248, 21, 26)));
}

TEST_F(ScreenWinTestSingleDisplay2x, ScreenToDIPSize) {
  EXPECT_EQ(gfx::Size(21, 66),
            ScreenWin::ScreenToDIPSize(GetFakeHwnd(), gfx::Size(42, 131)));
}

TEST_F(ScreenWinTestSingleDisplay2x, DIPToScreenSize) {
  EXPECT_EQ(gfx::Size(42, 132),
            ScreenWin::DIPToScreenSize(GetFakeHwnd(), gfx::Size(21, 66)));
}

TEST_F(ScreenWinTestSingleDisplay2x, GetDisplays) {
  std::vector<Display> displays = GetScreen()->GetAllDisplays();
  ASSERT_EQ(1u, displays.size());
  EXPECT_EQ(gfx::Rect(0, 0, 960, 600), displays[0].bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 960, 550), displays[0].work_area());
}

TEST_F(ScreenWinTestSingleDisplay2x, GetDisplayNearestWindow) {
  Screen* screen = GetScreen();
  gfx::NativeWindow native_window = GetNativeWindowFromHWND(GetFakeHwnd());
  EXPECT_EQ(screen->GetAllDisplays()[0],
            screen->GetDisplayNearestWindow(native_window));
}

TEST_F(ScreenWinTestSingleDisplay2x, GetDisplayNearestPoint) {
  Screen* screen = GetScreen();
  Display display = screen->GetAllDisplays()[0];
  EXPECT_EQ(display, screen->GetDisplayNearestPoint(gfx::Point(0, 0)));
  EXPECT_EQ(display, screen->GetDisplayNearestPoint(gfx::Point(125, 476)));
  EXPECT_EQ(display, screen->GetDisplayNearestPoint(gfx::Point(959, 599)));
}

TEST_F(ScreenWinTestSingleDisplay2x, GetDisplayMatching) {
  Screen* screen = GetScreen();
  Display display = screen->GetAllDisplays()[0];
  EXPECT_EQ(display, screen->GetDisplayMatching(gfx::Rect(0, 0, 100, 100)));
  EXPECT_EQ(display,
            screen->GetDisplayMatching(gfx::Rect(1819, 1099, 100, 100)));
}

namespace {

// Two Displays of 1.0 Device Scale Factor.
class ScreenWinTestTwoDisplays1x : public ScreenWinTest {
 public:
  ScreenWinTestTwoDisplays1x() = default;

  void SetUpScreen(TestScreenWinInitializer* initializer) override {
    initializer->AddMonitor(gfx::Rect(0, 0, 1920, 1200),
                            gfx::Rect(0, 0, 1920, 1100),
                            L"primary",
                            1.0);
    initializer->AddMonitor(gfx::Rect(1920, 0, 800, 600),
                            gfx::Rect(1920, 0, 800, 600),
                            L"secondary",
                            1.0);
    fake_hwnd_left_ = initializer->CreateFakeHwnd(gfx::Rect(0, 0, 1920, 1100));
    fake_hwnd_right_ =
        initializer->CreateFakeHwnd(gfx::Rect(1920, 0, 800, 600));
  }

  HWND GetLeftFakeHwnd() {
    return fake_hwnd_left_;
  }

  HWND GetRightFakeHwnd() {
    return fake_hwnd_right_;
  }

 private:
  HWND fake_hwnd_left_ = nullptr;
  HWND fake_hwnd_right_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScreenWinTestTwoDisplays1x);
};

}  // namespace

TEST_F(ScreenWinTestTwoDisplays1x, ScreenToDIPPoints) {
  gfx::Point left_origin(0, 0);
  gfx::Point left_middle(365, 694);
  gfx::Point left_lower_right(1919, 1199);
  EXPECT_EQ(left_origin, ScreenWin::ScreenToDIPPoint(left_origin));
  EXPECT_EQ(left_middle, ScreenWin::ScreenToDIPPoint(left_middle));
  EXPECT_EQ(left_lower_right, ScreenWin::ScreenToDIPPoint(left_lower_right));

  gfx::Point right_origin(1920, 0);
  gfx::Point right_middle(2384, 351);
  gfx::Point right_lower_right(2719, 599);
  EXPECT_EQ(right_origin, ScreenWin::ScreenToDIPPoint(right_origin));
  EXPECT_EQ(right_middle, ScreenWin::ScreenToDIPPoint(right_middle));
  EXPECT_EQ(right_lower_right, ScreenWin::ScreenToDIPPoint(right_lower_right));
}

TEST_F(ScreenWinTestTwoDisplays1x, DIPToScreenPoints) {
  gfx::Point left_origin(0, 0);
  gfx::Point left_middle(365, 694);
  gfx::Point left_lower_right(1919, 1199);
  EXPECT_EQ(left_origin, ScreenWin::DIPToScreenPoint(left_origin));
  EXPECT_EQ(left_middle, ScreenWin::DIPToScreenPoint(left_middle));
  EXPECT_EQ(left_lower_right, ScreenWin::DIPToScreenPoint(left_lower_right));

  gfx::Point right_origin(1920, 0);
  gfx::Point right_middle(2384, 351);
  gfx::Point right_lower_right(2719, 599);
  EXPECT_EQ(right_origin, ScreenWin::DIPToScreenPoint(right_origin));
  EXPECT_EQ(right_middle, ScreenWin::DIPToScreenPoint(right_middle));
  EXPECT_EQ(right_lower_right, ScreenWin::DIPToScreenPoint(right_lower_right));
}

TEST_F(ScreenWinTestTwoDisplays1x, ClientToDIPPoints) {
  HWND left_hwnd = GetLeftFakeHwnd();
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::ClientToDIPPoint(left_hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::ClientToDIPPoint(left_hwnd, middle));
  EXPECT_EQ(lower_right, ScreenWin::ClientToDIPPoint(left_hwnd, lower_right));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(origin, ScreenWin::ClientToDIPPoint(right_hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::ClientToDIPPoint(right_hwnd, middle));
  EXPECT_EQ(lower_right, ScreenWin::ClientToDIPPoint(right_hwnd, lower_right));
}

TEST_F(ScreenWinTestTwoDisplays1x, DIPToClientPoints) {
  HWND left_hwnd = GetLeftFakeHwnd();
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::DIPToClientPoint(left_hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::DIPToClientPoint(left_hwnd, middle));
  EXPECT_EQ(lower_right, ScreenWin::DIPToClientPoint(left_hwnd, lower_right));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(origin, ScreenWin::DIPToClientPoint(right_hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::DIPToClientPoint(right_hwnd, middle));
  EXPECT_EQ(lower_right, ScreenWin::DIPToClientPoint(right_hwnd, lower_right));
}

TEST_F(ScreenWinTestTwoDisplays1x, ScreenToDIPRects) {
  HWND left_hwnd = GetLeftFakeHwnd();
  gfx::Rect left_origin(0, 0, 50, 100);
  gfx::Rect left_middle(253, 495, 41, 52);
  EXPECT_EQ(left_origin, ScreenWin::ScreenToDIPRect(left_hwnd, left_origin));
  EXPECT_EQ(left_middle, ScreenWin::ScreenToDIPRect(left_hwnd, left_middle));

  HWND right_hwnd = GetRightFakeHwnd();
  gfx::Rect right_origin(1920, 0, 200, 300);
  gfx::Rect right_middle(2000, 496, 100, 200);
  EXPECT_EQ(right_origin, ScreenWin::ScreenToDIPRect(right_hwnd, right_origin));
  EXPECT_EQ(right_middle, ScreenWin::ScreenToDIPRect(right_hwnd, right_middle));
}

TEST_F(ScreenWinTestTwoDisplays1x, DIPToScreenRects) {
  HWND left_hwnd = GetLeftFakeHwnd();
  gfx::Rect left_origin(0, 0, 50, 100);
  gfx::Rect left_middle(253, 495, 41, 52);
  EXPECT_EQ(left_origin, ScreenWin::DIPToScreenRect(left_hwnd, left_origin));
  EXPECT_EQ(left_middle, ScreenWin::DIPToScreenRect(left_hwnd, left_middle));

  HWND right_hwnd = GetRightFakeHwnd();
  gfx::Rect right_origin(1920, 0, 200, 300);
  gfx::Rect right_middle(2000, 496, 100, 200);
  EXPECT_EQ(right_origin, ScreenWin::DIPToScreenRect(right_hwnd, right_origin));
  EXPECT_EQ(right_middle, ScreenWin::DIPToScreenRect(right_hwnd, right_middle));
}

TEST_F(ScreenWinTestTwoDisplays1x, ClientToDIPRects) {
  HWND left_hwnd = GetLeftFakeHwnd();
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::ClientToDIPRect(left_hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::ClientToDIPRect(left_hwnd, middle));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(origin, ScreenWin::ClientToDIPRect(right_hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::ClientToDIPRect(right_hwnd, middle));
}

TEST_F(ScreenWinTestTwoDisplays1x, DIPToClientRects) {
  HWND left_hwnd = GetLeftFakeHwnd();
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::DIPToClientRect(left_hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::DIPToClientRect(left_hwnd, middle));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(origin, ScreenWin::DIPToClientRect(right_hwnd, origin));
  EXPECT_EQ(middle, ScreenWin::DIPToClientRect(right_hwnd, middle));
}

TEST_F(ScreenWinTestTwoDisplays1x, ScreenToDIPSize) {
  HWND left_hwnd = GetLeftFakeHwnd();
  gfx::Size size(42, 131);
  EXPECT_EQ(size, ScreenWin::ScreenToDIPSize(left_hwnd, size));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(size, ScreenWin::ScreenToDIPSize(right_hwnd, size));
}

TEST_F(ScreenWinTestTwoDisplays1x, DIPToScreenSize) {
  HWND left_hwnd = GetLeftFakeHwnd();
  gfx::Size size(42, 131);
  EXPECT_EQ(size, ScreenWin::DIPToScreenSize(left_hwnd, size));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(size, ScreenWin::DIPToScreenSize(right_hwnd, size));
}

TEST_F(ScreenWinTestTwoDisplays1x, GetDisplays) {
  std::vector<Display> displays = GetScreen()->GetAllDisplays();
  ASSERT_EQ(2u, displays.size());
  EXPECT_EQ(gfx::Rect(0, 0, 1920, 1200), displays[0].bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 1920, 1100), displays[0].work_area());
  EXPECT_EQ(gfx::Rect(1920, 0, 800, 600), displays[1].bounds());
  EXPECT_EQ(gfx::Rect(1920, 0, 800, 600), displays[1].work_area());
}

TEST_F(ScreenWinTestTwoDisplays1x, GetNumDisplays) {
  EXPECT_EQ(2, GetScreen()->GetNumDisplays());
}

TEST_F(ScreenWinTestTwoDisplays1x, GetDisplayNearestWindowPrimaryDisplay) {
  Screen* screen = GetScreen();
  EXPECT_EQ(screen->GetPrimaryDisplay(),
            screen->GetDisplayNearestWindow(nullptr));
}

TEST_F(ScreenWinTestTwoDisplays1x, GetDisplayNearestWindow) {
  Screen* screen = GetScreen();
  const Display left_display = screen->GetAllDisplays()[0];
  const Display right_display = screen->GetAllDisplays()[1];

  gfx::NativeWindow left_window = GetNativeWindowFromHWND(GetLeftFakeHwnd());
  EXPECT_EQ(left_display, screen->GetDisplayNearestWindow(left_window));

  gfx::NativeWindow right_window = GetNativeWindowFromHWND(GetRightFakeHwnd());
  EXPECT_EQ(right_display, screen->GetDisplayNearestWindow(right_window));
}

TEST_F(ScreenWinTestTwoDisplays1x, GetDisplayNearestPoint) {
  Screen* screen = GetScreen();
  const Display left_display = screen->GetAllDisplays()[0];
  const Display right_display = screen->GetAllDisplays()[1];

  EXPECT_EQ(left_display, screen->GetDisplayNearestPoint(gfx::Point(0, 0)));
  EXPECT_EQ(left_display, screen->GetDisplayNearestPoint(gfx::Point(250, 952)));
  EXPECT_EQ(left_display,
            screen->GetDisplayNearestPoint(gfx::Point(1919, 1199)));

  EXPECT_EQ(right_display, screen->GetDisplayNearestPoint(gfx::Point(1920, 0)));
  EXPECT_EQ(right_display,
            screen->GetDisplayNearestPoint(gfx::Point(2000, 400)));
  EXPECT_EQ(right_display,
            screen->GetDisplayNearestPoint(gfx::Point(2719, 599)));
}

TEST_F(ScreenWinTestTwoDisplays1x, GetDisplayMatching) {
  Screen* screen = GetScreen();
  const Display left_display = screen->GetAllDisplays()[0];
  const Display right_display = screen->GetAllDisplays()[1];

  EXPECT_EQ(left_display,
            screen->GetDisplayMatching(gfx::Rect(0, 0, 100, 100)));
  EXPECT_EQ(left_display,
            screen->GetDisplayMatching(gfx::Rect(1819, 1099, 100, 100)));

  EXPECT_EQ(right_display,
            screen->GetDisplayMatching(gfx::Rect(1920, 0, 100, 100)));
  EXPECT_EQ(right_display,
            screen->GetDisplayMatching(gfx::Rect(2619, 499, 100, 100)));
}

TEST_F(ScreenWinTestTwoDisplays1x, GetPrimaryDisplay) {
  Screen* screen = GetScreen();
  Display primary = screen->GetPrimaryDisplay();
  EXPECT_EQ(gfx::Point(0, 0), primary.bounds().origin());
}

namespace {

// Two Displays of 2.0 Device Scale Factor.
class ScreenWinTestTwoDisplays2x : public ScreenWinTest {
 public:
  ScreenWinTestTwoDisplays2x() = default;

  void SetUpScreen(TestScreenWinInitializer* initializer) override {
    SetDefaultDeviceScaleFactor(2.0);
    initializer->AddMonitor(gfx::Rect(0, 0, 1920, 1200),
                            gfx::Rect(0, 0, 1920, 1100),
                            L"primary",
                            2.0);
    initializer->AddMonitor(gfx::Rect(1920, 0, 800, 600),
                            gfx::Rect(1920, 0, 800, 600),
                            L"secondary",
                            2.0);
    fake_hwnd_left_ = initializer->CreateFakeHwnd(gfx::Rect(0, 0, 1920, 1100));
    fake_hwnd_right_ =
        initializer->CreateFakeHwnd(gfx::Rect(1920, 0, 800, 600));
  }

  HWND GetLeftFakeHwnd() {
    return fake_hwnd_left_;
  }

  HWND GetRightFakeHwnd() {
    return fake_hwnd_right_;
  }

 private:
  HWND fake_hwnd_left_ = nullptr;
  HWND fake_hwnd_right_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScreenWinTestTwoDisplays2x);
};

}  // namespace

TEST_F(ScreenWinTestTwoDisplays2x, ScreenToDIPPoints) {
  EXPECT_EQ(gfx::Point(0, 0), ScreenWin::ScreenToDIPPoint(gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(182, 347),
            ScreenWin::ScreenToDIPPoint(gfx::Point(365, 694)));
  EXPECT_EQ(gfx::Point(959, 599),
            ScreenWin::ScreenToDIPPoint(gfx::Point(1919, 1199)));

  EXPECT_EQ(gfx::Point(960, 0),
            ScreenWin::ScreenToDIPPoint(gfx::Point(1920, 0)));
  EXPECT_EQ(gfx::Point(1192, 175),
            ScreenWin::ScreenToDIPPoint(gfx::Point(2384, 351)));
  EXPECT_EQ(gfx::Point(1359, 299),
            ScreenWin::ScreenToDIPPoint(gfx::Point(2719, 599)));
}

TEST_F(ScreenWinTestTwoDisplays2x, DIPToScreenPoints) {
  EXPECT_EQ(gfx::Point(0, 0), ScreenWin::DIPToScreenPoint(gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(364, 694),
            ScreenWin::DIPToScreenPoint(gfx::Point(182, 347)));
  EXPECT_EQ(gfx::Point(1918, 1198),
            ScreenWin::DIPToScreenPoint(gfx::Point(959, 599)));

  EXPECT_EQ(gfx::Point(1920, 0),
            ScreenWin::DIPToScreenPoint(gfx::Point(960, 0)));
  EXPECT_EQ(gfx::Point(2384, 350),
            ScreenWin::DIPToScreenPoint(gfx::Point(1192, 175)));
  EXPECT_EQ(gfx::Point(2718, 598),
            ScreenWin::DIPToScreenPoint(gfx::Point(1359, 299)));
}

TEST_F(ScreenWinTestTwoDisplays2x, ClientToDIPPoints) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::ClientToDIPPoint(left_hwnd, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(182, 347),
            ScreenWin::ClientToDIPPoint(left_hwnd, gfx::Point(365, 694)));
  EXPECT_EQ(gfx::Point(959, 599),
            ScreenWin::ClientToDIPPoint(left_hwnd, gfx::Point(1919, 1199)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::ClientToDIPPoint(right_hwnd, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(182, 347),
            ScreenWin::ClientToDIPPoint(right_hwnd, gfx::Point(365, 694)));
  EXPECT_EQ(gfx::Point(959, 599),
            ScreenWin::ClientToDIPPoint(right_hwnd, gfx::Point(1919, 1199)));
}

TEST_F(ScreenWinTestTwoDisplays2x, DIPToClientPoints) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::DIPToClientPoint(left_hwnd, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(364, 694),
            ScreenWin::DIPToClientPoint(left_hwnd, gfx::Point(182, 347)));
  EXPECT_EQ(gfx::Point(1918, 1198),
            ScreenWin::DIPToClientPoint(left_hwnd, gfx::Point(959, 599)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::DIPToClientPoint(right_hwnd, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(364, 694),
            ScreenWin::DIPToClientPoint(right_hwnd, gfx::Point(182, 347)));
  EXPECT_EQ(gfx::Point(1918, 1198),
            ScreenWin::DIPToClientPoint(right_hwnd, gfx::Point(959, 599)));
}

TEST_F(ScreenWinTestTwoDisplays2x, ScreenToDIPRects) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 25, 50),
            ScreenWin::ScreenToDIPRect(left_hwnd, gfx::Rect(0, 0, 50, 100)));
  EXPECT_EQ(gfx::Rect(126, 248, 21, 26),
            ScreenWin::ScreenToDIPRect(left_hwnd, gfx::Rect(253, 496, 41, 52)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Rect(960, 0, 100, 150),
            ScreenWin::ScreenToDIPRect(right_hwnd,
                                       gfx::Rect(1920, 0, 200, 300)));
  EXPECT_EQ(gfx::Rect(1000, 248, 50, 100),
            ScreenWin::ScreenToDIPRect(right_hwnd,
                                       gfx::Rect(2000, 496, 100, 200)));
}

TEST_F(ScreenWinTestTwoDisplays2x, DIPToScreenRects) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 50, 100),
            ScreenWin::DIPToScreenRect(left_hwnd, gfx::Rect(0, 0, 25, 50)));
  EXPECT_EQ(gfx::Rect(252, 496, 42, 52),
            ScreenWin::DIPToScreenRect(left_hwnd, gfx::Rect(126, 248, 21, 26)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Rect(1920, 0, 200, 300),
            ScreenWin::DIPToScreenRect(right_hwnd,
                                       gfx::Rect(960, 0, 100, 150)));
  EXPECT_EQ(gfx::Rect(2000, 496, 100, 200),
            ScreenWin::DIPToScreenRect(right_hwnd,
                                       gfx::Rect(1000, 248, 50, 100)));
}

TEST_F(ScreenWinTestTwoDisplays2x, ClientToDIPRects) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 25, 50),
            ScreenWin::ClientToDIPRect(left_hwnd, gfx::Rect(0, 0, 50, 100)));
  EXPECT_EQ(gfx::Rect(126, 248, 21, 26),
            ScreenWin::ClientToDIPRect(left_hwnd, gfx::Rect(253, 496, 41, 52)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 25, 50),
            ScreenWin::ClientToDIPRect(right_hwnd, gfx::Rect(0, 0, 50, 100)));
  EXPECT_EQ(gfx::Rect(126, 248, 21, 26),
            ScreenWin::ClientToDIPRect(right_hwnd,
                                       gfx::Rect(253, 496, 41, 52)));
}

TEST_F(ScreenWinTestTwoDisplays2x, DIPToClientRects) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 50, 100),
            ScreenWin::DIPToClientRect(left_hwnd, gfx::Rect(0, 0, 25, 50)));
  EXPECT_EQ(gfx::Rect(252, 496, 42, 52),
            ScreenWin::DIPToClientRect(left_hwnd, gfx::Rect(126, 248, 21, 26)));
}

TEST_F(ScreenWinTestTwoDisplays2x, ScreenToDIPSize) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Size(21, 66),
            ScreenWin::ScreenToDIPSize(left_hwnd, gfx::Size(42, 131)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Size(21, 66),
            ScreenWin::ScreenToDIPSize(right_hwnd, gfx::Size(42, 131)));
}

TEST_F(ScreenWinTestTwoDisplays2x, DIPToScreenSize) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Size(42, 132),
            ScreenWin::DIPToScreenSize(left_hwnd, gfx::Size(21, 66)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Size(42, 132),
            ScreenWin::DIPToScreenSize(right_hwnd, gfx::Size(21, 66)));
}

TEST_F(ScreenWinTestTwoDisplays2x, GetDisplays) {
  std::vector<Display> displays = GetScreen()->GetAllDisplays();
  ASSERT_EQ(2u, displays.size());
  EXPECT_EQ(gfx::Rect(0, 0, 960, 600), displays[0].bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 960, 550), displays[0].work_area());
  EXPECT_EQ(gfx::Rect(960, 0, 400, 300), displays[1].bounds());
  EXPECT_EQ(gfx::Rect(960, 0, 400, 300), displays[1].work_area());
}

TEST_F(ScreenWinTestTwoDisplays2x, GetDisplayNearestWindowPrimaryDisplay) {
  Screen* screen = GetScreen();
  EXPECT_EQ(screen->GetPrimaryDisplay(),
            screen->GetDisplayNearestWindow(nullptr));
}

TEST_F(ScreenWinTestTwoDisplays2x, GetDisplayNearestWindow) {
  Screen* screen = GetScreen();
  const Display left_display = screen->GetAllDisplays()[0];
  const Display right_display = screen->GetAllDisplays()[1];

  gfx::NativeWindow left_window = GetNativeWindowFromHWND(GetLeftFakeHwnd());
  EXPECT_EQ(left_display, screen->GetDisplayNearestWindow(left_window));

  gfx::NativeWindow right_window = GetNativeWindowFromHWND(GetRightFakeHwnd());
  EXPECT_EQ(right_display, screen->GetDisplayNearestWindow(right_window));
}

TEST_F(ScreenWinTestTwoDisplays2x, GetDisplayNearestPoint) {
  Screen* screen = GetScreen();
  const Display left_display = screen->GetAllDisplays()[0];
  const Display right_display = screen->GetAllDisplays()[1];

  EXPECT_EQ(left_display, screen->GetDisplayNearestPoint(gfx::Point(0, 0)));
  EXPECT_EQ(left_display, screen->GetDisplayNearestPoint(gfx::Point(125, 476)));
  EXPECT_EQ(left_display,
            screen->GetDisplayNearestPoint(gfx::Point(959, 599)));

  EXPECT_EQ(right_display, screen->GetDisplayNearestPoint(gfx::Point(960, 0)));
  EXPECT_EQ(right_display,
            screen->GetDisplayNearestPoint(gfx::Point(1000, 200)));
  EXPECT_EQ(right_display,
            screen->GetDisplayNearestPoint(gfx::Point(1359, 299)));
}

TEST_F(ScreenWinTestTwoDisplays2x, GetDisplayMatching) {
  Screen* screen = GetScreen();
  const Display left_display = screen->GetAllDisplays()[0];
  const Display right_display = screen->GetAllDisplays()[1];

  EXPECT_EQ(left_display,
            screen->GetDisplayMatching(gfx::Rect(0, 0, 100, 100)));
  EXPECT_EQ(left_display,
            screen->GetDisplayMatching(gfx::Rect(1819, 1099, 100, 100)));

  EXPECT_EQ(right_display,
            screen->GetDisplayMatching(gfx::Rect(1920, 0, 100, 100)));
  EXPECT_EQ(right_display,
            screen->GetDisplayMatching(gfx::Rect(2619, 499, 100, 100)));
}

TEST_F(ScreenWinTestTwoDisplays2x, GetPrimaryDisplay) {
  Screen* screen = GetScreen();
  Display primary = screen->GetPrimaryDisplay();
  EXPECT_EQ(gfx::Point(0, 0), primary.bounds().origin());
}

namespace {

// Two Displays of 2.0 (Left) and 1.0 (Right) Device Scale Factor under
// Windows DPI Virtualization. Note that the displays do not form a euclidean
// space.
class ScreenWinTestTwoDisplays2x1xVirtualized : public ScreenWinTest {
 public:
  ScreenWinTestTwoDisplays2x1xVirtualized() = default;

  void SetUpScreen(TestScreenWinInitializer* initializer) override {
    SetDefaultDeviceScaleFactor(2.0);
    initializer->AddMonitor(gfx::Rect(0, 0, 3200, 1600),
                            gfx::Rect(0, 0, 3200, 1500),
                            L"primary",
                            2.0);
    initializer->AddMonitor(gfx::Rect(6400, 0, 3840, 2400),
                            gfx::Rect(6400, 0, 3840, 2400),
                            L"secondary",
                            2.0);
    fake_hwnd_left_ = initializer->CreateFakeHwnd(gfx::Rect(0, 0, 3200, 1500));
    fake_hwnd_right_ =
        initializer->CreateFakeHwnd(gfx::Rect(6400, 0, 3840, 2400));
  }

  HWND GetLeftFakeHwnd() {
    return fake_hwnd_left_;
  }

  HWND GetRightFakeHwnd() {
    return fake_hwnd_right_;
  }

 private:
  HWND fake_hwnd_left_ = nullptr;
  HWND fake_hwnd_right_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScreenWinTestTwoDisplays2x1xVirtualized);
};

}  // namespace

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, ScreenToDIPPoints) {
  EXPECT_EQ(gfx::Point(0, 0), ScreenWin::ScreenToDIPPoint(gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(182, 347),
            ScreenWin::ScreenToDIPPoint(gfx::Point(365, 694)));
  EXPECT_EQ(gfx::Point(1599, 799),
            ScreenWin::ScreenToDIPPoint(gfx::Point(3199, 1599)));

  EXPECT_EQ(gfx::Point(3200, 0),
            ScreenWin::ScreenToDIPPoint(gfx::Point(6400, 0)));
  EXPECT_EQ(gfx::Point(4192, 175),
            ScreenWin::ScreenToDIPPoint(gfx::Point(8384, 351)));
  EXPECT_EQ(gfx::Point(5119, 1199),
            ScreenWin::ScreenToDIPPoint(gfx::Point(10239, 2399)));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, DIPToScreenPoints) {
  EXPECT_EQ(gfx::Point(0, 0), ScreenWin::DIPToScreenPoint(gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(364, 694),
            ScreenWin::DIPToScreenPoint(gfx::Point(182, 347)));
  EXPECT_EQ(gfx::Point(3198, 1598),
            ScreenWin::DIPToScreenPoint(gfx::Point(1599, 799)));

  EXPECT_EQ(gfx::Point(6400, 0),
            ScreenWin::DIPToScreenPoint(gfx::Point(3200, 0)));
  EXPECT_EQ(gfx::Point(8384, 350),
            ScreenWin::DIPToScreenPoint(gfx::Point(4192, 175)));
  EXPECT_EQ(gfx::Point(10238, 2398),
            ScreenWin::DIPToScreenPoint(gfx::Point(5119, 1199)));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, ClientToDIPPoints) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::ClientToDIPPoint(left_hwnd, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(182, 347),
            ScreenWin::ClientToDIPPoint(left_hwnd, gfx::Point(365, 694)));
  EXPECT_EQ(gfx::Point(959, 599),
            ScreenWin::ClientToDIPPoint(left_hwnd, gfx::Point(1919, 1199)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::ClientToDIPPoint(right_hwnd, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(182, 347),
            ScreenWin::ClientToDIPPoint(right_hwnd, gfx::Point(365, 694)));
  EXPECT_EQ(gfx::Point(959, 599),
            ScreenWin::ClientToDIPPoint(right_hwnd, gfx::Point(1919, 1199)));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, DIPToClientPoints) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::DIPToClientPoint(left_hwnd, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(364, 694),
            ScreenWin::DIPToClientPoint(left_hwnd, gfx::Point(182, 347)));
  EXPECT_EQ(gfx::Point(1918, 1198),
            ScreenWin::DIPToClientPoint(left_hwnd, gfx::Point(959, 599)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::DIPToClientPoint(right_hwnd, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(364, 694),
            ScreenWin::DIPToClientPoint(right_hwnd, gfx::Point(182, 347)));
  EXPECT_EQ(gfx::Point(1918, 1198),
            ScreenWin::DIPToClientPoint(right_hwnd, gfx::Point(959, 599)));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, ScreenToDIPRects) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 25, 50),
            ScreenWin::ScreenToDIPRect(left_hwnd, gfx::Rect(0, 0, 50, 100)));
  EXPECT_EQ(gfx::Rect(126, 248, 21, 26),
            ScreenWin::ScreenToDIPRect(left_hwnd, gfx::Rect(253, 496, 41, 52)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Rect(3200, 0, 100, 150),
            ScreenWin::ScreenToDIPRect(right_hwnd,
                                       gfx::Rect(6400, 0, 200, 300)));
  EXPECT_EQ(gfx::Rect(3500, 248, 50, 100),
            ScreenWin::ScreenToDIPRect(right_hwnd,
                                       gfx::Rect(7000, 496, 100, 200)));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, DIPToScreenRects) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 50, 100),
            ScreenWin::DIPToScreenRect(left_hwnd, gfx::Rect(0, 0, 25, 50)));
  EXPECT_EQ(gfx::Rect(252, 496, 42, 52),
            ScreenWin::DIPToScreenRect(left_hwnd, gfx::Rect(126, 248, 21, 26)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Rect(6400, 0, 200, 300),
            ScreenWin::DIPToScreenRect(right_hwnd,
                                       gfx::Rect(3200, 0, 100, 150)));
  EXPECT_EQ(gfx::Rect(7000, 496, 100, 200),
            ScreenWin::DIPToScreenRect(right_hwnd,
                                       gfx::Rect(3500, 248, 50, 100)));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, ClientToDIPRects) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 25, 50),
            ScreenWin::ClientToDIPRect(left_hwnd, gfx::Rect(0, 0, 50, 100)));
  EXPECT_EQ(gfx::Rect(126, 248, 21, 26),
            ScreenWin::ClientToDIPRect(left_hwnd, gfx::Rect(253, 496, 41, 52)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 25, 50),
            ScreenWin::ClientToDIPRect(right_hwnd, gfx::Rect(0, 0, 50, 100)));
  EXPECT_EQ(gfx::Rect(126, 248, 21, 26),
            ScreenWin::ClientToDIPRect(right_hwnd,
                                       gfx::Rect(253, 496, 41, 52)));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, DIPToClientRects) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 50, 100),
            ScreenWin::DIPToClientRect(left_hwnd, gfx::Rect(0, 0, 25, 50)));
  EXPECT_EQ(gfx::Rect(252, 496, 42, 52),
            ScreenWin::DIPToClientRect(left_hwnd, gfx::Rect(126, 248, 21, 26)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Rect(0, 0, 50, 100),
            ScreenWin::DIPToClientRect(right_hwnd, gfx::Rect(0, 0, 25, 50)));
  EXPECT_EQ(gfx::Rect(252, 496, 42, 52),
            ScreenWin::DIPToClientRect(right_hwnd,
                                       gfx::Rect(126, 248, 21, 26)));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, ScreenToDIPSize) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Size(21, 66),
            ScreenWin::ScreenToDIPSize(left_hwnd, gfx::Size(42, 131)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Size(21, 66),
            ScreenWin::ScreenToDIPSize(right_hwnd, gfx::Size(42, 131)));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, DIPToScreenSize) {
  HWND left_hwnd = GetLeftFakeHwnd();
  EXPECT_EQ(gfx::Size(42, 132),
            ScreenWin::DIPToScreenSize(left_hwnd, gfx::Size(21, 66)));

  HWND right_hwnd = GetRightFakeHwnd();
  EXPECT_EQ(gfx::Size(42, 132),
            ScreenWin::DIPToScreenSize(right_hwnd, gfx::Size(21, 66)));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, GetDisplays) {
  std::vector<Display> displays = GetScreen()->GetAllDisplays();
  ASSERT_EQ(2u, displays.size());
  EXPECT_EQ(gfx::Rect(0, 0, 1600, 800), displays[0].bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 1600, 750), displays[0].work_area());
  EXPECT_EQ(gfx::Rect(3200, 0, 1920, 1200), displays[1].bounds());
  EXPECT_EQ(gfx::Rect(3200, 0, 1920, 1200), displays[1].work_area());
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, GetNumDisplays) {
  EXPECT_EQ(2, GetScreen()->GetNumDisplays());
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized,
       GetDisplayNearestWindowPrimaryDisplay) {
  Screen* screen = GetScreen();
  EXPECT_EQ(screen->GetPrimaryDisplay(),
            screen->GetDisplayNearestWindow(nullptr));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, GetDisplayNearestWindow) {
  Screen* screen = GetScreen();
  const Display left_display = screen->GetAllDisplays()[0];
  const Display right_display = screen->GetAllDisplays()[1];

  gfx::NativeWindow left_window = GetNativeWindowFromHWND(GetLeftFakeHwnd());
  EXPECT_EQ(left_display, screen->GetDisplayNearestWindow(left_window));

  gfx::NativeWindow right_window = GetNativeWindowFromHWND(GetRightFakeHwnd());
  EXPECT_EQ(right_display, screen->GetDisplayNearestWindow(right_window));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, GetDisplayNearestPoint) {
  Screen* screen = GetScreen();
  const Display left_display = screen->GetAllDisplays()[0];
  const Display right_display = screen->GetAllDisplays()[1];

  EXPECT_EQ(left_display, screen->GetDisplayNearestPoint(gfx::Point(0, 0)));
  EXPECT_EQ(left_display, screen->GetDisplayNearestPoint(gfx::Point(125, 476)));
  EXPECT_EQ(left_display,
            screen->GetDisplayNearestPoint(gfx::Point(1599, 799)));

  EXPECT_EQ(right_display, screen->GetDisplayNearestPoint(gfx::Point(3200, 0)));
  EXPECT_EQ(right_display,
            screen->GetDisplayNearestPoint(gfx::Point(4000, 400)));
  EXPECT_EQ(right_display,
            screen->GetDisplayNearestPoint(gfx::Point(5119, 1199)));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, GetDisplayMatching) {
  Screen* screen = GetScreen();
  const Display left_display = screen->GetAllDisplays()[0];
  const Display right_display = screen->GetAllDisplays()[1];

  EXPECT_EQ(left_display,
            screen->GetDisplayMatching(gfx::Rect(0, 0, 100, 100)));
  EXPECT_EQ(left_display,
            screen->GetDisplayMatching(gfx::Rect(1819, 1099, 100, 100)));

  EXPECT_EQ(right_display,
            screen->GetDisplayMatching(gfx::Rect(6400, 0, 100, 100)));
  EXPECT_EQ(right_display,
            screen->GetDisplayMatching(gfx::Rect(10139, 2299, 100, 100)));
}

TEST_F(ScreenWinTestTwoDisplays2x1xVirtualized, GetPrimaryDisplay) {
  Screen* screen = GetScreen();
  Display primary = screen->GetPrimaryDisplay();
  EXPECT_EQ(gfx::Point(0, 0), primary.bounds().origin());
}

namespace {

// Forced 1x DPI for Other Tests without TestScreenWin.
class ScreenWinUninitializedForced1x : public testing::Test {
 public:
  ScreenWinUninitializedForced1x() = default;

  void SetUp() override {
    testing::Test::SetUp();
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor, "1");
  }

  void TearDown() override {
    display::Display::ResetForceDeviceScaleFactorForTesting();
    testing::Test::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenWinUninitializedForced1x);
};

}  // namespace

TEST_F(ScreenWinUninitializedForced1x, ScreenToDIPPoints) {
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::ScreenToDIPPoint(origin));
  EXPECT_EQ(middle, ScreenWin::ScreenToDIPPoint(middle));
  EXPECT_EQ(lower_right, ScreenWin::ScreenToDIPPoint(lower_right));
}

TEST_F(ScreenWinUninitializedForced1x, DIPToScreenPoints) {
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::DIPToScreenPoint(origin));
  EXPECT_EQ(middle, ScreenWin::DIPToScreenPoint(middle));
  EXPECT_EQ(lower_right, ScreenWin::DIPToScreenPoint(lower_right));
}

TEST_F(ScreenWinUninitializedForced1x, ClientToDIPPoints) {
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::ClientToDIPPoint(nullptr, origin));
  EXPECT_EQ(middle, ScreenWin::ClientToDIPPoint(nullptr, middle));
  EXPECT_EQ(lower_right, ScreenWin::ClientToDIPPoint(nullptr, lower_right));
}

TEST_F(ScreenWinUninitializedForced1x, DIPToClientPoints) {
  gfx::Point origin(0, 0);
  gfx::Point middle(365, 694);
  gfx::Point lower_right(1919, 1199);
  EXPECT_EQ(origin, ScreenWin::DIPToClientPoint(nullptr, origin));
  EXPECT_EQ(middle, ScreenWin::DIPToClientPoint(nullptr, middle));
  EXPECT_EQ(lower_right, ScreenWin::DIPToClientPoint(nullptr, lower_right));
}

TEST_F(ScreenWinUninitializedForced1x, ScreenToDIPRects) {
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::ScreenToDIPRect(nullptr, origin));
  EXPECT_EQ(middle, ScreenWin::ScreenToDIPRect(nullptr, middle));
}

TEST_F(ScreenWinUninitializedForced1x, DIPToScreenRects) {
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::DIPToScreenRect(nullptr, origin));
  EXPECT_EQ(middle, ScreenWin::DIPToScreenRect(nullptr, middle));
}

TEST_F(ScreenWinUninitializedForced1x, ClientToDIPRects) {
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::ClientToDIPRect(nullptr, origin));
  EXPECT_EQ(middle, ScreenWin::ClientToDIPRect(nullptr, middle));
}

TEST_F(ScreenWinUninitializedForced1x, DIPToClientRects) {
  gfx::Rect origin(0, 0, 50, 100);
  gfx::Rect middle(253, 495, 41, 52);
  EXPECT_EQ(origin, ScreenWin::DIPToClientRect(nullptr, origin));
  EXPECT_EQ(middle, ScreenWin::DIPToClientRect(nullptr, middle));
}

TEST_F(ScreenWinUninitializedForced1x, ScreenToDIPSize) {
  gfx::Size size(42, 131);
  EXPECT_EQ(size, ScreenWin::ScreenToDIPSize(nullptr, size));
}

TEST_F(ScreenWinUninitializedForced1x, DIPToScreenSize) {
  gfx::Size size(42, 131);
  EXPECT_EQ(size, ScreenWin::DIPToScreenSize(nullptr, size));
}

namespace {

// Forced 2x DPI for Other Tests without TestScreenWin.
class ScreenWinUninitializedForced2x : public testing::Test {
 public:
  ScreenWinUninitializedForced2x() = default;

  void SetUp() override {
    testing::Test::SetUp();
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor, "2");
  }

  void TearDown() override {
    display::Display::ResetForceDeviceScaleFactorForTesting();
    testing::Test::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenWinUninitializedForced2x);
};

}  // namespace

TEST_F(ScreenWinUninitializedForced2x, ScreenToDIPPoints) {
  EXPECT_EQ(gfx::Point(0, 0), ScreenWin::ScreenToDIPPoint(gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(182, 347),
            ScreenWin::ScreenToDIPPoint(gfx::Point(365, 694)));
  EXPECT_EQ(gfx::Point(959, 599),
            ScreenWin::ScreenToDIPPoint(gfx::Point(1919, 1199)));
}

TEST_F(ScreenWinUninitializedForced2x, DIPToScreenPoints) {
  EXPECT_EQ(gfx::Point(0, 0), ScreenWin::DIPToScreenPoint(gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(364, 694),
            ScreenWin::DIPToScreenPoint(gfx::Point(182, 347)));
  EXPECT_EQ(gfx::Point(1918, 1198),
            ScreenWin::DIPToScreenPoint(gfx::Point(959, 599)));
}

TEST_F(ScreenWinUninitializedForced2x, ClientToDIPPoints) {
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::ClientToDIPPoint(nullptr, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(182, 347),
            ScreenWin::ClientToDIPPoint(nullptr, gfx::Point(365, 694)));
  EXPECT_EQ(gfx::Point(959, 599),
            ScreenWin::ClientToDIPPoint(nullptr, gfx::Point(1919, 1199)));
}

TEST_F(ScreenWinUninitializedForced2x, DIPToClientPoints) {
  EXPECT_EQ(gfx::Point(0, 0),
            ScreenWin::DIPToClientPoint(nullptr, gfx::Point(0, 0)));
  EXPECT_EQ(gfx::Point(364, 694),
            ScreenWin::DIPToClientPoint(nullptr, gfx::Point(182, 347)));
  EXPECT_EQ(gfx::Point(1918, 1198),
            ScreenWin::DIPToClientPoint(nullptr, gfx::Point(959, 599)));
}

TEST_F(ScreenWinUninitializedForced2x, ScreenToDIPRects) {
  EXPECT_EQ(gfx::Rect(0, 0, 25, 50),
            ScreenWin::ScreenToDIPRect(nullptr, gfx::Rect(0, 0, 50, 100)));
  EXPECT_EQ(gfx::Rect(126, 248, 21, 26),
            ScreenWin::ScreenToDIPRect(nullptr, gfx::Rect(253, 496, 41, 52)));
}

TEST_F(ScreenWinUninitializedForced2x, DIPToScreenRects) {
  EXPECT_EQ(gfx::Rect(0, 0, 50, 100),
            ScreenWin::DIPToScreenRect(nullptr, gfx::Rect(0, 0, 25, 50)));
  EXPECT_EQ(gfx::Rect(252, 496, 42, 52),
            ScreenWin::DIPToScreenRect(nullptr, gfx::Rect(126, 248, 21, 26)));
}

TEST_F(ScreenWinUninitializedForced2x, ClientToDIPRects) {
  EXPECT_EQ(gfx::Rect(0, 0, 25, 50),
            ScreenWin::ClientToDIPRect(nullptr, gfx::Rect(0, 0, 50, 100)));
  EXPECT_EQ(gfx::Rect(126, 248, 21, 26),
            ScreenWin::ClientToDIPRect(nullptr, gfx::Rect(253, 496, 41, 52)));
}

TEST_F(ScreenWinUninitializedForced2x, DIPToClientRects) {
  EXPECT_EQ(gfx::Rect(0, 0, 50, 100),
            ScreenWin::DIPToClientRect(nullptr, gfx::Rect(0, 0, 25, 50)));
  EXPECT_EQ(gfx::Rect(252, 496, 42, 52),
            ScreenWin::DIPToClientRect(nullptr, gfx::Rect(126, 248, 21, 26)));
}

TEST_F(ScreenWinUninitializedForced2x, ScreenToDIPSize) {
  EXPECT_EQ(gfx::Size(21, 66),
            ScreenWin::ScreenToDIPSize(nullptr, gfx::Size(42, 131)));
}

TEST_F(ScreenWinUninitializedForced2x, DIPToScreenSize) {
  EXPECT_EQ(gfx::Size(42, 132),
            ScreenWin::DIPToScreenSize(nullptr, gfx::Size(21, 66)));
}

}  // namespace win
}  // namespace display
