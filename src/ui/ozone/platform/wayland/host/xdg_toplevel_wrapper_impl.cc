// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/xdg_toplevel_wrapper_impl.h"

#include <aura-shell-client-protocol.h>
#include <xdg-decoration-unstable-v1-client-protocol.h>
#include <xdg-shell-client-protocol.h>
#include <xdg-shell-unstable-v6-client-protocol.h>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/hit_test.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/shell_surface_wrapper.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zaura_shell.h"
#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper_impl.h"

namespace ui {

namespace {

XDGToplevelWrapperImpl::DecorationMode ToDecorationMode(uint32_t mode) {
  switch (mode) {
    case ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE:
      return XDGToplevelWrapperImpl::DecorationMode::kClientSide;
    case ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE:
      return XDGToplevelWrapperImpl::DecorationMode::kServerSide;
    default:
      NOTREACHED();
      return XDGToplevelWrapperImpl::DecorationMode::kClientSide;
  }
}

uint32_t ToInt32(XDGToplevelWrapperImpl::DecorationMode mode) {
  switch (mode) {
    case XDGToplevelWrapperImpl::DecorationMode::kClientSide:
      return ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
    case XDGToplevelWrapperImpl::DecorationMode::kServerSide:
      return ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
    default:
      NOTREACHED();
      return ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
  }
}

absl::optional<wl::Serial> GetSerialForMoveResize(
    WaylandConnection* connection) {
  return connection->serial_tracker().GetSerial({wl::SerialType::kTouchPress,
                                                 wl::SerialType::kMousePress,
                                                 wl::SerialType::kKeyPress});
}

}  // namespace

XDGToplevelWrapperImpl::XDGToplevelWrapperImpl(
    std::unique_ptr<XDGSurfaceWrapperImpl> surface,
    WaylandWindow* wayland_window,
    WaylandConnection* connection)
    : xdg_surface_wrapper_(std::move(surface)),
      wayland_window_(wayland_window),
      connection_(connection),
      decoration_mode_(DecorationMode::kNone) {}

XDGToplevelWrapperImpl::~XDGToplevelWrapperImpl() = default;

bool XDGToplevelWrapperImpl::Initialize() {
  if (!connection_->shell()) {
    NOTREACHED() << "Wrong shell protocol";
    return false;
  }

  static constexpr xdg_toplevel_listener xdg_toplevel_listener = {
      &ConfigureTopLevel,
      &CloseTopLevel,
  };

  if (!xdg_surface_wrapper_)
    return false;

  xdg_toplevel_.reset(
      xdg_surface_get_toplevel(xdg_surface_wrapper_->xdg_surface()));
  if (!xdg_toplevel_) {
    LOG(ERROR) << "Failed to create xdg_toplevel";
    return false;
  }

  if (connection_->zaura_shell()) {
    uint32_t version =
        zaura_shell_get_version(connection_->zaura_shell()->wl_object());
    if (version >=
        ZAURA_SHELL_GET_AURA_TOPLEVEL_FOR_XDG_TOPLEVEL_SINCE_VERSION) {
      aura_toplevel_.reset(zaura_shell_get_aura_toplevel_for_xdg_toplevel(
          connection_->zaura_shell()->wl_object(), xdg_toplevel_.get()));
      if (IsWaylandSurfaceSubmissionInPixelCoordinatesEnabled() &&
          version >=
              ZAURA_TOPLEVEL_SURFACE_SUBMISSION_IN_PIXEL_COORDINATES_SINCE_VERSION)
        zaura_toplevel_surface_submission_in_pixel_coordinates(
            aura_toplevel_.get());
    }
  }

  xdg_toplevel_add_listener(xdg_toplevel_.get(), &xdg_toplevel_listener, this);

  InitializeXdgDecoration();

  return true;
}

void XDGToplevelWrapperImpl::SetMaximized() {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_maximized(xdg_toplevel_.get());
}

void XDGToplevelWrapperImpl::UnSetMaximized() {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_unset_maximized(xdg_toplevel_.get());
}

void XDGToplevelWrapperImpl::SetFullscreen() {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_fullscreen(xdg_toplevel_.get(), nullptr);
}

void XDGToplevelWrapperImpl::UnSetFullscreen() {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_unset_fullscreen(xdg_toplevel_.get());
}

void XDGToplevelWrapperImpl::SetMinimized() {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_minimized(xdg_toplevel_.get());
}

void XDGToplevelWrapperImpl::SurfaceMove(WaylandConnection* connection) {
  DCHECK(xdg_toplevel_);
  if (auto serial = GetSerialForMoveResize(connection))
    xdg_toplevel_move(xdg_toplevel_.get(), connection->seat()->wl_object(),
                      serial->value);
}

void XDGToplevelWrapperImpl::SurfaceResize(WaylandConnection* connection,
                                           uint32_t hittest) {
  DCHECK(xdg_toplevel_);
  if (auto serial = GetSerialForMoveResize(connection)) {
    xdg_toplevel_resize(xdg_toplevel_.get(), connection->seat()->wl_object(),
                        serial->value,
                        wl::IdentifyDirection(*connection, hittest));
  }
}

void XDGToplevelWrapperImpl::SetTitle(const std::u16string& title) {
  DCHECK(xdg_toplevel_);

  // TODO(crbug.com/1241097): find a better way to handle long titles, or change
  // this logic completely (and at the platform-agnostic level) because a title
  // that long does not make any sense.
  //
  // A long title may exceed the maximum size of the Wayland event sent below
  // upon calling xdg_toplevel_set_title(), which results in a fatal Wayland
  // communication error and termination of the process.  4096 bytes is the
  // limit for the size of the entire message; here we set 4000 as the maximum
  // length of the string so it would fit the message with some margin.
  const size_t kMaxLengh = 4000;
  auto short_title = base::UTF16ToUTF8(title);
  if (short_title.size() > kMaxLengh)
    short_title.resize(kMaxLengh);
  xdg_toplevel_set_title(xdg_toplevel_.get(), short_title.c_str());
}

void XDGToplevelWrapperImpl::SetWindowGeometry(const gfx::Rect& bounds) {
  xdg_surface_wrapper_->SetWindowGeometry(bounds);
}

void XDGToplevelWrapperImpl::SetMinSize(int32_t width, int32_t height) {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_min_size(xdg_toplevel_.get(), width, height);
}

void XDGToplevelWrapperImpl::SetMaxSize(int32_t width, int32_t height) {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_max_size(xdg_toplevel_.get(), width, height);
}

void XDGToplevelWrapperImpl::SetAppId(const std::string& app_id) {
  DCHECK(xdg_toplevel_);
  xdg_toplevel_set_app_id(xdg_toplevel_.get(), app_id.c_str());
}

void XDGToplevelWrapperImpl::SetDecoration(DecorationMode decoration) {
  SetTopLevelDecorationMode(decoration);
}

void XDGToplevelWrapperImpl::AckConfigure(uint32_t serial) {
  DCHECK(xdg_surface_wrapper_);
  xdg_surface_wrapper_->AckConfigure(serial);
}

bool XDGToplevelWrapperImpl::IsConfigured() {
  DCHECK(xdg_surface_wrapper_);
  return xdg_surface_wrapper_->IsConfigured();
}

// static
void XDGToplevelWrapperImpl::ConfigureTopLevel(
    void* data,
    struct xdg_toplevel* xdg_toplevel,
    int32_t width,
    int32_t height,
    struct wl_array* states) {
  auto* surface = static_cast<XDGToplevelWrapperImpl*>(data);
  DCHECK(surface);

