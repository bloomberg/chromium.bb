// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zxdg_shell.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/display.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/xdg_shell_surface.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace exo {
namespace wayland {
namespace {

////////////////////////////////////////////////////////////////////////////////
// xdg_positioner_interface:

uint32_t InvertBitfield(uint32_t bitfield, uint32_t mask) {
  return (bitfield & ~mask) | ((bitfield & mask) ^ mask);
}

// TODO(oshima): propagate x/y flip state to children.
struct WaylandPositioner {
  static constexpr uint32_t kHorizontalAnchors =
      ZXDG_POSITIONER_V6_ANCHOR_LEFT | ZXDG_POSITIONER_V6_ANCHOR_RIGHT;
  static constexpr uint32_t kVerticalAnchors =
      ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_BOTTOM;
  static constexpr uint32_t kHorizontalGravities =
      ZXDG_POSITIONER_V6_GRAVITY_LEFT | ZXDG_POSITIONER_V6_GRAVITY_RIGHT;
  static constexpr uint32_t kVerticalGravities =
      ZXDG_POSITIONER_V6_GRAVITY_TOP | ZXDG_POSITIONER_V6_GRAVITY_BOTTOM;

  static int CalculateX(const gfx::Size& size,
                        const gfx::Rect& anchor_rect,
                        uint32_t anchor,
                        uint32_t gravity,
                        int offset,
                        bool flipped) {
    if (flipped) {
      anchor = InvertBitfield(anchor, kHorizontalAnchors);
      gravity = InvertBitfield(gravity, kHorizontalGravities);
      offset = -offset;
    }

    int x = offset;
    if (anchor & ZXDG_POSITIONER_V6_ANCHOR_LEFT)
      x += anchor_rect.x();
    else if (anchor & ZXDG_POSITIONER_V6_ANCHOR_RIGHT)
      x += anchor_rect.right();
    else
      x += anchor_rect.CenterPoint().x();

    if (gravity & ZXDG_POSITIONER_V6_GRAVITY_LEFT)
      return x - size.width();
    if (gravity & ZXDG_POSITIONER_V6_GRAVITY_RIGHT)
      return x;
    return x - size.width() / 2;
  }

  static int CalculateY(const gfx::Size& size,
                        const gfx::Rect& anchor_rect,
                        uint32_t anchor,
                        uint32_t gravity,
                        int offset,
                        bool flipped) {
    if (flipped) {
      anchor = InvertBitfield(anchor, kVerticalAnchors);
      gravity = InvertBitfield(gravity, kVerticalGravities);
      offset = -offset;
    }

    int y = offset;
    if (anchor & ZXDG_POSITIONER_V6_ANCHOR_TOP)
      y += anchor_rect.y();
    else if (anchor & ZXDG_POSITIONER_V6_ANCHOR_BOTTOM)
      y += anchor_rect.bottom();
    else
      y += anchor_rect.CenterPoint().y();

    if (gravity & ZXDG_POSITIONER_V6_GRAVITY_TOP)
      return y - size.height();
    if (gravity & ZXDG_POSITIONER_V6_GRAVITY_BOTTOM)
      return y;
    return y - size.height() / 2;
  }

