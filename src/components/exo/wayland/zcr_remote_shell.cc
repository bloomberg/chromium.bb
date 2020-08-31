// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_remote_shell.h"

#include <remote-shell-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/work_area_insets.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/display.h"
#include "components/exo/input_method_surface.h"
#include "components/exo/notification_surface.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wm_helper_chromeos.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_types.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/activation_change_observer.h"

namespace exo {
namespace wayland {

namespace {

namespace switches {

// This flag can be used to emulate device scale factor for remote shell.
constexpr char kForceRemoteShellScale[] = "force-remote-shell-scale";

}  // namespace switches

// We don't send configure immediately after tablet mode switch
// because layout can change due to orientation lock state or accelerometer.
constexpr int kConfigureDelayAfterLayoutSwitchMs = 300;

// Convert to 8.24 fixed format.
int32_t To8_24Fixed(double value) {
  constexpr int kDecimalBits = 24;
  return static_cast<int32_t>(value * (1 << kDecimalBits));
}

uint32_t ResizeDirection(int component) {
  switch (component) {
    case HTCAPTION:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE;
    case HTTOP:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOP;
    case HTTOPRIGHT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPRIGHT;
    case HTRIGHT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_RIGHT;
    case HTBOTTOMRIGHT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMRIGHT;
    case HTBOTTOM:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOM;
    case HTBOTTOMLEFT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMLEFT;
    case HTLEFT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_LEFT;
    case HTTOPLEFT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPLEFT;
    default:
      LOG(ERROR) << "Unknown component:" << component;
      break;
  }
  NOTREACHED();
  return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE;
}

// Returns the scale factor to be used by remote shell clients.
double GetDefaultDeviceScaleFactor() {
  // A flag used by VM to emulate a device scale for a particular board.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceRemoteShellScale)) {
    std::string value =
        command_line->GetSwitchValueASCII(switches::kForceRemoteShellScale);
    double scale = 1.0;
    if (base::StringToDouble(value, &scale))
      return std::max(1.0, scale);
  }
  return WMHelper::GetInstance()->GetDefaultDeviceScaleFactor();
}

// Scale the |child_bounds| in such a way that if it should fill the
// |parent_size|'s width/height, it returns the |parent_size_in_pixel|'s
// width/height.
gfx::Rect ScaleBoundsToPixelSnappedToParent(
    const gfx::Size& parent_size_in_pixel,
    const gfx::Size& parent_size,
    float device_scale_factor,
    const gfx::Rect& child_bounds) {
  int right = child_bounds.right();
  int bottom = child_bounds.bottom();

  int new_x = gfx::ToRoundedInt(child_bounds.x() * device_scale_factor);
  int new_y = gfx::ToRoundedInt(child_bounds.y() * device_scale_factor);

  int new_right = right == parent_size.width()
                      ? parent_size_in_pixel.width()
                      : gfx::ToRoundedInt(right * device_scale_factor);

  int new_bottom = bottom == parent_size.height()
                       ? parent_size_in_pixel.height()
                       : gfx::ToRoundedInt(bottom * device_scale_factor);
  return gfx::Rect(new_x, new_y, new_right - new_x, new_bottom - new_y);
}

// Create the insets make sure that work area will be within the chrome's
// work area when converted to the pixel on client side.
gfx::Insets GetAdjustedInsets(const display::Display& display) {
  float scale = display.device_scale_factor();
  gfx::Size size_in_pixel = display.GetSizeInPixel();
  gfx::Rect work_area_in_display = display.work_area();
  work_area_in_display.Offset(-display.bounds().x(), -display.bounds().y());
  gfx::Rect work_area_in_pixel = ScaleBoundsToPixelSnappedToParent(
      size_in_pixel, display.bounds().size(), scale, work_area_in_display);
  gfx::Insets insets_in_pixel =
      gfx::Rect(size_in_pixel).InsetsFrom(work_area_in_pixel);
  return gfx::Insets(gfx::ToCeiledInt(insets_in_pixel.top() / scale),
                     gfx::ToCeiledInt(insets_in_pixel.left() / scale),
                     gfx::ToCeiledInt(insets_in_pixel.bottom() / scale),
                     gfx::ToCeiledInt(insets_in_pixel.right() / scale));
}

ash::ShelfLayoutManager* GetShelfLayoutManagerForDisplay(
    const display::Display& display) {
  auto* root = ash::Shell::GetRootWindowForDisplayId(display.id());
  return ash::Shelf::ForWindow(root)->shelf_layout_manager();
}

int Component(uint32_t direction) {
  switch (direction) {
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE:
      return HTNOWHERE;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOP:
      return HTTOP;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPRIGHT:
      return HTTOPRIGHT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_RIGHT:
      return HTRIGHT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMRIGHT:
      return HTBOTTOMRIGHT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOM:
      return HTBOTTOM;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMLEFT:
      return HTBOTTOMLEFT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_LEFT:
      return HTLEFT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPLEFT:
      return HTTOPLEFT;
    default:
      VLOG(2) << "Unknown direction:" << direction;
      break;
  }
  return HTNOWHERE;
}

uint32_t CaptionButtonMask(uint32_t mask) {
  uint32_t caption_button_icon_mask = 0;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_BACK)
    caption_button_icon_mask |= 1 << views::CAPTION_BUTTON_ICON_BACK;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_MENU)
    caption_button_icon_mask |= 1 << views::CAPTION_BUTTON_ICON_MENU;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_MINIMIZE)
    caption_button_icon_mask |= 1 << views::CAPTION_BUTTON_ICON_MINIMIZE;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_MAXIMIZE_RESTORE)
    caption_button_icon_mask |= 1
                                << views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_CLOSE)
    caption_button_icon_mask |= 1 << views::CAPTION_BUTTON_ICON_CLOSE;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_ZOOM)
    caption_button_icon_mask |= 1 << views::CAPTION_BUTTON_ICON_ZOOM;
  return caption_button_icon_mask;
}