  bool is_maximized =
      CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_MAXIMIZED);
  bool is_fullscreen =
      CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_FULLSCREEN);
  bool is_activated =
      CheckIfWlArrayHasValue(states, XDG_TOPLEVEL_STATE_ACTIVATED);

  surface->wayland_window_->HandleToplevelConfigure(
      width, height, is_maximized, is_fullscreen, is_activated);
}

// static
void XDGToplevelWrapperImpl::CloseTopLevel(void* data,
                                           struct xdg_toplevel* xdg_toplevel) {
  auto* surface = static_cast<XDGToplevelWrapperImpl*>(data);
  DCHECK(surface);
  surface->wayland_window_->OnCloseRequest();
}

void XDGToplevelWrapperImpl::SetTopLevelDecorationMode(
    DecorationMode requested_mode) {
  if (!zxdg_toplevel_decoration_ || requested_mode == decoration_mode_)
    return;

  zxdg_toplevel_decoration_v1_set_mode(zxdg_toplevel_decoration_.get(),
                                       ToInt32(requested_mode));
}

// static
void XDGToplevelWrapperImpl::ConfigureDecoration(
    void* data,
    struct zxdg_toplevel_decoration_v1* decoration,
    uint32_t mode) {
  auto* surface = static_cast<XDGToplevelWrapperImpl*>(data);
  DCHECK(surface);
  surface->decoration_mode_ = ToDecorationMode(mode);
}