  // Calculate and return position from current state.
  gfx::Point CalculatePosition(const gfx::Rect& work_area) {
    // TODO(oshima): The size must be smaller than work area.

    gfx::Rect bounds(gfx::Point(CalculateX(size, anchor_rect, anchor, gravity,
                                           offset.x(), x_flipped),
                                CalculateY(size, anchor_rect, anchor, gravity,
                                           offset.y(), y_flipped)),
                     size);

    // Adjust x position if the bounds are not fully contained by the work area.
    if (work_area.x() > bounds.x() || work_area.right() < bounds.right()) {
      // Allow sliding horizontally if the surface is attached below
      // or above the parent surface.
      bool can_slide_x = (anchor & ZXDG_POSITIONER_V6_ANCHOR_BOTTOM &&
                          gravity & ZXDG_POSITIONER_V6_GRAVITY_BOTTOM) ||
                         (anchor & ZXDG_POSITIONER_V6_ANCHOR_TOP &&
                          gravity & ZXDG_POSITIONER_V6_GRAVITY_TOP);
      if (adjustment & ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_X &&
          can_slide_x) {
        if (bounds.x() < work_area.x())
          bounds.set_x(work_area.x());
        else if (bounds.right() > work_area.right())
          bounds.set_x(work_area.right() - size.width());
      } else if (adjustment & ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X) {
        x_flipped = !x_flipped;
        bounds.set_x(CalculateX(size, anchor_rect, anchor, gravity, offset.x(),
                                x_flipped));
      }
    }

    // Adjust y position if the bounds are not fully contained by the work area.
    if (work_area.y() > bounds.y() || work_area.bottom() < bounds.bottom()) {
      // Allow sliding vertically if the surface is attached left or
      // right of the parent surface.
      bool can_slide_y = (anchor & ZXDG_POSITIONER_V6_ANCHOR_LEFT &&
                          gravity & ZXDG_POSITIONER_V6_GRAVITY_LEFT) ||
                         (anchor & ZXDG_POSITIONER_V6_ANCHOR_RIGHT &&
                          gravity & ZXDG_POSITIONER_V6_GRAVITY_RIGHT);
      if (adjustment & ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_Y &&
          can_slide_y) {
        if (bounds.y() < work_area.y())
          bounds.set_y(work_area.y());
        else if (bounds.bottom() > work_area.bottom())
          bounds.set_y(work_area.bottom() - size.height());
      } else if (adjustment & ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y) {
        y_flipped = !y_flipped;
        bounds.set_y(CalculateY(size, anchor_rect, anchor, gravity, offset.y(),
                                y_flipped));
      }
    }
    return bounds.origin();
  }

  gfx::Size size;
  gfx::Rect anchor_rect;
  uint32_t anchor = ZXDG_POSITIONER_V6_ANCHOR_NONE;
  uint32_t gravity = ZXDG_POSITIONER_V6_GRAVITY_NONE;
  uint32_t adjustment = ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_NONE;
  gfx::Vector2d offset;
  bool y_flipped = false;
  bool x_flipped = false;
};

void xdg_positioner_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_positioner_v6_set_size(wl_client* client,
                                wl_resource* resource,
                                int32_t width,
                                int32_t height) {
  if (width < 1 || height < 1) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "width and height must be positive and non-zero");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->size = gfx::Size(width, height);
}

void xdg_positioner_v6_set_anchor_rect(wl_client* client,
                                       wl_resource* resource,
                                       int32_t x,
                                       int32_t y,
                                       int32_t width,
                                       int32_t height) {
  if (width < 1 || height < 1) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "width and height must be positive and non-zero");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->anchor_rect =
      gfx::Rect(x, y, width, height);
}

void xdg_positioner_v6_set_anchor(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t anchor) {
  if (((anchor & ZXDG_POSITIONER_V6_ANCHOR_LEFT) &&
       (anchor & ZXDG_POSITIONER_V6_ANCHOR_RIGHT)) ||
      ((anchor & ZXDG_POSITIONER_V6_ANCHOR_TOP) &&
       (anchor & ZXDG_POSITIONER_V6_ANCHOR_BOTTOM))) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "same-axis values are not allowed");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->anchor = anchor;
}

void xdg_positioner_v6_set_gravity(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t gravity) {
  if (((gravity & ZXDG_POSITIONER_V6_GRAVITY_LEFT) &&
       (gravity & ZXDG_POSITIONER_V6_GRAVITY_RIGHT)) ||
      ((gravity & ZXDG_POSITIONER_V6_GRAVITY_TOP) &&
       (gravity & ZXDG_POSITIONER_V6_GRAVITY_BOTTOM))) {
    wl_resource_post_error(resource, ZXDG_POSITIONER_V6_ERROR_INVALID_INPUT,
                           "same-axis values are not allowed");
    return;
  }

  GetUserDataAs<WaylandPositioner>(resource)->gravity = gravity;
}

void xdg_positioner_v6_set_constraint_adjustment(wl_client* client,
                                                 wl_resource* resource,
                                                 uint32_t adjustment) {
  GetUserDataAs<WaylandPositioner>(resource)->adjustment = adjustment;
}

void xdg_positioner_v6_set_offset(wl_client* client,
                                  wl_resource* resource,
                                  int32_t x,
                                  int32_t y) {
  GetUserDataAs<WaylandPositioner>(resource)->offset = gfx::Vector2d(x, y);
}

const struct zxdg_positioner_v6_interface xdg_positioner_v6_implementation = {
    xdg_positioner_v6_destroy,
    xdg_positioner_v6_set_size,
    xdg_positioner_v6_set_anchor_rect,
    xdg_positioner_v6_set_anchor,
    xdg_positioner_v6_set_gravity,
    xdg_positioner_v6_set_constraint_adjustment,
    xdg_positioner_v6_set_offset};