void MaybeApplyCTSHack(int layout_mode,
                       const gfx::Size& size_in_pixel,
                       gfx::Insets* insets_in_client_pixel,
                       gfx::Insets* stable_insets_in_client_pixel) {
  constexpr int kBadBottomInsets = 90;
  if (layout_mode == ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET &&
      size_in_pixel.width() == 3000 && size_in_pixel.height() == 2000 &&
      stable_insets_in_client_pixel->bottom() == kBadBottomInsets) {
    stable_insets_in_client_pixel->set_bottom(kBadBottomInsets + 1);
    if (insets_in_client_pixel->bottom() == kBadBottomInsets)
      insets_in_client_pixel->set_bottom(kBadBottomInsets + 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
// remote_surface_interface:

SurfaceFrameType RemoteShellSurfaceFrameType(uint32_t frame_type) {
  switch (frame_type) {
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_NONE:
      return SurfaceFrameType::NONE;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_NORMAL:
      return SurfaceFrameType::NORMAL;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_SHADOW:
      return SurfaceFrameType::SHADOW;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_AUTOHIDE:
      return SurfaceFrameType::AUTOHIDE;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_OVERLAY:
      return SurfaceFrameType::OVERLAY;
    default:
      VLOG(2) << "Unknown remote-shell frame type: " << frame_type;
      return SurfaceFrameType::NONE;
  }
}

void remote_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void remote_surface_set_app_id(wl_client* client,
                               wl_resource* resource,
                               const char* app_id) {
  GetUserDataAs<ShellSurfaceBase>(resource)->SetApplicationId(app_id);
}

void remote_surface_set_window_geometry(wl_client* client,
                                        wl_resource* resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height) {
  GetUserDataAs<ShellSurfaceBase>(resource)->SetGeometry(
      gfx::Rect(x, y, width, height));
}

void remote_surface_set_orientation(wl_client* client,
                                    wl_resource* resource,
                                    int32_t orientation) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetOrientation(
      orientation == ZCR_REMOTE_SURFACE_V1_ORIENTATION_PORTRAIT
          ? Orientation::PORTRAIT
          : Orientation::LANDSCAPE);
}

void remote_surface_set_scale(wl_client* client,
                              wl_resource* resource,
                              wl_fixed_t scale) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetScale(
      wl_fixed_to_double(scale));
}

void remote_surface_set_rectangular_shadow_DEPRECATED(wl_client* client,
                                                      wl_resource* resource,
                                                      int32_t x,
                                                      int32_t y,
                                                      int32_t width,
                                                      int32_t height) {
  NOTIMPLEMENTED();
}

void remote_surface_set_rectangular_shadow_background_opacity_DEPRECATED(
    wl_client* client,
    wl_resource* resource,
    wl_fixed_t opacity) {
  NOTIMPLEMENTED();
}

void remote_surface_set_title(wl_client* client,
                              wl_resource* resource,
                              const char* title) {
  GetUserDataAs<ShellSurfaceBase>(resource)->SetTitle(
      base::string16(base::UTF8ToUTF16(title)));
}

void remote_surface_set_top_inset(wl_client* client,
                                  wl_resource* resource,
                                  int32_t height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetTopInset(height);
}

void remote_surface_activate(wl_client* client,
                             wl_resource* resource,
                             uint32_t serial) {
  ShellSurfaceBase* shell_surface = GetUserDataAs<ShellSurfaceBase>(resource);
  shell_surface->Activate();
}

void remote_surface_maximize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMaximized();
}

void remote_surface_minimize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMinimized();
}

void remote_surface_restore(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetRestored();
}

void remote_surface_fullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetFullscreen(true);
}

void remote_surface_unfullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetFullscreen(false);
}

void remote_surface_pin(wl_client* client,
                        wl_resource* resource,
                        int32_t trusted) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetPinned(
      trusted ? ash::WindowPinType::kTrustedPinned
              : ash::WindowPinType::kPinned);
}

void remote_surface_unpin(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetPinned(
      ash::WindowPinType::kNone);
}

void remote_surface_set_system_modal(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSystemModal(true);
}

void remote_surface_unset_system_modal(wl_client* client,
                                       wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSystemModal(false);
}

void remote_surface_set_rectangular_surface_shadow(wl_client* client,
                                                   wl_resource* resource,
                                                   int32_t x,
                                                   int32_t y,
                                                   int32_t width,
                                                   int32_t height) {
  ClientControlledShellSurface* shell_surface =
      GetUserDataAs<ClientControlledShellSurface>(resource);
  shell_surface->SetShadowBounds(gfx::Rect(x, y, width, height));
}

void remote_surface_set_systemui_visibility(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t visibility) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSystemUiVisibility(
      visibility != ZCR_REMOTE_SURFACE_V1_SYSTEMUI_VISIBILITY_STATE_VISIBLE);
}

void remote_surface_set_always_on_top(wl_client* client,
                                      wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetAlwaysOnTop(true);
}

void remote_surface_unset_always_on_top(wl_client* client,
                                        wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetAlwaysOnTop(false);
}

void remote_surface_ack_configure(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t serial) {
  // DEPRECATED
}

void remote_surface_move(wl_client* client, wl_resource* resource) {
  // DEPRECATED
}