void XDGToplevelWrapperImpl::InitializeXdgDecoration() {
  if (connection_->xdg_decoration_manager_v1()) {
    DCHECK(!zxdg_toplevel_decoration_);
    static constexpr zxdg_toplevel_decoration_v1_listener decoration_listener =
        {
            &ConfigureDecoration,
        };
    zxdg_toplevel_decoration_.reset(
        zxdg_decoration_manager_v1_get_toplevel_decoration(
            connection_->xdg_decoration_manager_v1(), xdg_toplevel_.get()));
    zxdg_toplevel_decoration_v1_add_listener(zxdg_toplevel_decoration_.get(),
                                             &decoration_listener, this);
  }
}

XDGSurfaceWrapperImpl* XDGToplevelWrapperImpl::xdg_surface_wrapper() const {
  DCHECK(xdg_surface_wrapper_.get());
  return xdg_surface_wrapper_.get();
}

zaura_toplevel_orientation_lock ToZauraSurfaceOrientationLock(
    WaylandOrientationLockType lock_type) {
  switch (lock_type) {
    case WaylandOrientationLockType::kLandscape:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_LANDSCAPE;
    case WaylandOrientationLockType::kLandscapePrimary:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_LANDSCAPE_PRIMARY;
    case WaylandOrientationLockType::kLandscapeSecondary:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_LANDSCAPE_SECONDARY;
    case WaylandOrientationLockType::kPortrait:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_PORTRAIT;
    case WaylandOrientationLockType::kPortraitPrimary:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_PORTRAIT_PRIMARY;
    case WaylandOrientationLockType::kPortraitSecondary:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_PORTRAIT_SECONDARY;
    case WaylandOrientationLockType::kAny:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_NONE;
    case WaylandOrientationLockType::kNatural:
      return ZAURA_TOPLEVEL_ORIENTATION_LOCK_CURRENT;
  }
  return ZAURA_TOPLEVEL_ORIENTATION_LOCK_NONE;
}

void XDGToplevelWrapperImpl::Lock(WaylandOrientationLockType lock_type) {
  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_SET_ORIENTATION_LOCK_SINCE_VERSION) {
    zaura_toplevel_set_orientation_lock(
        aura_toplevel_.get(), ToZauraSurfaceOrientationLock(lock_type));
  }
}

void XDGToplevelWrapperImpl::Unlock() {
  if (aura_toplevel_ && zaura_toplevel_get_version(aura_toplevel_.get()) >=
                            ZAURA_TOPLEVEL_SET_ORIENTATION_LOCK_SINCE_VERSION) {
    zaura_toplevel_set_orientation_lock(aura_toplevel_.get(),
                                        ZAURA_TOPLEVEL_ORIENTATION_LOCK_NONE);
  }
}

}  // namespace ui