////////////////////////////////////////////////////////////////////////////////
// xdg_toplevel_interface:

int XdgToplevelV6ResizeComponent(uint32_t edges) {
  switch (edges) {
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP:
      return HTTOP;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM:
      return HTBOTTOM;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_LEFT:
      return HTLEFT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_LEFT:
      return HTTOPLEFT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_LEFT:
      return HTBOTTOMLEFT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_RIGHT:
      return HTRIGHT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_TOP_RIGHT:
      return HTTOPRIGHT;
    case ZXDG_TOPLEVEL_V6_RESIZE_EDGE_BOTTOM_RIGHT:
      return HTBOTTOMRIGHT;
    default:
      return HTBOTTOMRIGHT;
  }
}

using XdgSurfaceConfigureCallback =
    base::Callback<void(const gfx::Size& size,
                        ash::mojom::WindowStateType state_type,
                        bool resizing,
                        bool activated)>;

uint32_t HandleXdgSurfaceV6ConfigureCallback(
    wl_resource* resource,
    const XdgSurfaceConfigureCallback& callback,
    const gfx::Size& size,
    ash::mojom::WindowStateType state_type,
    bool resizing,
    bool activated,
    const gfx::Vector2d& origin_offset) {
  uint32_t serial = wl_display_next_serial(
      wl_client_get_display(wl_resource_get_client(resource)));
  callback.Run(size, state_type, resizing, activated);
  zxdg_surface_v6_send_configure(resource, serial);
  wl_client_flush(wl_resource_get_client(resource));
  return serial;
}

// Wrapper around shell surface that allows us to handle the case where the
// xdg surface resource is destroyed before the toplevel resource.
class WaylandToplevel : public aura::WindowObserver {
 public:
  WaylandToplevel(wl_resource* resource, wl_resource* surface_resource)
      : resource_(resource),
        shell_surface_(GetUserDataAs<XdgShellSurface>(surface_resource)),
        weak_ptr_factory_(this) {
    shell_surface_->host_window()->AddObserver(this);
    shell_surface_->set_close_callback(
        base::Bind(&WaylandToplevel::OnClose, weak_ptr_factory_.GetWeakPtr()));
    shell_surface_->set_configure_callback(
        base::Bind(&HandleXdgSurfaceV6ConfigureCallback, surface_resource,
                   base::Bind(&WaylandToplevel::OnConfigure,
                              weak_ptr_factory_.GetWeakPtr())));
  }
  ~WaylandToplevel() override {
    if (shell_surface_)
      shell_surface_->host_window()->RemoveObserver(this);
  }

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    shell_surface_ = nullptr;
  }

  void SetParent(WaylandToplevel* parent) {
    if (!shell_surface_)
      return;

    if (!parent) {
      shell_surface_->SetParent(nullptr);
      return;
    }

    // This is a no-op if parent is not mapped.
    if (parent->shell_surface_ && parent->shell_surface_->GetWidget())
      shell_surface_->SetParent(parent->shell_surface_);
  }

  void SetTitle(const base::string16& title) {
    if (shell_surface_)
      shell_surface_->SetTitle(title);
  }

  void SetApplicationId(const char* application_id) {
    if (shell_surface_)
      shell_surface_->SetApplicationId(application_id);
  }

  void Move() {
    if (shell_surface_)
      shell_surface_->StartMove();
  }

  void Resize(int component) {
    if (!shell_surface_)
      return;

    if (component != HTNOWHERE)
      shell_surface_->StartResize(component);
  }

  void SetMaximumSize(const gfx::Size& size) {
    if (shell_surface_)
      shell_surface_->SetMaximumSize(size);
  }

  void SetMinimumSize(const gfx::Size& size) {
    if (shell_surface_)
      shell_surface_->SetMinimumSize(size);
  }

  void Maximize() {
    if (shell_surface_)
      shell_surface_->Maximize();
  }

  void Restore() {
    if (shell_surface_)
      shell_surface_->Restore();
  }

  void SetFullscreen(bool fullscreen) {
    if (shell_surface_)
      shell_surface_->SetFullscreen(fullscreen);
  }

  void Minimize() {
    if (shell_surface_)
      shell_surface_->Minimize();
  }

 private:
  void OnClose() {
    zxdg_toplevel_v6_send_close(resource_);
    wl_client_flush(wl_resource_get_client(resource_));
  }

  static void AddState(wl_array* states, zxdg_toplevel_v6_state state) {
    zxdg_toplevel_v6_state* value = static_cast<zxdg_toplevel_v6_state*>(
        wl_array_add(states, sizeof(zxdg_toplevel_v6_state)));
    DCHECK(value);
    *value = state;
  }

  void OnConfigure(const gfx::Size& size,
                   ash::mojom::WindowStateType state_type,
                   bool resizing,
                   bool activated) {
    wl_array states;
    wl_array_init(&states);
    if (state_type == ash::mojom::WindowStateType::MAXIMIZED)
      AddState(&states, ZXDG_TOPLEVEL_V6_STATE_MAXIMIZED);
    if (state_type == ash::mojom::WindowStateType::FULLSCREEN)
      AddState(&states, ZXDG_TOPLEVEL_V6_STATE_FULLSCREEN);
    if (resizing)
      AddState(&states, ZXDG_TOPLEVEL_V6_STATE_RESIZING);
    if (activated)
      AddState(&states, ZXDG_TOPLEVEL_V6_STATE_ACTIVATED);
    zxdg_toplevel_v6_send_configure(resource_, size.width(), size.height(),
                                    &states);
    wl_array_release(&states);
  }

  wl_resource* const resource_;
  XdgShellSurface* shell_surface_;
  base::WeakPtrFactory<WaylandToplevel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandToplevel);
};