void remote_surface_set_window_type(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t type) {
  auto* widget = GetUserDataAs<ShellSurfaceBase>(resource)->GetWidget();
  if (!widget)
    return;

  switch (type) {
    case ZCR_REMOTE_SURFACE_V1_WINDOW_TYPE_NORMAL:
      widget->GetNativeWindow()->SetProperty(ash::kHideInOverviewKey, false);
      break;
    case ZCR_REMOTE_SURFACE_V1_WINDOW_TYPE_SYSTEM_UI:
      // TODO(takise): Consider removing this as this window type was added for
      // the old assistant and is not longer used.
      widget->GetNativeWindow()->SetProperty(ash::kHideInOverviewKey, true);
      wm::SetWindowVisibilityAnimationType(
          widget->GetNativeWindow(), wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
      break;
    case ZCR_REMOTE_SURFACE_V1_WINDOW_TYPE_HIDDEN_IN_OVERVIEW:
      widget->GetNativeWindow()->SetProperty(ash::kHideInOverviewKey, true);
      break;
  }
}

void remote_surface_resize(wl_client* client, wl_resource* resource) {
  // DEPRECATED
}

void remote_surface_set_resize_outset(wl_client* client,
                                      wl_resource* resource,
                                      int32_t outset) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetResizeOutset(
      outset);
}

void remote_surface_start_move(wl_client* client,
                               wl_resource* resource,
                               int32_t x,
                               int32_t y) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->StartDrag(
      HTCAPTION, gfx::PointF(x, y));
}

void remote_surface_set_can_maximize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetCanMaximize(true);
}

void remote_surface_unset_can_maximize(wl_client* client,
                                       wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetCanMaximize(false);
}

void remote_surface_set_min_size(wl_client* client,
                                 wl_resource* resource,
                                 int32_t width,
                                 int32_t height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMinimumSize(
      gfx::Size(width, height));
}

void remote_surface_set_max_size(wl_client* client,
                                 wl_resource* resource,
                                 int32_t width,
                                 int32_t height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMaximumSize(
      gfx::Size(width, height));
}

void remote_surface_set_aspect_ratio(wl_client* client,
                                     wl_resource* resource,
                                     int32_t aspect_ratio_width,
                                     int32_t aspect_ratio_height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetAspectRatio(
      gfx::SizeF(aspect_ratio_width, aspect_ratio_height));
}

void remote_surface_set_snapped_to_left(wl_client* client,
                                        wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSnappedToLeft();
}

void remote_surface_set_snapped_to_right(wl_client* client,
                                         wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSnappedToRight();
}

void remote_surface_start_resize(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t direction,
                                 int32_t x,
                                 int32_t y) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->StartDrag(
      Component(direction), gfx::PointF(x, y));
}

void remote_surface_set_frame(wl_client* client,
                              wl_resource* resource,
                              uint32_t type) {
  ClientControlledShellSurface* shell_surface =
      GetUserDataAs<ClientControlledShellSurface>(resource);
  shell_surface->root_surface()->SetFrame(RemoteShellSurfaceFrameType(type));
}

void remote_surface_set_frame_buttons(wl_client* client,
                                      wl_resource* resource,
                                      uint32_t visible_button_mask,
                                      uint32_t enabled_button_mask) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetFrameButtons(
      CaptionButtonMask(visible_button_mask),
      CaptionButtonMask(enabled_button_mask));
}

void remote_surface_set_extra_title(wl_client* client,
                                    wl_resource* resource,
                                    const char* extra_title) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetExtraTitle(
      base::string16(base::UTF8ToUTF16(extra_title)));
}

ash::OrientationLockType OrientationLock(uint32_t orientation_lock) {
  switch (orientation_lock) {
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_NONE:
      return ash::OrientationLockType::kAny;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_CURRENT:
      return ash::OrientationLockType::kCurrent;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_PORTRAIT:
      return ash::OrientationLockType::kPortrait;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_LANDSCAPE:
      return ash::OrientationLockType::kLandscape;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_PORTRAIT_PRIMARY:
      return ash::OrientationLockType::kPortraitPrimary;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_PORTRAIT_SECONDARY:
      return ash::OrientationLockType::kPortraitSecondary;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_LANDSCAPE_PRIMARY:
      return ash::OrientationLockType::kLandscapePrimary;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_LANDSCAPE_SECONDARY:
      return ash::OrientationLockType::kLandscapeSecondary;
  }
  VLOG(2) << "Unexpected value of orientation_lock: " << orientation_lock;
  return ash::OrientationLockType::kAny;
}

void remote_surface_set_orientation_lock(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t orientation_lock) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetOrientationLock(
      OrientationLock(orientation_lock));
}

void remote_surface_pip(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetPip();
}

void remote_surface_set_bounds(wl_client* client,
                               wl_resource* resource,
                               uint32_t display_id_hi,
                               uint32_t display_id_lo,
                               int32_t x,
                               int32_t y,
                               int32_t width,
                               int32_t height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetBounds(
      static_cast<int64_t>(display_id_hi) << 32 | display_id_lo,
      gfx::Rect(x, y, width, height));
}

void remote_surface_block_ime(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetImeBlocked(true);
}

void remote_surface_unblock_ime(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetImeBlocked(false);
}

void remote_surface_set_accessibility_id(wl_client* client,
                                         wl_resource* resource,
                                         int32_t accessibility_id) {
  GetUserDataAs<ClientControlledShellSurface>(resource)
      ->SetClientAccessibilityId(accessibility_id);
}

void remote_surface_set_pip_original_window(wl_client* client,
                                            wl_resource* resource) {
  auto* widget = GetUserDataAs<ShellSurfaceBase>(resource)->GetWidget();
  if (!widget) {
    LOG(ERROR) << "no widget found for setting pip original window";
    return;
  }

  widget->GetNativeWindow()->SetProperty(ash::kPipOriginalWindowKey, true);
}

void remote_surface_unset_pip_original_window(wl_client* client,
                                              wl_resource* resource) {
  auto* widget = GetUserDataAs<ShellSurfaceBase>(resource)->GetWidget();
  if (!widget) {
    LOG(ERROR) << "no widget found for unsetting pip original window";
    return;
  }

  widget->GetNativeWindow()->SetProperty(ash::kPipOriginalWindowKey, false);
}

