// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

namespace views {

DesktopWindowTreeHostLinux::DesktopWindowTreeHostLinux(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : DesktopWindowTreeHostPlatform(native_widget_delegate,
                                    desktop_native_widget_aura) {}

DesktopWindowTreeHostLinux::~DesktopWindowTreeHostLinux() = default;

void DesktopWindowTreeHostLinux::SetPendingXVisualId(int x_visual_id) {
  pending_x_visual_id_ = x_visual_id;
}

gfx::Size DesktopWindowTreeHostLinux::AdjustSizeForDisplay(
    const gfx::Size& requested_size_in_pixels) {
  std::vector<display::Display> displays =
      display::Screen::GetScreen()->GetAllDisplays();
  // Compare against all monitor sizes. The window manager can move the window
  // to whichever monitor it wants.
  for (const auto& display : displays) {
    if (requested_size_in_pixels == display.GetSizeInPixel()) {
      return gfx::Size(requested_size_in_pixels.width() - 1,
                       requested_size_in_pixels.height() - 1);
    }
  }

  // Do not request a 0x0 window size. It causes an XError.
  gfx::Size size_in_pixels = requested_size_in_pixels;
  size_in_pixels.SetToMax(gfx::Size(1, 1));
  return size_in_pixels;
}

void DesktopWindowTreeHostLinux::AddAdditionalInitProperties(
    const Widget::InitParams& params,
    ui::PlatformWindowInitProperties* properties) {
  // Calculate initial bounds
  gfx::Rect bounds_in_pixels = ToPixelRect(properties->bounds);
  gfx::Size adjusted_size = AdjustSizeForDisplay(bounds_in_pixels.size());
  bounds_in_pixels.set_size(adjusted_size);
  properties->bounds = bounds_in_pixels;

  // Set the background color on startup to make the initial flickering
  // happening between the XWindow is mapped and the first expose event
  // is completely handled less annoying. If possible, we use the content
  // window's background color, otherwise we fallback to white.
  base::Optional<int> background_color;
  const views::LinuxUI* linux_ui = views::LinuxUI::instance();
  if (linux_ui && content_window()) {
    ui::NativeTheme::ColorId target_color;
    switch (properties->type) {
      case ui::PlatformWindowType::kBubble:
        target_color = ui::NativeTheme::kColorId_BubbleBackground;
        break;
      case ui::PlatformWindowType::kTooltip:
        target_color = ui::NativeTheme::kColorId_TooltipBackground;
        break;
      default:
        target_color = ui::NativeTheme::kColorId_WindowBackground;
        break;
    }
    ui::NativeTheme* theme = linux_ui->GetNativeTheme(content_window());
    background_color = theme->GetSystemColor(target_color);
  }
  properties->prefer_dark_theme = linux_ui && linux_ui->PreferDarkTheme();
  properties->background_color = background_color;
  properties->icon = ViewsDelegate::GetInstance()->GetDefaultWindowIcon();

  properties->wm_class_name = params.wm_class_name;
  properties->wm_class_class = params.wm_class_class;
  properties->wm_role_name = params.wm_role_name;

  properties->x_visual_id = pending_x_visual_id_;
}

}  // namespace views