void xdg_toplevel_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_toplevel_v6_set_parent(wl_client* client,
                                wl_resource* resource,
                                wl_resource* parent) {
  WaylandToplevel* parent_surface = nullptr;
  if (parent)
    parent_surface = GetUserDataAs<WaylandToplevel>(parent);

  GetUserDataAs<WaylandToplevel>(resource)->SetParent(parent_surface);
}

void xdg_toplevel_v6_set_title(wl_client* client,
                               wl_resource* resource,
                               const char* title) {
  GetUserDataAs<WaylandToplevel>(resource)->SetTitle(
      base::string16(base::UTF8ToUTF16(title)));
}

void xdg_toplevel_v6_set_app_id(wl_client* client,
                                wl_resource* resource,
                                const char* app_id) {
  GetUserDataAs<WaylandToplevel>(resource)->SetApplicationId(app_id);
}

void xdg_toplevel_v6_show_window_menu(wl_client* client,
                                      wl_resource* resource,
                                      wl_resource* seat,
                                      uint32_t serial,
                                      int32_t x,
                                      int32_t y) {
  NOTIMPLEMENTED();
}

void xdg_toplevel_v6_move(wl_client* client,
                          wl_resource* resource,
                          wl_resource* seat,
                          uint32_t serial) {
  GetUserDataAs<WaylandToplevel>(resource)->Move();
}

void xdg_toplevel_v6_resize(wl_client* client,
                            wl_resource* resource,
                            wl_resource* seat,
                            uint32_t serial,
                            uint32_t edges) {
  GetUserDataAs<WaylandToplevel>(resource)->Resize(
      XdgToplevelV6ResizeComponent(edges));
}

void xdg_toplevel_v6_set_max_size(wl_client* client,
                                  wl_resource* resource,
                                  int32_t width,
                                  int32_t height) {
  GetUserDataAs<WaylandToplevel>(resource)->SetMaximumSize(
      gfx::Size(width, height));
}

void xdg_toplevel_v6_set_min_size(wl_client* client,
                                  wl_resource* resource,
                                  int32_t width,
                                  int32_t height) {
  GetUserDataAs<WaylandToplevel>(resource)->SetMinimumSize(
      gfx::Size(width, height));
}

void xdg_toplevel_v6_set_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->Maximize();
}

void xdg_toplevel_v6_unset_maximized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->Restore();
}

void xdg_toplevel_v6_set_fullscreen(wl_client* client,
                                    wl_resource* resource,
                                    wl_resource* output) {
  GetUserDataAs<WaylandToplevel>(resource)->SetFullscreen(true);
}

void xdg_toplevel_v6_unset_fullscreen(wl_client* client,
                                      wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->SetFullscreen(false);
}

void xdg_toplevel_v6_set_minimized(wl_client* client, wl_resource* resource) {
  GetUserDataAs<WaylandToplevel>(resource)->Minimize();
}