void remote_surface_set_system_gesture_exclusion(wl_client* client,
                                                 wl_resource* resource,
                                                 wl_resource* region_resource) {
  auto* widget = GetUserDataAs<ShellSurfaceBase>(resource)->GetWidget();
  if (!widget) {
    LOG(ERROR) << "no widget found for setting system gesture exclusion";
    return;
  }

  if (region_resource) {
    widget->GetNativeWindow()->SetProperty(
        ash::kSystemGestureExclusionKey,
        new SkRegion(*GetUserDataAs<SkRegion>(region_resource)));
  } else {
    widget->GetNativeWindow()->ClearProperty(ash::kSystemGestureExclusionKey);
  }
}

const struct zcr_remote_surface_v1_interface remote_surface_implementation = {
    remote_surface_destroy,
    remote_surface_set_app_id,
    remote_surface_set_window_geometry,
    remote_surface_set_scale,
    remote_surface_set_rectangular_shadow_DEPRECATED,
    remote_surface_set_rectangular_shadow_background_opacity_DEPRECATED,
    remote_surface_set_title,
    remote_surface_set_top_inset,
    remote_surface_activate,
    remote_surface_maximize,
    remote_surface_minimize,
    remote_surface_restore,
    remote_surface_fullscreen,
    remote_surface_unfullscreen,
    remote_surface_pin,
    remote_surface_unpin,
    remote_surface_set_system_modal,
    remote_surface_unset_system_modal,
    remote_surface_set_rectangular_surface_shadow,
    remote_surface_set_systemui_visibility,
    remote_surface_set_always_on_top,
    remote_surface_unset_always_on_top,
    remote_surface_ack_configure,
    remote_surface_move,
    remote_surface_set_orientation,
    remote_surface_set_window_type,
    remote_surface_resize,
    remote_surface_set_resize_outset,
    remote_surface_start_move,
    remote_surface_set_can_maximize,
    remote_surface_unset_can_maximize,
    remote_surface_set_min_size,
    remote_surface_set_max_size,
    remote_surface_set_snapped_to_left,
    remote_surface_set_snapped_to_right,
    remote_surface_start_resize,
    remote_surface_set_frame,
    remote_surface_set_frame_buttons,
    remote_surface_set_extra_title,
    remote_surface_set_orientation_lock,
    remote_surface_pip,
    remote_surface_set_bounds,
    remote_surface_set_aspect_ratio,
    remote_surface_block_ime,
    remote_surface_unblock_ime,
    remote_surface_set_accessibility_id,
    remote_surface_set_pip_original_window,
    remote_surface_unset_pip_original_window,
    remote_surface_set_system_gesture_exclusion};

////////////////////////////////////////////////////////////////////////////////
// notification_surface_interface:

void notification_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void notification_surface_set_app_id(wl_client* client,
                                     wl_resource* resource,
                                     const char* app_id) {
  GetUserDataAs<NotificationSurface>(resource)->SetApplicationId(app_id);
}

const struct zcr_notification_surface_v1_interface
    notification_surface_implementation = {notification_surface_destroy,
                                           notification_surface_set_app_id};

////////////////////////////////////////////////////////////////////////////////
// input_method_surface_interface:

void input_method_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void input_method_surface_set_bounds(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t display_id_hi,
                                     uint32_t display_id_lo,
                                     int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height) {
  GetUserDataAs<InputMethodSurface>(resource)->SetBounds(
      static_cast<int64_t>(display_id_hi) << 32 | display_id_lo,
      gfx::Rect(x, y, width, height));
}

const struct zcr_input_method_surface_v1_interface
    input_method_surface_implementation = {input_method_surface_destroy,
                                           input_method_surface_set_bounds};

////////////////////////////////////////////////////////////////////////////////
// remote_shell_interface:

