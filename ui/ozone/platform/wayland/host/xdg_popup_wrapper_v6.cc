// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_popup_wrapper_v6.h"

#include <xdg-shell-unstable-v6-client-protocol.h>
#include <memory>

#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_pointer.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper_v6.h"

namespace ui {

namespace {

constexpr uint32_t kAnchorDefaultWidth = 1;
constexpr uint32_t kAnchorDefaultHeight = 1;
constexpr uint32_t kAnchorHeightParentMenu = 30;

enum class MenuType {
  TYPE_RIGHT_CLICK,
  TYPE_3DOT_PARENT_MENU,
  TYPE_3DOT_CHILD_MENU,
  TYPE_UNKNOWN,
};

uint32_t GetAnchor(MenuType menu_type, const gfx::Rect& bounds) {
  uint32_t anchor = 0;
  switch (menu_type) {
    case MenuType::TYPE_RIGHT_CLICK:
      anchor = ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_LEFT;
      break;
    case MenuType::TYPE_3DOT_PARENT_MENU:
      anchor =
          ZXDG_POSITIONER_V6_ANCHOR_BOTTOM | ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
      break;
    case MenuType::TYPE_3DOT_CHILD_MENU:
      anchor = ZXDG_POSITIONER_V6_ANCHOR_TOP;
      // Chromium may want to manually position a child menu on the left side of
      // its parent menu. Thus, react accordingly. Positive x means the child is
      // located on the right side of the parent and negative - on the left
      // side.
      if (bounds.x() >= 0)
        anchor |= ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
      else
        anchor |= ZXDG_POSITIONER_V6_ANCHOR_LEFT;
      break;
    case MenuType::TYPE_UNKNOWN:
      NOTREACHED() << "Unsupported menu type";
      break;
  }

  return anchor;
}

uint32_t GetGravity(MenuType menu_type, const gfx::Rect& bounds) {
  uint32_t gravity = 0;
  switch (menu_type) {
    case MenuType::TYPE_RIGHT_CLICK:
      gravity =
          ZXDG_POSITIONER_V6_GRAVITY_BOTTOM | ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
      break;
    case MenuType::TYPE_3DOT_PARENT_MENU:
      gravity =
          ZXDG_POSITIONER_V6_GRAVITY_BOTTOM | ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
      break;
    case MenuType::TYPE_3DOT_CHILD_MENU:
      gravity = ZXDG_POSITIONER_V6_GRAVITY_BOTTOM;
      // Chromium may want to manually position a child menu on the left side of
      // its parent menu. Thus, react accordingly. Positive x means the child is
      // located on the right side of the parent and negative - on the left
      // side.
      if (bounds.x() >= 0)
        gravity |= ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
      else
        gravity |= ZXDG_POSITIONER_V6_GRAVITY_LEFT;
      break;
    case MenuType::TYPE_UNKNOWN:
      NOTREACHED() << "Unsupported menu type";
      break;
  }

  return gravity;
}

uint32_t GetConstraintAdjustment(MenuType menu_type) {
  uint32_t constraint = 0;

  switch (menu_type) {
    case MenuType::TYPE_RIGHT_CLICK:
      constraint = ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_X |
                   ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_Y |
                   ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y;
      break;
    case MenuType::TYPE_3DOT_PARENT_MENU:
      constraint = ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_X |
                   ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y;
      break;
    case MenuType::TYPE_3DOT_CHILD_MENU:
      constraint = ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X |
                   ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_Y;
      break;
    case MenuType::TYPE_UNKNOWN:
      NOTREACHED() << "Unsupported menu type";
      break;
  }

  return constraint;
}

gfx::Rect GetAnchorRect(MenuType menu_type,
                        const gfx::Rect& menu_bounds,
                        const gfx::Rect& parent_window_bounds) {
  gfx::Rect anchor_rect;
  switch (menu_type) {
    case MenuType::TYPE_RIGHT_CLICK:
      // Place anchor for right click menus normally.
      anchor_rect = gfx::Rect(menu_bounds.x(), menu_bounds.y(),
                              kAnchorDefaultWidth, kAnchorDefaultHeight);
      break;
    case MenuType::TYPE_3DOT_PARENT_MENU:
      // The anchor for parent menu windows is positioned slightly above the
      // specified bounds to ensure flipped window along y-axis won't hide 3-dot
      // menu button.
      anchor_rect = gfx::Rect(menu_bounds.x() - kAnchorDefaultWidth,
                              menu_bounds.y() - kAnchorHeightParentMenu,
                              kAnchorDefaultWidth, kAnchorHeightParentMenu);
      break;
    case MenuType::TYPE_3DOT_CHILD_MENU:
      // The child menu's anchor must meet the following requirements: at some
      // point, the Wayland compositor can flip it along x-axis. To make sure
      // it's positioned correctly, place it closer to the beginning of the
      // parent menu shifted by the same value along x-axis. The width of anchor
      // must correspond the width between two points - specified origin by the
      // Chromium and calculated point shifted by the same value along x-axis
      // from the beginning of the parent menu width.
      //
      // We also have to bear in mind that Chromium may decide to flip the
      // position of the menu window along the x-axis and show it on the other
      // side of the parent menu window (normally, the Wayland compositor does
      // it). Thus, check which side the child menu window is going to be
      // presented on and create right anchor.
      if (menu_bounds.x() >= 0) {
        auto anchor_width =
            parent_window_bounds.width() -
            (parent_window_bounds.width() - menu_bounds.x()) * 2;
        anchor_rect =
            gfx::Rect(parent_window_bounds.width() - menu_bounds.x(),
                      menu_bounds.y(), anchor_width, kAnchorDefaultHeight);
      } else {
        DCHECK_LE(menu_bounds.x(), 0);
        auto position = menu_bounds.width() + menu_bounds.x();
        DCHECK(position > 0 && position < parent_window_bounds.width());
        auto anchor_width = parent_window_bounds.width() - position * 2;
        anchor_rect = gfx::Rect(position, menu_bounds.y(), anchor_width,
                                kAnchorDefaultHeight);
      }
      break;
    case MenuType::TYPE_UNKNOWN:
      NOTREACHED() << "Unsupported menu type";
      break;
  }

  return anchor_rect;
}

}  // namespace

XDGPopupWrapperV6::XDGPopupWrapperV6(std::unique_ptr<XDGSurfaceWrapper> surface,
                                     WaylandWindow* wayland_window)
    : wayland_window_(wayland_window), zxdg_surface_v6_(std::move(surface)) {
  DCHECK(zxdg_surface_v6_);
}

XDGPopupWrapperV6::~XDGPopupWrapperV6() {}

bool XDGPopupWrapperV6::Initialize(WaylandConnection* connection,
                                   wl_surface* surface,
                                   WaylandWindow* parent_window,
                                   const gfx::Rect& bounds) {
  DCHECK(connection && surface && parent_window);
  static const struct zxdg_popup_v6_listener zxdg_popup_v6_listener = {
      &XDGPopupWrapperV6::Configure,
      &XDGPopupWrapperV6::PopupDone,
  };

  XDGSurfaceWrapperV6* xdg_surface =
      static_cast<XDGSurfaceWrapperV6*>(zxdg_surface_v6_.get());
  if (!xdg_surface)
    return false;

  XDGSurfaceWrapperV6* parent_xdg_surface;
  // If the parent window is a popup, the surface of that popup must be used as
  // a parent.
  if (parent_window->xdg_popup()) {
    XDGPopupWrapperV6* popup =
        reinterpret_cast<XDGPopupWrapperV6*>(parent_window->xdg_popup());
    parent_xdg_surface =
        reinterpret_cast<XDGSurfaceWrapperV6*>(popup->xdg_surface());
  } else {
    parent_xdg_surface =
        reinterpret_cast<XDGSurfaceWrapperV6*>(parent_window->xdg_surface());
  }

  if (!parent_xdg_surface)
    return false;

  zxdg_positioner_v6* positioner =
      CreatePositioner(connection, parent_window, bounds);
  if (!positioner)
    return false;

  xdg_popup_.reset(zxdg_surface_v6_get_popup(xdg_surface->xdg_surface(),
                                             parent_xdg_surface->xdg_surface(),
                                             positioner));
  if (!xdg_popup_)
    return false;

  zxdg_positioner_v6_destroy(positioner);

  zxdg_popup_v6_grab(xdg_popup_.get(), connection->seat(),
                     connection->serial());
  zxdg_popup_v6_add_listener(xdg_popup_.get(), &zxdg_popup_v6_listener, this);

  wl_surface_commit(surface);
  return true;
}

zxdg_positioner_v6* XDGPopupWrapperV6::CreatePositioner(
    WaylandConnection* connection,
    WaylandWindow* parent_window,
    const gfx::Rect& bounds) {
  struct zxdg_positioner_v6* positioner;
  positioner = zxdg_shell_v6_create_positioner(connection->shell_v6());
  if (!positioner)
    return nullptr;

  auto* pointer = connection->pointer();
  uint32_t flags = 0;
  if (pointer)
    flags = pointer->GetFlagsWithKeyboardModifiers();
  bool is_right_click_menu = flags & EF_RIGHT_MOUSE_BUTTON;

  // Different types of menu require different anchors, constraint adjustments,
  // gravity and etc.
  MenuType menu_type = MenuType::TYPE_UNKNOWN;
  if (is_right_click_menu)
    menu_type = MenuType::TYPE_RIGHT_CLICK;
  else if (parent_window->xdg_popup())
    menu_type = MenuType::TYPE_3DOT_CHILD_MENU;
  else
    menu_type = MenuType::TYPE_3DOT_PARENT_MENU;

  // Place anchor to the end of the possible position.
  gfx::Rect anchor_rect =
      GetAnchorRect(menu_type, bounds, parent_window->GetBounds());

  zxdg_positioner_v6_set_anchor_rect(positioner, anchor_rect.x(),
                                     anchor_rect.y(), anchor_rect.width(),
                                     anchor_rect.height());
  zxdg_positioner_v6_set_size(positioner, bounds.width(), bounds.height());
  zxdg_positioner_v6_set_anchor(positioner, GetAnchor(menu_type, bounds));
  zxdg_positioner_v6_set_gravity(positioner, GetGravity(menu_type, bounds));
  zxdg_positioner_v6_set_constraint_adjustment(
      positioner, GetConstraintAdjustment(menu_type));
  return positioner;
}

// static
void XDGPopupWrapperV6::Configure(void* data,
                                  struct zxdg_popup_v6* zxdg_popup_v6,
                                  int32_t x,
                                  int32_t y,
                                  int32_t width,
                                  int32_t height) {
  // As long as the Wayland compositor repositions/requires to position windows
  // relative to their parents, do not propagate final bounds information to
  // Chromium. The browser places windows in respect to screen origin, but
  // Wayland requires doing so in respect to parent window's origin. To properly
  // place windows, the bounds are translated and adjusted according to the
  // Wayland compositor needs during WaylandWindow::CreateXdgPopup call.
  gfx::Rect new_bounds(x, y, width, height);
  WaylandWindow* window =
      static_cast<XDGPopupWrapperV6*>(data)->wayland_window_;
  DCHECK(window);
  window->HandlePopupConfigure(new_bounds);
}

// static
void XDGPopupWrapperV6::PopupDone(void* data,
                                  struct zxdg_popup_v6* zxdg_popup_v6) {
  WaylandWindow* window =
      static_cast<XDGPopupWrapperV6*>(data)->wayland_window_;
  DCHECK(window);
  window->Hide();
  window->OnCloseRequest();
}

XDGSurfaceWrapper* XDGPopupWrapperV6::xdg_surface() {
  DCHECK(zxdg_surface_v6_.get());
  return zxdg_surface_v6_.get();
}

}  // namespace ui
