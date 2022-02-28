// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCREEN_OZONE_H_
#define UI_AURA_SCREEN_OZONE_H_

#include <memory>
#include <vector>

#include "ui/aura/aura_export.h"
#include "ui/display/screen.h"

namespace ui {
class PlatformScreen;
}

namespace aura {

// display::Screen implementation on top of ui::PlatformScreen provided by
// Ozone.
class AURA_EXPORT ScreenOzone : public display::Screen {
 public:
  ScreenOzone();

  ScreenOzone(const ScreenOzone&) = delete;
  ScreenOzone& operator=(const ScreenOzone&) = delete;

  ~ScreenOzone() override;

  void Initialize();

  // display::Screen interface.
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override;
  int GetNumDisplays() const override;
  const std::vector<display::Display>& GetAllDisplays() const override;
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override;
  display::Display GetDisplayNearestView(gfx::NativeView view) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  display::Display GetPrimaryDisplay() const override;
  bool SetScreenSaverSuspended(bool suspend) override;
  bool IsScreenSaverActive() const override;
  base::TimeDelta CalculateIdleTime() const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;
  std::string GetCurrentWorkspace() override;
  std::vector<base::Value> GetGpuExtraInfo(
      const gfx::GpuExtraInfo& gpu_extra_info) override;

  // Returns the NativeWindow associated with the AcceleratedWidget.
  virtual gfx::NativeWindow GetNativeWindowFromAcceleratedWidget(
      gfx::AcceleratedWidget widget) const;

 protected:
  ui::PlatformScreen* platform_screen() { return platform_screen_.get(); }

 private:
  gfx::AcceleratedWidget GetAcceleratedWidgetForWindow(
      aura::Window* window) const;

  virtual void OnBeforePlatformScreenInit();

  display::Screen* const old_screen_ = display::Screen::SetScreenInstance(this);
  std::unique_ptr<ui::PlatformScreen> platform_screen_;
};

}  // namespace aura

#endif  // UI_AURA_SCREEN_OZONE_H_