// Implements remote shell interface and monitors workspace state needed
// for the remote shell interface.
class WaylandRemoteShell : public ash::TabletModeObserver,
                           public wm::ActivationChangeObserver,
                           public display::DisplayObserver {
 public:
  WaylandRemoteShell(Display* display, wl_resource* remote_shell_resource)
      : display_(display), remote_shell_resource_(remote_shell_resource) {
    WMHelperChromeOS* helper = WMHelperChromeOS::GetInstance();
    helper->AddTabletModeObserver(this);
    helper->AddActivationObserver(this);
    display::Screen::GetScreen()->AddObserver(this);

    layout_mode_ = helper->InTabletMode()
                       ? ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET
                       : ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;

    if (wl_resource_get_version(remote_shell_resource_) >= 8) {
      double scale_factor = GetDefaultDeviceScaleFactor();
      int32_t fixed_scale = To8_24Fixed(scale_factor);
      zcr_remote_shell_v1_send_default_device_scale_factor(
          remote_shell_resource_, fixed_scale);
    }

    SendDisplayMetrics();
    SendActivated(helper->GetActiveWindow(), nullptr);
  }
  ~WaylandRemoteShell() override {
    WMHelperChromeOS* helper = WMHelperChromeOS::GetInstance();
    helper->RemoveTabletModeObserver(this);
    helper->RemoveActivationObserver(this);
    display::Screen::GetScreen()->RemoveObserver(this);
  }

  std::unique_ptr<ClientControlledShellSurface> CreateShellSurface(
      Surface* surface,
      int container,
      double default_device_scale_factor) {
    return display_->CreateClientControlledShellSurface(
        surface, container, default_device_scale_factor);
  }

  std::unique_ptr<NotificationSurface> CreateNotificationSurface(
      Surface* surface,
      const std::string& notification_key) {
    return display_->CreateNotificationSurface(surface, notification_key);
  }

  std::unique_ptr<InputMethodSurface> CreateInputMethodSurface(
      Surface* surface,
      double default_device_scale_factor) {
    return display_->CreateInputMethodSurface(surface,
                                              default_device_scale_factor);
  }

  // TODO(mukai, oshima): rewrite this through delegate-style instead of
  // creating callbacks.
  ClientControlledShellSurface::BoundsChangedCallback
  CreateBoundsChangedCallback(wl_resource* resource) {
    return base::BindRepeating(
        &WaylandRemoteShell::HandleRemoteSurfaceBoundsChangedCallback,
        weak_ptr_factory_.GetWeakPtr(), base::Unretained(resource));
  }

  ClientControlledShellSurface::ChangeZoomLevelCallback
  CreateChangeZoomLevelCallback(wl_resource* resource) {
    return base::BindRepeating(
        &WaylandRemoteShell::HandleRemoteSurfaceChangeZoomLevelCallback,
        weak_ptr_factory_.GetWeakPtr(), base::Unretained(resource));
  }

  ClientControlledShellSurface::StateChangedCallback CreateStateChangedCallback(
      wl_resource* resource) {
    return base::BindRepeating(
        &WaylandRemoteShell::HandleRemoteSurfaceStateChangedCallback,
        weak_ptr_factory_.GetWeakPtr(), base::Unretained(resource));
  }

  ClientControlledShellSurface::GeometryChangedCallback
  CreateGeometryChangedCallback(wl_resource* resource) {
    return base::BindRepeating(
        &WaylandRemoteShell::HandleRemoteSurfaceGeometryChangedCallback,
        weak_ptr_factory_.GetWeakPtr(), base::Unretained(resource));
  }

  // Overridden from display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {
    ScheduleSendDisplayMetrics(0);
  }

  void OnDisplayRemoved(const display::Display& old_display) override {
    ScheduleSendDisplayMetrics(0);
  }

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    // No need to update when a primary display has changed without bounds
    // change. See WaylandDisplayObserver::OnDisplayMetricsChanged
    // for more details.
    if (changed_metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
         DISPLAY_METRIC_ROTATION | DISPLAY_METRIC_WORK_AREA)) {
      ScheduleSendDisplayMetrics(0);
    }
  }

  // Overridden from ash::TabletModeObserver:
  void OnTabletModeStarted() override {
    layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET;
    ScheduleSendDisplayMetrics(kConfigureDelayAfterLayoutSwitchMs);
  }
  void OnTabletModeEnding() override {
    layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;
    ScheduleSendDisplayMetrics(kConfigureDelayAfterLayoutSwitchMs);
  }
  void OnTabletModeEnded() override {}

  // Overridden from wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    SendActivated(gained_active, lost_active);
  }

 private:
  void ScheduleSendDisplayMetrics(int delay_ms) {
    needs_send_display_metrics_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WaylandRemoteShell::SendDisplayMetrics,
                       weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(delay_ms));
  }

  // Returns the transform that a display's output is currently adjusted for.
  wl_output_transform DisplayTransform(display::Display::Rotation rotation) {
    switch (rotation) {
      case display::Display::ROTATE_0:
        return WL_OUTPUT_TRANSFORM_NORMAL;
      case display::Display::ROTATE_90:
        return WL_OUTPUT_TRANSFORM_90;
      case display::Display::ROTATE_180:
        return WL_OUTPUT_TRANSFORM_180;
      case display::Display::ROTATE_270:
        return WL_OUTPUT_TRANSFORM_270;
    }
    NOTREACHED();
    return WL_OUTPUT_TRANSFORM_NORMAL;
  }

  void SendDisplayMetrics() {
    if (!needs_send_display_metrics_)
      return;
    needs_send_display_metrics_ = false;

    const display::Screen* screen = display::Screen::GetScreen();
    double default_dsf = GetDefaultDeviceScaleFactor();

    for (const auto& display : screen->GetAllDisplays()) {
      const gfx::Rect& bounds = display.bounds();

      double device_scale_factor = display.device_scale_factor();

      uint32_t display_id_hi = static_cast<uint32_t>(display.id() >> 32);
      uint32_t display_id_lo = static_cast<uint32_t>(display.id());
      gfx::Size size_in_pixel = display.GetSizeInPixel();

      wl_array data;
      wl_array_init(&data);

      const auto& bytes =
          WMHelper::GetInstance()->GetDisplayIdentificationData(display.id());
      for (uint8_t byte : bytes) {
        uint8_t* ptr =
            static_cast<uint8_t*>(wl_array_add(&data, sizeof(uint8_t)));
        DCHECK(ptr);
        *ptr = byte;
      }

      if (wl_resource_get_version(remote_shell_resource_) >= 20) {
        auto* shelf_layout_manager = GetShelfLayoutManagerForDisplay(display);

        // Apply the scale factor used on the remote shell client (ARC).
        const gfx::Rect& bounds = display.bounds();

        // Note: The origin is used just to identify the workspace on the client
        // side, and does not account the actual pixel size of other workspace
        // on the client side.
        int x_px = gfx::ToRoundedInt(bounds.x() * default_dsf);
        int y_px = gfx::ToRoundedInt(bounds.y() * default_dsf);

        float server_to_client_pixel_scale = default_dsf / device_scale_factor;

        gfx::Size size_in_client_pixel = gfx::ScaleToRoundedSize(
            size_in_pixel, server_to_client_pixel_scale);

        gfx::Insets insets_in_client_pixel = GetWorkAreaInsetsInClientPixel(
            display, default_dsf, size_in_client_pixel, display.work_area());

        gfx::Insets stable_insets_in_client_pixel =
            GetWorkAreaInsetsInClientPixel(display, default_dsf,
                                           size_in_client_pixel,
                                           GetStableWorkArea(display));

        // TODO(b/148977363): Fix the issue and remove the hack.
        MaybeApplyCTSHack(layout_mode_, size_in_pixel, &insets_in_client_pixel,
                          &stable_insets_in_client_pixel);

        int systemui_visibility =
            shelf_layout_manager->visibility_state() == ash::SHELF_AUTO_HIDE
                ? ZCR_REMOTE_SURFACE_V1_SYSTEMUI_VISIBILITY_STATE_AUTOHIDE_NON_STICKY
                : ZCR_REMOTE_SURFACE_V1_SYSTEMUI_VISIBILITY_STATE_VISIBLE;

        zcr_remote_shell_v1_send_workspace_info(
            remote_shell_resource_, display_id_hi, display_id_lo, x_px, y_px,
            size_in_client_pixel.width(), size_in_client_pixel.height(),
            insets_in_client_pixel.left(), insets_in_client_pixel.top(),
            insets_in_client_pixel.right(), insets_in_client_pixel.bottom(),
            stable_insets_in_client_pixel.left(),
            stable_insets_in_client_pixel.top(),
            stable_insets_in_client_pixel.right(),
            stable_insets_in_client_pixel.bottom(), systemui_visibility,
            DisplayTransform(display.rotation()), display.IsInternal(), &data);
      } else {
        const gfx::Insets& insets = GetAdjustedInsets(display);
        zcr_remote_shell_v1_send_workspace(
            remote_shell_resource_, display_id_hi, display_id_lo, bounds.x(),
            bounds.y(), bounds.width(), bounds.height(), insets.left(),
            insets.top(), insets.right(), insets.bottom(),
            DisplayTransform(display.rotation()),
            wl_fixed_from_double(device_scale_factor), display.IsInternal());

        if (wl_resource_get_version(remote_shell_resource_) == 19) {
          zcr_remote_shell_v1_send_display_info(
              remote_shell_resource_, display_id_hi, display_id_lo,
              size_in_pixel.width(), size_in_pixel.height(), &data);
        }
      }

      wl_array_release(&data);
    }

    zcr_remote_shell_v1_send_configure(remote_shell_resource_, layout_mode_);

    base::flat_set<wl_client*> clients;
    clients.insert(wl_resource_get_client(remote_shell_resource_));

    for (const auto& bounds_change : pending_bounds_changes_) {
      SendBoundsChanged(bounds_change.first, bounds_change.second.display_id,
                        bounds_change.second.bounds_in_display,
                        bounds_change.second.reason);
      clients.insert(wl_resource_get_client(bounds_change.first));
    }
    pending_bounds_changes_.clear();

    for (auto* client : clients)
      wl_client_flush(client);
  }

  void SendActivated(aura::Window* gained_active, aura::Window* lost_active) {
    Surface* gained_active_surface =
        gained_active ? GetShellMainSurface(gained_active) : nullptr;
    Surface* lost_active_surface =
        lost_active ? GetShellMainSurface(lost_active) : nullptr;
    wl_resource* gained_active_surface_resource =
        gained_active_surface ? GetSurfaceResource(gained_active_surface)
                              : nullptr;
    wl_resource* lost_active_surface_resource =
        lost_active_surface ? GetSurfaceResource(lost_active_surface) : nullptr;

    wl_client* client = wl_resource_get_client(remote_shell_resource_);

    // If surface that gained active is not owned by remote shell client then
    // set it to null.
    if (gained_active_surface_resource &&
        wl_resource_get_client(gained_active_surface_resource) != client) {
      gained_active_surface_resource = nullptr;
    }

    // If surface that lost active is not owned by remote shell client then
    // set it to null.
    if (lost_active_surface_resource &&
        wl_resource_get_client(lost_active_surface_resource) != client) {
      lost_active_surface_resource = nullptr;
    }

    zcr_remote_shell_v1_send_activated(remote_shell_resource_,
                                       gained_active_surface_resource,
                                       lost_active_surface_resource);
    wl_client_flush(client);
  }

  void HandleRemoteSurfaceBoundsChangedCallback(
      wl_resource* resource,
      ash::WindowStateType current_state,
      ash::WindowStateType requested_state,
      int64_t display_id,
      const gfx::Rect& bounds_in_display,
      bool resize,
      int bounds_change) {
    zcr_remote_surface_v1_bounds_change_reason reason =
        ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_RESIZE;
    if (!resize) {
      reason = current_state == ash::WindowStateType::kPip
                   ? ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_MOVE_PIP
                   : ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_MOVE;
    }
    if (bounds_change & ash::WindowResizer::kBoundsChange_Resizes) {
      reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_DRAG_RESIZE;
    } else if (bounds_change & ash::WindowResizer::kBoundsChange_Repositions) {
      reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_DRAG_MOVE;
    }
    // Override the reason only if the window enters snapped mode. If the window
    // resizes by dragging in snapped mode, we need to keep the original reason.
    if (requested_state != current_state) {
      if (requested_state == ash::WindowStateType::kLeftSnapped) {
        reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_SNAP_TO_LEFT;
      } else if (requested_state == ash::WindowStateType::kRightSnapped) {
        reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_SNAP_TO_RIGHT;
      }
    }
    if (wl_resource_get_version(resource) >= 22) {
      if (needs_send_display_metrics_) {
        pending_bounds_changes_.emplace(
            std::make_pair<wl_resource*, BoundsChangeData>(
                std::move(resource),
                BoundsChangeData(display_id, bounds_in_display, reason)));
        return;
      }
      SendBoundsChanged(resource, display_id, bounds_in_display, reason);
    } else {
      gfx::Rect bounds_in_screen = gfx::Rect(bounds_in_display);
      display::Display display;
      display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id,
                                                            &display);
      // The display ID should be valid.
      DCHECK(display.is_valid());
      if (display.is_valid())
        bounds_in_screen.Offset(display.bounds().OffsetFromOrigin());
      else
        LOG(ERROR) << "Invalid Display in send_bounds_changed:" << display_id;

      zcr_remote_surface_v1_send_bounds_changed(
          resource, static_cast<uint32_t>(display_id >> 32),
          static_cast<uint32_t>(display_id), bounds_in_screen.x(),
          bounds_in_screen.y(), bounds_in_screen.width(),
          bounds_in_screen.height(), reason);
    }
    wl_client_flush(wl_resource_get_client(resource));
  }

  void SendBoundsChanged(wl_resource* resource,
                         int64_t display_id,
                         const gfx::Rect& bounds_in_display,
                         zcr_remote_surface_v1_bounds_change_reason reason) {
    zcr_remote_surface_v1_send_bounds_changed(
        resource, static_cast<uint32_t>(display_id >> 32),
        static_cast<uint32_t>(display_id), bounds_in_display.x(),
        bounds_in_display.y(), bounds_in_display.width(),
        bounds_in_display.height(), reason);
  }

  void HandleRemoteSurfaceStateChangedCallback(
      wl_resource* resource,
      ash::WindowStateType old_state_type,
      ash::WindowStateType new_state_type) {
    DCHECK_NE(old_state_type, new_state_type);
    LOG_IF(ERROR, pending_bounds_changes_.count(resource) > 0)
        << "Sending window state while there is a pending bounds change. This "
           "should not happen.";

    uint32_t state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_NORMAL;
    switch (new_state_type) {
      case ash::WindowStateType::kMinimized:
        state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_MINIMIZED;
        break;
      case ash::WindowStateType::kMaximized:
        state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_MAXIMIZED;
        break;
      case ash::WindowStateType::kFullscreen:
        state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_FULLSCREEN;
        break;
      case ash::WindowStateType::kPinned:
        state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_PINNED;
        break;
      case ash::WindowStateType::kTrustedPinned:
        state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_TRUSTED_PINNED;
        break;
      case ash::WindowStateType::kLeftSnapped:
        state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_LEFT_SNAPPED;
        break;
      case ash::WindowStateType::kRightSnapped:
        state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_RIGHT_SNAPPED;
        break;
      case ash::WindowStateType::kPip:
        state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_PIP;
        break;
      default:
        break;
    }

    zcr_remote_surface_v1_send_state_type_changed(resource, state_type);
    wl_client_flush(wl_resource_get_client(resource));
  }

  void HandleRemoteSurfaceChangeZoomLevelCallback(wl_resource* resource,
                                                  ZoomChange change) {
    int32_t value = 0;
    switch (change) {
      case ZoomChange::IN:
        value = ZCR_REMOTE_SURFACE_V1_ZOOM_CHANGE_IN;
        break;
      case ZoomChange::OUT:
        value = ZCR_REMOTE_SURFACE_V1_ZOOM_CHANGE_OUT;
        break;
      case ZoomChange::RESET:
        value = ZCR_REMOTE_SURFACE_V1_ZOOM_CHANGE_RESET;
        break;
    }
    zcr_remote_surface_v1_send_change_zoom_level(resource, value);
  }

  void HandleRemoteSurfaceGeometryChangedCallback(wl_resource* resource,
                                                  const gfx::Rect& geometry) {
    LOG_IF(ERROR, pending_bounds_changes_.count(resource) > 0)
        << "Sending the new window geometry while there is a pending bounds "
           "change. This should not happen.";
    zcr_remote_surface_v1_send_window_geometry_changed(
        resource, geometry.x(), geometry.y(), geometry.width(),
        geometry.height());
    wl_client_flush(wl_resource_get_client(resource));
  }

  struct BoundsChangeData {
    int64_t display_id;
    gfx::Rect bounds_in_display;
    zcr_remote_surface_v1_bounds_change_reason reason;
    BoundsChangeData(int64_t display_id,
                     const gfx::Rect& bounds,
                     zcr_remote_surface_v1_bounds_change_reason reason)
        : display_id(display_id), bounds_in_display(bounds), reason(reason) {}
  };

  // The exo display instance. Not owned.
  Display* const display_;

  // The remote shell resource associated with observer.
  wl_resource* const remote_shell_resource_;

  bool needs_send_display_metrics_ = true;

  int layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;

  base::flat_map<wl_resource*, BoundsChangeData> pending_bounds_changes_;

  base::WeakPtrFactory<WaylandRemoteShell> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WaylandRemoteShell);
};