const struct zxdg_toplevel_v6_interface xdg_toplevel_v6_implementation = {
    xdg_toplevel_v6_destroy,          xdg_toplevel_v6_set_parent,
    xdg_toplevel_v6_set_title,        xdg_toplevel_v6_set_app_id,
    xdg_toplevel_v6_show_window_menu, xdg_toplevel_v6_move,
    xdg_toplevel_v6_resize,           xdg_toplevel_v6_set_max_size,
    xdg_toplevel_v6_set_min_size,     xdg_toplevel_v6_set_maximized,
    xdg_toplevel_v6_unset_maximized,  xdg_toplevel_v6_set_fullscreen,
    xdg_toplevel_v6_unset_fullscreen, xdg_toplevel_v6_set_minimized};

////////////////////////////////////////////////////////////////////////////////
// xdg_popup_interface:

// Wrapper around shell surface that allows us to handle the case where the
// xdg surface resource is destroyed before the popup resource.
class WaylandPopup : aura::WindowObserver {
 public:
  WaylandPopup(wl_resource* resource, wl_resource* surface_resource)
      : resource_(resource),
        shell_surface_(GetUserDataAs<ShellSurface>(surface_resource)),
        weak_ptr_factory_(this) {
    shell_surface_->host_window()->AddObserver(this);
    shell_surface_->set_close_callback(
        base::Bind(&WaylandPopup::OnClose, weak_ptr_factory_.GetWeakPtr()));
    shell_surface_->set_configure_callback(
        base::Bind(&HandleXdgSurfaceV6ConfigureCallback, surface_resource,
                   base::Bind(&WaylandPopup::OnConfigure,
                              weak_ptr_factory_.GetWeakPtr())));
  }
  ~WaylandPopup() override {
    if (shell_surface_)
      shell_surface_->host_window()->RemoveObserver(this);
  }

  void Grab() {
    if (!shell_surface_) {
      wl_resource_post_error(resource_, ZXDG_POPUP_V6_ERROR_INVALID_GRAB,
                             "the surface has already been destroyed");
      return;
    }
    if (shell_surface_->GetWidget()) {
      wl_resource_post_error(resource_, ZXDG_POPUP_V6_ERROR_INVALID_GRAB,
                             "grab must be called before construction");
      return;
    }
    shell_surface_->Grab();
  }

  // Overridden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    shell_surface_ = nullptr;
  }

 private:
  void OnClose() {
    zxdg_popup_v6_send_popup_done(resource_);
    wl_client_flush(wl_resource_get_client(resource_));
  }

  void OnConfigure(const gfx::Size& size,
                   ash::mojom::WindowStateType state_type,
                   bool resizing,
                   bool activated) {
    // Nothing to do here as popups don't have additional configure state.
  }

  wl_resource* const resource_;
  ShellSurface* shell_surface_;
  base::WeakPtrFactory<WaylandPopup> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandPopup);
};

void xdg_popup_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_popup_v6_grab(wl_client* client,
                       wl_resource* resource,
                       wl_resource* seat,
                       uint32_t serial) {
  GetUserDataAs<WaylandPopup>(resource)->Grab();
}

const struct zxdg_popup_v6_interface xdg_popup_v6_implementation = {
    xdg_popup_v6_destroy, xdg_popup_v6_grab};

////////////////////////////////////////////////////////////////////////////////
// xdg_surface_interface:

void xdg_surface_v6_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdg_surface_v6_get_toplevel(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t id) {
  ShellSurface* shell_surface = GetUserDataAs<ShellSurface>(resource);
  if (shell_surface->enabled()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }

  shell_surface->SetCanMinimize(true);
  shell_surface->SetEnabled(true);

  wl_resource* xdg_toplevel_resource =
      wl_resource_create(client, &zxdg_toplevel_v6_interface, 1, id);

  SetImplementation(
      xdg_toplevel_resource, &xdg_toplevel_v6_implementation,
      std::make_unique<WaylandToplevel>(xdg_toplevel_resource, resource));
}

