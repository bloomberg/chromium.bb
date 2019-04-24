// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/debug_commands.h"

#include "ash/accelerators/accelerator_commands.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/toast/toast_data.h"
#include "ash/system/toast/toast_manager.h"
#include "ash/touch/touch_devices_controller.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wm/focus_rules.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/widget_finder.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/ws/window_lookup.h"
#include "ash/ws/window_service_owner.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/window_service.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/debug_utils.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/views/debug_utils.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_properties.h"

namespace ash {
namespace debug {
namespace {

void HandlePrintLayerHierarchy() {
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    ui::Layer* layer = root->layer();
    if (layer)
      ui::PrintLayerHierarchy(
          layer,
          RootWindowController::ForWindow(root)->GetLastMouseLocationInRoot());
  }

  if (!features::IsSingleProcessMash())
    return;

  for (aura::Window* mus_root :
       views::MusClient::Get()->window_tree_client()->GetRoots()) {
    ui::Layer* layer = mus_root->layer();
    if (layer)
      ui::PrintLayerHierarchy(layer, gfx::Point());
  }
}

void HandlePrintViewHierarchy() {
  aura::Window* active_window = wm::GetActiveWindow();
  if (!active_window)
    return;
  views::Widget* widget = GetInternalWidgetForWindow(active_window);
  if (!widget)
    return;
  views::PrintViewHierarchy(widget->GetRootView());
}

void PrintWindowHierarchy(ws::WindowService* window_service,
                          const aura::Window* active_window,
                          const aura::Window* focused_window,
                          aura::Window* window,
                          int indent,
                          std::ostringstream* out) {
  std::string indent_str(indent, ' ');
  std::string name(window->GetName());
  if (name.empty())
    name = "\"\"";
  const gfx::Vector2dF& subpixel_position_offset =
      window->layer()->subpixel_position_offset();
  *out << indent_str;
  if (window_service && ws::WindowService::IsProxyWindow(window))
    *out << " [proxy] id=" << window_service->GetIdForDebugging(window) << " ";
  *out << name << " (" << window << ")"
       << " type=" << window->type();
  if (ash::IsToplevelWindow(window))
    *out << " " << wm::GetWindowState(window)->GetStateType();
  *out << ((window == active_window) ? " [active]" : "")
       << ((window == focused_window) ? " [focused]" : "")
       << (window->IsVisible() ? " visible" : "") << " "
       << window->bounds().ToString();
  if (window->GetProperty(::wm::kSnapChildrenToPixelBoundary))
    *out << " [snapped]";
  if (!subpixel_position_offset.IsZero())
    *out << " subpixel offset=" + subpixel_position_offset.ToString();
  if (features::IsSingleProcessMash()) {
    aura::Window* proxy_window =
        window_lookup::GetProxyWindowForClientWindow(window);
    if (proxy_window)
      *out << " id=" << window_service->GetIdForDebugging(proxy_window);
  }
  std::string* tree_id = window->GetProperty(ui::kChildAXTreeID);
  if (tree_id)
    *out << " ax_tree_id=" << *tree_id;
  *out << '\n';

  for (aura::Window* child : window->children()) {
    PrintWindowHierarchy(window_service, active_window, focused_window, child,
                         indent + 3, out);
  }
}

void HandlePrintWindowHierarchy() {
  aura::Window* active_window = wm::GetActiveWindow();
  aura::Window* focused_window = wm::GetFocusedWindow();
  aura::Window::Windows roots = Shell::Get()->GetAllRootWindows();
  ws::WindowService* window_service =
      Shell::Get()->window_service_owner()->window_service();
  for (size_t i = 0; i < roots.size(); ++i) {
    std::ostringstream out;
    out << "RootWindow " << i << ":\n";
    PrintWindowHierarchy(window_service, active_window, focused_window,
                         roots[i], 0, &out);
    // Error so logs can be collected from end-users.
    LOG(ERROR) << out.str();
  }

  if (!features::IsSingleProcessMash())
    return;

  for (aura::Window* mus_root :
       views::MusClient::Get()->window_tree_client()->GetRoots()) {
    std::ostringstream out;
    out << "WindowService Client RootWindow\n";
    PrintWindowHierarchy(nullptr, nullptr, nullptr, mus_root, 0, &out);
    // Error so logs can be collected from end-users.
    LOG(ERROR) << out.str();
  }
}

gfx::ImageSkia CreateWallpaperImage(SkColor fill, SkColor rect) {
  // TODO(oshima): Consider adding a command line option to control wallpaper
  // images for testing. The size is randomly picked.
  gfx::Size image_size(1366, 768);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(image_size.width(), image_size.height(), true);
  SkCanvas canvas(bitmap);
  canvas.drawColor(fill);
  SkPaint paint;
  paint.setColor(rect);
  paint.setStrokeWidth(10);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setBlendMode(SkBlendMode::kSrcOver);
  canvas.drawRoundRect(gfx::RectToSkRect(gfx::Rect(image_size)), 100.f, 100.f,
                       paint);
  return gfx::ImageSkia(gfx::ImageSkiaRep(std::move(bitmap), 1.f));
}

void HandleToggleWallpaperMode() {
  static int index = 0;
  WallpaperController* wallpaper_controller =
      Shell::Get()->wallpaper_controller();
  WallpaperInfo info("", WALLPAPER_LAYOUT_STRETCH, DEFAULT,
                     base::Time::Now().LocalMidnight());
  switch (++index % 4) {
    case 0:
      wallpaper_controller->ShowDefaultWallpaperForTesting();
      break;
    case 1:
      wallpaper_controller->ShowWallpaperImage(
          CreateWallpaperImage(SK_ColorRED, SK_ColorBLUE), info,
          /*preview_mode=*/false, /*always_on_top=*/false);
      break;
    case 2:
      info.layout = WALLPAPER_LAYOUT_CENTER;
      wallpaper_controller->ShowWallpaperImage(
          CreateWallpaperImage(SK_ColorBLUE, SK_ColorGREEN), info,
          /*preview_mode=*/false, /*always_on_top=*/false);
      break;
    case 3:
      info.layout = WALLPAPER_LAYOUT_CENTER_CROPPED;
      wallpaper_controller->ShowWallpaperImage(
          CreateWallpaperImage(SK_ColorGREEN, SK_ColorRED), info,
          /*preview_mode=*/false, /*always_on_top=*/false);
      break;
  }
}

void HandleToggleTouchpad() {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Touchpad"));
  Shell::Get()->touch_devices_controller()->ToggleTouchpad();
}

void HandleToggleTouchscreen() {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Touchscreen"));
  TouchDevicesController* controller = Shell::Get()->touch_devices_controller();
  controller->SetTouchscreenEnabled(
      !controller->GetTouchscreenEnabled(TouchDeviceEnabledSource::USER_PREF),
      TouchDeviceEnabledSource::USER_PREF);
}

void HandleToggleTabletMode() {
  TabletModeController* controller = Shell::Get()->tablet_mode_controller();
  controller->EnableTabletModeWindowManager(
      !controller->IsTabletModeWindowManagerEnabled());
}

void HandleTriggerCrash() {
  LOG(FATAL) << "Intentional crash via debug accelerator.";
}

}  // namespace

void PrintUIHierarchies() {
  // This is a separate command so the user only has to hit one key to generate
  // all the logs. Developers use the individual dumps repeatedly, so keep
  // those as separate commands to avoid spamming their logs.
  HandlePrintLayerHierarchy();
  HandlePrintWindowHierarchy();
  HandlePrintViewHierarchy();
}

bool DebugAcceleratorsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshDebugShortcuts);
}

