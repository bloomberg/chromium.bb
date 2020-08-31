// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_

#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

class WaylandConnection;
class ShellPopupWrapper;

class WaylandPopup : public WaylandWindow {
 public:
  WaylandPopup(PlatformWindowDelegate* delegate, WaylandConnection* connection);
  ~WaylandPopup() override;

  ShellPopupWrapper* shell_popup() const { return shell_popup_.get(); }

  // PlatformWindow
  void Show(bool inactive) override;
  void Hide() override;
  bool IsVisible() const override;

 private:
  // WaylandWindow overrides:
  void HandlePopupConfigure(const gfx::Rect& bounds) override;
  void OnCloseRequest() override;
  bool OnInitialize(PlatformWindowInitProperties properties) override;

  // Creates a popup window, which is visible as a menu window.
  bool CreateShellPopup();

  // Returns bounds with origin relative to parent window's origin.
  gfx::Rect AdjustPopupWindowPosition();

  // Wrappers around xdg v5 and xdg v6 objects. WaylandPopup doesn't
  // know anything about the version.
  std::unique_ptr<ShellPopupWrapper> shell_popup_;

  DISALLOW_COPY_AND_ASSIGN(WaylandPopup);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_POPUP_H_