void xdg_surface_v6_get_popup(wl_client* client,
                              wl_resource* resource,
                              uint32_t id,
                              wl_resource* parent_resource,
                              wl_resource* positioner_resource) {
  XdgShellSurface* shell_surface = GetUserDataAs<XdgShellSurface>(resource);
  if (shell_surface->enabled()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "surface has already been constructed");
    return;
  }

  XdgShellSurface* parent = GetUserDataAs<XdgShellSurface>(parent_resource);
  if (!parent->GetWidget()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_NOT_CONSTRUCTED,
                           "popup parent not constructed");
    return;
  }

  if (shell_surface->GetWidget()) {
    wl_resource_post_error(resource, ZXDG_SURFACE_V6_ERROR_ALREADY_CONSTRUCTED,
                           "get_popup is called after constructed");
    return;
  }

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          parent->GetWidget()->GetNativeWindow());
  gfx::Rect work_area = display.work_area();
  wm::ConvertRectFromScreen(parent->GetWidget()->GetNativeWindow(), &work_area);

  WaylandPositioner* positioner =
      GetUserDataAs<WaylandPositioner>(positioner_resource);
  // Try layout using parent's flip state.
  positioner->x_flipped = parent->x_flipped();
  positioner->y_flipped = parent->y_flipped();

  gfx::Point position = positioner->CalculatePosition(work_area);

  // Remember the new flip state for its child popups.
  shell_surface->set_x_flipped(positioner->x_flipped);
  shell_surface->set_y_flipped(positioner->y_flipped);

  // |position| is relative to the parent's contents view origin, and |origin|
  // is in screen coordinates.
  gfx::Point origin = position;
  views::View::ConvertPointToScreen(
      parent->GetWidget()->widget_delegate()->GetContentsView(), &origin);
  shell_surface->SetOrigin(origin);
  shell_surface->SetContainer(ash::kShellWindowId_MenuContainer);
  shell_surface->DisableMovement();
  shell_surface->SetActivatable(false);
  shell_surface->SetCanMinimize(false);
  shell_surface->SetParent(parent);
  shell_surface->SetPopup();
  shell_surface->SetEnabled(true);

  wl_resource* xdg_popup_resource =
      wl_resource_create(client, &zxdg_popup_v6_interface, 1, id);

  SetImplementation(
      xdg_popup_resource, &xdg_popup_v6_implementation,
      std::make_unique<WaylandPopup>(xdg_popup_resource, resource));
}

void xdg_surface_v6_set_window_geometry(wl_client* client,
                                        wl_resource* resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height) {
  GetUserDataAs<ShellSurface>(resource)->SetGeometry(
      gfx::Rect(x, y, width, height));
}

void xdg_surface_v6_ack_configure(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t serial) {
  GetUserDataAs<ShellSurface>(resource)->AcknowledgeConfigure(serial);
}

const struct zxdg_surface_v6_interface xdg_surface_v6_implementation = {
    xdg_surface_v6_destroy, xdg_surface_v6_get_toplevel,
    xdg_surface_v6_get_popup, xdg_surface_v6_set_window_geometry,
    xdg_surface_v6_ack_configure};

////////////////////////////////////////////////////////////////////////////////
// xdg_shell_interface:

void xdg_shell_v6_destroy(wl_client* client, wl_resource* resource) {
  // Nothing to do here.
}

void xdg_shell_v6_create_positioner(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t id) {
  wl_resource* positioner_resource =
      wl_resource_create(client, &zxdg_positioner_v6_interface, 1, id);

  SetImplementation(positioner_resource, &xdg_positioner_v6_implementation,
                    std::make_unique<WaylandPositioner>());
}

void xdg_shell_v6_get_xdg_surface(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t id,
                                  wl_resource* surface) {
  std::unique_ptr<ShellSurface> shell_surface =
      GetUserDataAs<Display>(resource)->CreateXdgShellSurface(
          GetUserDataAs<Surface>(surface));
  if (!shell_surface) {
    wl_resource_post_error(resource, ZXDG_SHELL_V6_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  // Xdg shell v6 surfaces are initially disabled and needs to be explicitly
  // mapped before they are enabled and can become visible.
  shell_surface->SetEnabled(false);

  wl_resource* xdg_surface_resource =
      wl_resource_create(client, &zxdg_surface_v6_interface, 1, id);

  SetImplementation(xdg_surface_resource, &xdg_surface_v6_implementation,
                    std::move(shell_surface));
}

void xdg_shell_v6_pong(wl_client* client,
                       wl_resource* resource,
                       uint32_t serial) {
  NOTIMPLEMENTED();
}

const struct zxdg_shell_v6_interface xdg_shell_v6_implementation = {
    xdg_shell_v6_destroy, xdg_shell_v6_create_positioner,
    xdg_shell_v6_get_xdg_surface, xdg_shell_v6_pong};

}  // namespace

void bind_xdg_shell_v6(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zxdg_shell_v6_interface, 1, id);

  wl_resource_set_implementation(resource, &xdg_shell_v6_implementation, data,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