bool DeveloperAcceleratorsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshDeveloperShortcuts);
}

void PerformDebugActionIfEnabled(AcceleratorAction action) {
  if (!DebugAcceleratorsEnabled())
    return;

  switch (action) {
    case DEBUG_PRINT_LAYER_HIERARCHY:
      HandlePrintLayerHierarchy();
      break;
    case DEBUG_PRINT_VIEW_HIERARCHY:
      HandlePrintViewHierarchy();
      break;
    case DEBUG_PRINT_WINDOW_HIERARCHY:
      HandlePrintWindowHierarchy();
      break;
    case DEBUG_SHOW_TOAST:
      Shell::Get()->toast_manager()->Show(
          ToastData("id", base::ASCIIToUTF16("Toast"), 5000 /* duration_ms */,
                    base::ASCIIToUTF16("Dismiss")));
      break;
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
      Shell::Get()->display_manager()->ToggleDisplayScaleFactor();
      break;
    case DEBUG_TOGGLE_TOUCH_PAD:
      HandleToggleTouchpad();
      break;
    case DEBUG_TOGGLE_TOUCH_SCREEN:
      HandleToggleTouchscreen();
      break;
    case DEBUG_TOGGLE_TABLET_MODE:
      HandleToggleTabletMode();
      break;
    case DEBUG_TOGGLE_WALLPAPER_MODE:
      HandleToggleWallpaperMode();
      break;
    case DEBUG_TRIGGER_CRASH:
      HandleTriggerCrash();
      break;
    default:
      break;
  }
}

}  // namespace debug
}  // namespace ash