void remote_shell_destroy(wl_client* client, wl_resource* resource) {
  // Nothing to do here.
}

int RemoteSurfaceContainer(uint32_t container) {
  switch (container) {
    case ZCR_REMOTE_SHELL_V1_CONTAINER_DEFAULT:
      return ash::desks_util::GetActiveDeskContainerId();
    case ZCR_REMOTE_SHELL_V1_CONTAINER_OVERLAY:
      return ash::kShellWindowId_SystemModalContainer;
    default:
      DLOG(WARNING) << "Unsupported container: " << container;
      return ash::desks_util::GetActiveDeskContainerId();
  }
}

void HandleRemoteSurfaceCloseCallback(wl_resource* resource) {
  zcr_remote_surface_v1_send_close(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleRemoteSurfaceDragStartedCallback(wl_resource* resource,
                                            int component) {
  zcr_remote_surface_v1_send_drag_started(resource, ResizeDirection(component));
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleRemoteSurfaceDragFinishedCallback(wl_resource* resource,
                                             int x,
                                             int y,
                                             bool canceled) {
  zcr_remote_surface_v1_send_drag_finished(resource, x, y, canceled ? 1 : 0);
  wl_client_flush(wl_resource_get_client(resource));
}

void remote_shell_get_remote_surface(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t id,
                                     wl_resource* surface,
                                     uint32_t container) {
  WaylandRemoteShell* shell = GetUserDataAs<WaylandRemoteShell>(resource);
  double default_scale_factor = wl_resource_get_version(resource) >= 8
                                    ? GetDefaultDeviceScaleFactor()
                                    : 1.0;

  std::unique_ptr<ClientControlledShellSurface> shell_surface =
      shell->CreateShellSurface(GetUserDataAs<Surface>(surface),
                                RemoteSurfaceContainer(container),
                                default_scale_factor);
  if (!shell_surface) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  wl_resource* remote_surface_resource =
      wl_resource_create(client, &zcr_remote_surface_v1_interface,
                         wl_resource_get_version(resource), id);

  if (wl_resource_get_version(remote_surface_resource) < 18)
    shell_surface->set_server_reparent_window(true);

  shell_surface->set_close_callback(
      base::BindRepeating(&HandleRemoteSurfaceCloseCallback,
                          base::Unretained(remote_surface_resource)));
  shell_surface->set_state_changed_callback(
      shell->CreateStateChangedCallback(remote_surface_resource));
  shell_surface->set_geometry_changed_callback(
      shell->CreateGeometryChangedCallback(remote_surface_resource));
  shell_surface->set_surface_destroyed_callback(base::BindOnce(
      &wl_resource_destroy, base::Unretained(remote_surface_resource)));

  DCHECK(wl_resource_get_version(remote_surface_resource) >= 10);
  shell_surface->set_bounds_changed_callback(
      shell->CreateBoundsChangedCallback(remote_surface_resource));
  shell_surface->set_drag_started_callback(
      base::BindRepeating(&HandleRemoteSurfaceDragStartedCallback,
                          base::Unretained(remote_surface_resource)));
  shell_surface->set_drag_finished_callback(
      base::BindRepeating(&HandleRemoteSurfaceDragFinishedCallback,
                          base::Unretained(remote_surface_resource)));

  if (wl_resource_get_version(remote_surface_resource) >= 23) {
    shell_surface->set_change_zoom_level_callback(
        shell->CreateChangeZoomLevelCallback(remote_surface_resource));
  }

  SetImplementation(remote_surface_resource, &remote_surface_implementation,
                    std::move(shell_surface));
}

void remote_shell_get_notification_surface(wl_client* client,
                                           wl_resource* resource,
                                           uint32_t id,
                                           wl_resource* surface,
                                           const char* notification_key) {
  if (GetUserDataAs<Surface>(surface)->HasSurfaceDelegate()) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  std::unique_ptr<NotificationSurface> notification_surface =
      GetUserDataAs<WaylandRemoteShell>(resource)->CreateNotificationSurface(
          GetUserDataAs<Surface>(surface), std::string(notification_key));
  if (!notification_surface) {
    wl_resource_post_error(resource,
                           ZCR_REMOTE_SHELL_V1_ERROR_INVALID_NOTIFICATION_KEY,
                           "invalid notification key");
    return;
  }

  wl_resource* notification_surface_resource =
      wl_resource_create(client, &zcr_notification_surface_v1_interface,
                         wl_resource_get_version(resource), id);
  SetImplementation(notification_surface_resource,
                    &notification_surface_implementation,
                    std::move(notification_surface));
}

void remote_shell_get_input_method_surface(wl_client* client,
                                           wl_resource* resource,
                                           uint32_t id,
                                           wl_resource* surface) {
  if (GetUserDataAs<Surface>(surface)->HasSurfaceDelegate()) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  std::unique_ptr<ClientControlledShellSurface> input_method_surface =
      GetUserDataAs<WaylandRemoteShell>(resource)->CreateInputMethodSurface(
          GetUserDataAs<Surface>(surface), GetDefaultDeviceScaleFactor());
  if (!input_method_surface) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "Cannot create an IME surface");
    return;
  }

  wl_resource* input_method_surface_resource =
      wl_resource_create(client, &zcr_input_method_surface_v1_interface,
                         wl_resource_get_version(resource), id);
  SetImplementation(input_method_surface_resource,
                    &input_method_surface_implementation,
                    std::move(input_method_surface));
}

const struct zcr_remote_shell_v1_interface remote_shell_implementation = {
    remote_shell_destroy, remote_shell_get_remote_surface,
    remote_shell_get_notification_surface,
    remote_shell_get_input_method_surface};

}  // namespace

void bind_remote_shell(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource = wl_resource_create(
      client, &zcr_remote_shell_v1_interface,
      std::min<uint32_t>(version, zcr_remote_shell_v1_interface.version), id);

  SetImplementation(resource, &remote_shell_implementation,
                    std::make_unique<WaylandRemoteShell>(
                        static_cast<Display*>(data), resource));
}

gfx::Insets GetWorkAreaInsetsInClientPixel(
    const display::Display& display,
    float default_dsf,
    const gfx::Size& size_in_client_pixel,
    const gfx::Rect& work_area_in_dp) {
  gfx::Rect local_work_area_in_dp = work_area_in_dp;
  local_work_area_in_dp.Offset(-display.bounds().x(), -display.bounds().y());
  gfx::Rect work_area_in_client_pixel = ScaleBoundsToPixelSnappedToParent(
      size_in_client_pixel, display.bounds().size(), default_dsf,
      local_work_area_in_dp);
  gfx::Insets insets_in_client_pixel =
      gfx::Rect(size_in_client_pixel).InsetsFrom(work_area_in_client_pixel);

  // TODO(oshima): I think this is more conservative than necessary. The correct
  // way is to use enclosed rect when converting the work area from dp to
  // client pixel, but that led to weird buffer size in overlay detection.
  // (crbug.com/920650). Investigate if we can fix it and use enclosed rect.
  return gfx::Insets(
      gfx::ToRoundedInt(
          gfx::ToCeiledInt(insets_in_client_pixel.top() / default_dsf) *
          default_dsf),
      gfx::ToRoundedInt(
          gfx::ToCeiledInt(insets_in_client_pixel.left() / default_dsf) *
          default_dsf),
      gfx::ToRoundedInt(
          gfx::ToCeiledInt(insets_in_client_pixel.bottom() / default_dsf) *
          default_dsf),
      gfx::ToRoundedInt(
          gfx::ToCeiledInt(insets_in_client_pixel.right() / default_dsf) *
          default_dsf));
}

gfx::Rect GetStableWorkArea(const display::Display& display) {
  auto* root = ash::Shell::GetRootWindowForDisplayId(display.id());
  return ash::WorkAreaInsets::ForWindow(root)->ComputeStableWorkArea();
}

}  // namespace wayland
}  // namespace exo
