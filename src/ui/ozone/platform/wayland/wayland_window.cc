// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_window.h"

#include <memory>

#include <wayland-client.h>

#include "base/bind.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/wayland_pointer.h"
#include "ui/ozone/platform/wayland/xdg_popup_wrapper_v5.h"
#include "ui/ozone/platform/wayland/xdg_popup_wrapper_v6.h"
#include "ui/ozone/platform/wayland/xdg_surface_wrapper_v5.h"
#include "ui/ozone/platform/wayland/xdg_surface_wrapper_v6.h"
#include "ui/platform_window/platform_window_handler/wm_drop_handler.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

namespace {

// Factory, which decides which version type of xdg object to build.
class XDGShellObjectFactory {
 public:
  XDGShellObjectFactory() = default;
  ~XDGShellObjectFactory() = default;

  std::unique_ptr<XDGSurfaceWrapper> CreateXDGSurface(
      WaylandConnection* connection,
      WaylandWindow* wayland_window) {
    if (connection->shell_v6())
      return std::make_unique<XDGSurfaceWrapperV6>(wayland_window);

    DCHECK(connection->shell());
    return std::make_unique<XDGSurfaceWrapperV5>(wayland_window);
  }

  std::unique_ptr<XDGPopupWrapper> CreateXDGPopup(
      WaylandConnection* connection,
      WaylandWindow* wayland_window) {
    if (connection->shell_v6()) {
      std::unique_ptr<XDGSurfaceWrapper> surface =
          CreateXDGSurface(connection, wayland_window);
      surface->Initialize(connection, wayland_window->surface(), false);
      return std::make_unique<XDGPopupWrapperV6>(std::move(surface),
                                                 wayland_window);
    }
    DCHECK(connection->shell());
    return std::make_unique<XDGPopupWrapperV5>(wayland_window);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(XDGShellObjectFactory);
};

// Translates bounds relative to top level window to specified parent.
gfx::Rect TranslateBoundsToParentCoordinates(const gfx::Rect& child_bounds,
                                             const gfx::Rect& parent_bounds) {
  return gfx::Rect(gfx::Point(child_bounds.x() - parent_bounds.x(),
                              child_bounds.y() - parent_bounds.y()),
                   child_bounds.size());
}

// Translates bounds relative to parent window to top level window.
gfx::Rect TranslateBoundsToTopLevelCoordinates(const gfx::Rect& child_bounds,
                                               const gfx::Rect& parent_bounds) {
  return gfx::Rect(gfx::Point(child_bounds.x() + parent_bounds.x(),
                              child_bounds.y() + parent_bounds.y()),
                   child_bounds.size());
}

}  // namespace

WaylandWindow::WaylandWindow(PlatformWindowDelegate* delegate,
                             WaylandConnection* connection)
    : delegate_(delegate),
      connection_(connection),
      xdg_shell_objects_factory_(new XDGShellObjectFactory()),
      state_(PlatformWindowState::PLATFORM_WINDOW_STATE_NORMAL),
      pending_state_(PlatformWindowState::PLATFORM_WINDOW_STATE_UNKNOWN) {
  // Set a class property key, which allows |this| to be used for interactive
  // events, e.g. move or resize.
  SetWmMoveResizeHandler(this, AsWmMoveResizeHandler());

  // Set a class property key, which allows |this| to be used for drag action.
  SetWmDragHandler(this, this);
}

WaylandWindow::~WaylandWindow() {
  if (drag_closed_callback_) {
    std::move(drag_closed_callback_)
        .Run(DragDropTypes::DragOperation::DRAG_NONE);
  }

  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  connection_->RemoveWindow(GetWidget());

  if (parent_window_)
    parent_window_->set_child_window(nullptr);

  if (has_pointer_focus_)
    connection_->pointer()->reset_window_with_pointer_focus();
}

// static
WaylandWindow* WaylandWindow::FromSurface(wl_surface* surface) {
  return static_cast<WaylandWindow*>(
      wl_proxy_get_user_data(reinterpret_cast<wl_proxy*>(surface)));
}

bool WaylandWindow::Initialize(PlatformWindowInitProperties properties) {
  DCHECK(xdg_shell_objects_factory_);

  bounds_ = properties.bounds;

  surface_.reset(wl_compositor_create_surface(connection_->compositor()));
  if (!surface_) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }
  wl_surface_set_user_data(surface_.get(), this);
  AddSurfaceListener();

  ui::PlatformWindowType ui_window_type = properties.type;
  switch (ui_window_type) {
    case ui::PlatformWindowType::kMenu:
    case ui::PlatformWindowType::kPopup:
      parent_window_ = GetParentWindow(properties.parent_widget);

      // TODO(msisov, jkim): Handle notification windows, which are marked
      // as popup windows as well. Those are the windows that do not have
      // parents and pop up when the browser receives a notification.
      CreateXdgPopup();
      break;
    case ui::PlatformWindowType::kTooltip:
      // Tooltips subsurfaces are created on demand, upon ::Show calls.
      is_tooltip_ = true;
      break;
    case ui::PlatformWindowType::kWindow:
      CreateXdgSurface();
      break;
  }

  connection_->ScheduleFlush();

  connection_->AddWindow(GetWidget(), this);
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(GetWidget());

  return true;
}

gfx::AcceleratedWidget WaylandWindow::GetWidget() const {
  if (!surface_)
    return gfx::kNullAcceleratedWidget;
  return surface_.id();
}

std::set<uint32_t> WaylandWindow::GetEnteredOutputsIds() const {
  return entered_outputs_ids_;
}

void WaylandWindow::CreateXdgPopup() {
  if (bounds_.IsEmpty())
    return;

  // TODO(jkim): Consider how to support DropArrow window on tabstrip.
  // When it starts dragging, as described the protocol, https://goo.gl/1Mskq3,
  // the client must have an active implicit grab. If we try to create a popup
  // window while dragging is executed, it gets 'popup_done' directly from
  // Wayland compositor and it's destroyed through 'popup_done'. It causes
  // a crash when aura::Window is destroyed.
  // https://crbug.com/875164
  if (connection_->IsDragInProgress()) {
    surface_.reset();
    LOG(ERROR) << "Wayland can't create a popup window during dragging.";
    return;
  }

  DCHECK(parent_window_ && !xdg_popup_);

  auto bounds = AdjustPopupWindowPosition();

  xdg_popup_ = xdg_shell_objects_factory_->CreateXDGPopup(connection_, this);
  if (!xdg_popup_ ||
      !xdg_popup_->Initialize(connection_, surface(), parent_window_, bounds)) {
    CHECK(false) << "Failed to create xdg_popup";
  }

  parent_window_->set_child_window(this);
}

void WaylandWindow::CreateXdgSurface() {
  xdg_surface_ =
      xdg_shell_objects_factory_->CreateXDGSurface(connection_, this);
  if (!xdg_surface_ || !xdg_surface_->Initialize(connection_, surface_.get())) {
    CHECK(false) << "Failed to create xdg_surface";
  }
}

void WaylandWindow::CreateAndShowTooltipSubSurface() {
  // Since Aura does not not provide a reference parent window, needed by
  // Wayland, we get the current focused window to place and show the tooltips.
  parent_window_ = connection_->GetCurrentFocusedWindow();

  // Tooltip creation is an async operation. By the time Aura actually creates
  // the tooltip, it is possible that the user has already moved the
  // mouse/pointer out of the window that triggered the tooptip. In this case,
  // parent_window_ is NULL.
  if (!parent_window_) {
    Hide();
    return;
  }

  if (!tooltip_subsurface_) {
    wl_subcompositor* subcompositor = connection_->subcompositor();
    DCHECK(subcompositor);
    tooltip_subsurface_.reset(wl_subcompositor_get_subsurface(
        subcompositor, surface_.get(), parent_window_->surface()));
  }

  DCHECK(tooltip_subsurface_);
  wl_subsurface_set_position(tooltip_subsurface_.get(), bounds_.x(),
                             bounds_.y());
  wl_subsurface_set_desync(tooltip_subsurface_.get());
  wl_surface_commit(parent_window_->surface());
  connection_->ScheduleFlush();
}

void WaylandWindow::ApplyPendingBounds() {
  if (pending_bounds_.IsEmpty())
    return;
  DCHECK(xdg_surface_);

  SetBounds(pending_bounds_);
  xdg_surface_->SetWindowGeometry(bounds_);
  xdg_surface_->AckConfigure();
  pending_bounds_ = gfx::Rect();
  connection_->ScheduleFlush();
}

void WaylandWindow::DispatchHostWindowDragMovement(
    int hittest,
    const gfx::Point& pointer_location) {
  DCHECK(xdg_surface_);

  connection_->ResetPointerFlags();
  if (hittest == HTCAPTION)
    xdg_surface_->SurfaceMove(connection_);
  else
    xdg_surface_->SurfaceResize(connection_, hittest);

  connection_->ScheduleFlush();
}

void WaylandWindow::StartDrag(const ui::OSExchangeData& data,
                              int operation,
                              gfx::NativeCursor cursor,
                              base::OnceCallback<void(int)> callback) {
  DCHECK(!drag_closed_callback_);
  drag_closed_callback_ = std::move(callback);
  connection_->StartDrag(data, operation);
}

void WaylandWindow::Show() {
  if (!is_tooltip_)  // Tooltip windows should not get keyboard focus
    set_keyboard_focus(true);

  if (xdg_surface_)
    return;

  if (is_tooltip_) {
    CreateAndShowTooltipSubSurface();
    return;
  }

  if (!xdg_popup_) {
    CreateXdgPopup();
    connection_->ScheduleFlush();
  }
}

void WaylandWindow::Hide() {
  if (is_tooltip_) {
    parent_window_ = nullptr;
    wl_surface_attach(surface_.get(), NULL, 0, 0);
    wl_surface_commit(surface_.get());
    return;
  }

  if (child_window_)
    child_window_->Hide();

  if (xdg_popup_) {
    parent_window_->set_child_window(nullptr);
    xdg_popup_.reset();
    // Detach buffer from surface in order to completely shutdown popups and
    // release resources.
    wl_surface_attach(surface_.get(), NULL, 0, 0);
    wl_surface_commit(surface_.get());
  }
}

void WaylandWindow::Close() {
  NOTIMPLEMENTED();
}

void WaylandWindow::PrepareForShutdown() {}

void WaylandWindow::SetBounds(const gfx::Rect& bounds) {
  if (bounds == bounds_)
    return;
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

gfx::Rect WaylandWindow::GetBounds() {
  return bounds_;
}

void WaylandWindow::SetTitle(const base::string16& title) {
  DCHECK(xdg_surface_);
  xdg_surface_->SetTitle(title);
  connection_->ScheduleFlush();
}

void WaylandWindow::SetCapture() {
  // Wayland does implicit grabs, and doesn't allow for explicit grabs. The
  // exception to that are popups, but we explicitly send events to a
  // parent popup if such exists.
}

void WaylandWindow::ReleaseCapture() {
  // See comment in SetCapture() for details on wayland and grabs.
}

bool WaylandWindow::HasCapture() const {
  // If WaylandWindow is a popup window, assume it has the capture.
  return xdg_popup() ? true : has_implicit_grab_;
}

void WaylandWindow::ToggleFullscreen() {
  DCHECK(xdg_surface_);

  // There are some cases, when Chromium triggers a fullscreen state change
  // before the surface is activated. In such cases, Wayland may ignore state
  // changes and such flags as --kiosk or --start-fullscreen will be ignored.
  // To overcome this, set a pending state, and once the surface is activated,
  // trigger the change.
  if (!is_active_) {
    DCHECK(!IsFullscreen());
    pending_state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN;
    return;
  }

  // TODO(msisov, tonikitoo): add multiscreen support. As the documentation says
  // if xdg_surface_set_fullscreen() is not provided with wl_output, it's up to
  // the compositor to choose which display will be used to map this surface.
  if (!IsFullscreen()) {
    // Fullscreen state changes have to be handled manually and then checked
    // against configuration events, which come from a compositor. The reason
    // of manually changing the |state_| is that the compositor answers about
    // state changes asynchronously, which leads to a wrong return value in
    // DesktopWindowTreeHostPlatform::IsFullscreen, for example, and media
    // files can never be set to fullscreen.
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN;
    xdg_surface_->SetFullscreen();
  } else {
    // Check the comment above. If it's not handled synchronously, media files
    // may not leave the fullscreen mode.
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_UNKNOWN;
    xdg_surface_->UnSetFullscreen();
  }

  connection_->ScheduleFlush();
}

void WaylandWindow::Maximize() {
  DCHECK(xdg_surface_);

  if (IsFullscreen())
    ToggleFullscreen();

  xdg_surface_->SetMaximized();
  connection_->ScheduleFlush();
}

void WaylandWindow::Minimize() {
  DCHECK(xdg_surface_);
  DCHECK(!is_minimizing_);
  // Wayland doesn't explicitly say if a window is minimized. Instead, it
  // notifies that the window is not activated. But there are many cases, when
  // the window is not minimized and deactivated. In order to properly record
  // the minimized state, mark this window as being minimized. And as soon as a
  // configuration event comes, check if the window has been deactivated and has
  // |is_minimizing_| set.
  is_minimizing_ = true;
  xdg_surface_->SetMinimized();
  connection_->ScheduleFlush();
}

void WaylandWindow::Restore() {
  DCHECK(xdg_surface_);

  // Unfullscreen the window if it is fullscreen.
  if (IsFullscreen())
    ToggleFullscreen();

  xdg_surface_->UnSetMaximized();
  connection_->ScheduleFlush();
}

PlatformWindowState WaylandWindow::GetPlatformWindowState() const {
  return state_;
}

void WaylandWindow::SetCursor(PlatformCursor cursor) {
  scoped_refptr<BitmapCursorOzone> bitmap =
      BitmapCursorFactoryOzone::GetBitmapCursor(cursor);
  if (bitmap_ == bitmap)
    return;

  bitmap_ = bitmap;

  if (bitmap_) {
    connection_->SetCursorBitmap(bitmap_->bitmaps(), bitmap_->hotspot());
  } else {
    connection_->SetCursorBitmap(std::vector<SkBitmap>(), gfx::Point());
  }
}

void WaylandWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WaylandWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

PlatformImeController* WaylandWindow::GetPlatformImeController() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WaylandWindow::SetRestoredBoundsInPixels(const gfx::Rect& bounds) {
  restored_bounds_ = bounds;
}

gfx::Rect WaylandWindow::GetRestoredBoundsInPixels() const {
  return restored_bounds_;
}

bool WaylandWindow::CanDispatchEvent(const PlatformEvent& event) {
  // This window is a nested popup window, all the events must be forwarded
  // to the main popup window.
  if (child_window_ && child_window_->xdg_popup())
    return !!xdg_popup_.get();

  // If this is a nested menu window with a parent, it mustn't recieve any
  // events.
  if (parent_window_ && parent_window_->xdg_popup())
    return false;

  // If another window has capture, return early before checking focus.
  if (HasCapture())
    return true;

  if (event->IsMouseEvent())
    return has_pointer_focus_;
  if (event->IsKeyEvent())
    return has_keyboard_focus_;
  if (event->IsTouchEvent())
    return has_touch_focus_;
  return false;
}

uint32_t WaylandWindow::DispatchEvent(const PlatformEvent& native_event) {
  Event* event = static_cast<Event*>(native_event);

  if (event->IsLocatedEvent()) {
    auto copied_event = Event::Clone(*event);
    UpdateCursorPositionFromEvent(std::move(copied_event));
  }

  // If the window does not have a pointer focus, but received this event, it
  // means the window is a popup window with a child popup window. In this case,
  // the location of the event must be converted from the nested popup to the
  // main popup, which the menu controller needs to properly handle events.
  if (event->IsLocatedEvent() && xdg_popup()) {
    // Parent window of the main menu window is not a popup, but rather an
    // xdg surface.
    DCHECK(!parent_window_->xdg_popup() && parent_window_->xdg_surface());
    WaylandWindow* window = connection_->GetCurrentFocusedWindow();
    if (window) {
      ConvertEventLocationToTargetWindowLocation(GetBounds().origin(),
                                                 window->GetBounds().origin(),
                                                 event->AsLocatedEvent());
    }
  }

  DispatchEventFromNativeUiEvent(
      native_event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                                   base::Unretained(delegate_)));
  return POST_DISPATCH_STOP_PROPAGATION;
}

void WaylandWindow::HandleSurfaceConfigure(int32_t width,
                                           int32_t height,
                                           bool is_maximized,
                                           bool is_fullscreen,
                                           bool is_activated) {
  DCHECK(!xdg_popup());

  // Propagate the window state information to the client.
  PlatformWindowState old_state = state_;

  // Ensure that manually handled state changes to fullscreen correspond to the
  // configuration events from a compositor.
  DCHECK_EQ(is_fullscreen, IsFullscreen());

  // There are two cases, which must be handled for the minimized state.
  // The first one is the case, when the surface goes into the minimized state
  // (check comment in WaylandWindow::Minimize), and the second case is when the
  // surface still has been minimized, but another cofiguration event with
  // !is_activated comes. For this, check if the WaylandWindow has been
  // minimized before and !is_activated is sent.
  if ((is_minimizing_ || IsMinimized()) && !is_activated) {
    is_minimizing_ = false;
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_MINIMIZED;
  } else if (is_fullscreen) {
    // To ensure the |delegate_| is notified about state changes to fullscreen,
    // assume the old_state is UNKNOWN (check comment in ToggleFullscreen).
    old_state = PlatformWindowState::PLATFORM_WINDOW_STATE_UNKNOWN;
    DCHECK(state_ == PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN);
  } else if (is_maximized) {
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_MAXIMIZED;
  } else {
    state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_NORMAL;
  }
  const bool state_changed = old_state != state_;
  const bool is_normal = !IsFullscreen() && !IsMaximized();

  // Update state before notifying delegate.
  const bool did_active_change = is_active_ != is_activated;
  is_active_ = is_activated;

  // Rather than call SetBounds here for every configure event, just save the
  // most recent bounds, and have WaylandConnection call ApplyPendingBounds
  // when it has finished processing events. We may get many configure events
  // in a row during an interactive resize, and only the last one matters.
  //
  // Width or height set to 0 means that we should decide on width and height by
  // ourselves, but we don't want to set them to anything else. Use restored
  // bounds size or the current bounds iff the current state is normal (neither
  // maximized nor fullscreen).
  //
  // Note: if the browser was started with --start-fullscreen and a user exits
  // the fullscreen mode, wayland may set the width and height to be 1. Instead,
  // explicitly set the bounds to the current desired ones or the previous
  // bounds.
  if (width > 1 && height > 1) {
    pending_bounds_ = gfx::Rect(0, 0, width, height);
  } else if (is_normal) {
    pending_bounds_.set_size(restored_bounds_.IsEmpty()
                                 ? GetBounds().size()
                                 : restored_bounds_.size());
  }

  if (state_changed) {
    // The |restored_bounds_| are used when the window gets back to normal
    // state after it went maximized or fullscreen.  So we reset these if the
    // window has just become normal and store the current bounds if it is
    // either going out of normal state or simply changes the state and we don't
    // have any meaningful value stored.
    if (is_normal) {
      SetRestoredBoundsInPixels({});
    } else if (old_state == PlatformWindowState::PLATFORM_WINDOW_STATE_NORMAL ||
               restored_bounds_.IsEmpty()) {
      SetRestoredBoundsInPixels(bounds_);
    }

    delegate_->OnWindowStateChanged(state_);
  }

  if (did_active_change)
    delegate_->OnActivationChanged(is_active_);

  MaybeTriggerPendingStateChange();
}

void WaylandWindow::HandlePopupConfigure(const gfx::Rect& bounds) {
  DCHECK(xdg_popup());
  gfx::Rect new_bounds = bounds;

  // It's not enough to just set new bounds. If it is a menu window, whose
  // parent is a top level window aka browser window, it can be flipped
  // vertically along y-axis and have negative values set. Chromium cannot
  // understand that and starts to position nested menu windows incorrectly. To
  // fix that, we have to bear in mind that Wayland compositor does not share
  // global coordinates for any surfaces, and Chromium assumes the top level
  // window is always located at 0,0 origin. What is more, child windows must
  // always be positioned relative to parent window local surface coordinates.
  // Thus, if the menu window is flipped along y-axis by Wayland and its origin
  // is above the top level parent window, the origin of the top level window
  // has to be shifted by that value on y-axis so that the origin of the menu
  // becomes x,0, and events can be handled normally.
  if (!parent_window_->xdg_popup()) {
    gfx::Rect parent_bounds = parent_window_->GetBounds();
    // The menu window is flipped along y-axis and have x,-y origin. Shift the
    // parent top level window instead.
    if (new_bounds.y() < 0) {
      // Move parent bounds along y-axis.
      parent_bounds.set_y(-(new_bounds.y()));
      new_bounds.set_y(0);
    } else {
      // If the menu window is located at correct origin from the browser point
      // of view, return the top level window back to 0,0.
      parent_bounds.set_y(0);
    }
    parent_window_->SetBounds(parent_bounds);
  } else {
    // The nested menu windows are located relative to the parent menu windows.
    // Thus, the location must be translated to be relative to the top level
    // window, which automatically becomes the same as relative to an origin of
    // a display.
    new_bounds = TranslateBoundsToTopLevelCoordinates(
        new_bounds, parent_window_->GetBounds());
    DCHECK(new_bounds.y() >= 0);
  }
  SetBounds(new_bounds);
}

void WaylandWindow::OnCloseRequest() {
  // Before calling OnCloseRequest, the |xdg_popup_| must become hidden and
  // only then call OnCloseRequest().
  DCHECK(!xdg_popup_);
  delegate_->OnCloseRequest();
}

void WaylandWindow::OnDragEnter(const gfx::PointF& point,
                                std::unique_ptr<OSExchangeData> data,
                                int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;
  drop_handler->OnDragEnter(point, std::move(data), operation);
}

int WaylandWindow::OnDragMotion(const gfx::PointF& point,
                                uint32_t time,
                                int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return 0;

  return drop_handler->OnDragMotion(point, operation);
}

void WaylandWindow::OnDragDrop(std::unique_ptr<OSExchangeData> data) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;
  drop_handler->OnDragDrop(std::move(data));
}

void WaylandWindow::OnDragLeave() {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;
  drop_handler->OnDragLeave();
}

void WaylandWindow::OnDragSessionClose(uint32_t dnd_action) {
  std::move(drag_closed_callback_).Run(dnd_action);
  connection_->ResetPointerFlags();
}

bool WaylandWindow::IsMinimized() const {
  return state_ == PlatformWindowState::PLATFORM_WINDOW_STATE_MINIMIZED;
}

bool WaylandWindow::IsMaximized() const {
  return state_ == PlatformWindowState::PLATFORM_WINDOW_STATE_MAXIMIZED;
}

bool WaylandWindow::IsFullscreen() const {
  return state_ == PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN;
}

void WaylandWindow::MaybeTriggerPendingStateChange() {
  if (pending_state_ == PlatformWindowState::PLATFORM_WINDOW_STATE_UNKNOWN ||
      !is_active_)
    return;
  DCHECK_EQ(pending_state_,
            PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN);
  pending_state_ = PlatformWindowState::PLATFORM_WINDOW_STATE_UNKNOWN;
  ToggleFullscreen();
}

WaylandWindow* WaylandWindow::GetParentWindow(
    gfx::AcceleratedWidget parent_widget) {
  WaylandWindow* parent_window = connection_->GetWindow(parent_widget);

  // If propagated parent has already had a child, it means that |this| is a
  // submenu of a 3-dot menu. In aura, the parent of a 3-dot menu and its
  // submenu is the main native widget, which is the main window. In contrast,
  // Wayland requires a menu window to be a parent of a submenu window. Thus,
  // check if the suggested parent has a child. If yes, take its child as a
  // parent of |this|.
  // Another case is a notifcation window or a drop down window, which do not
  // have a parent in aura. In this case, take the current focused window as a
  // parent.
  if (parent_window && parent_window->child_window_)
    return parent_window->child_window_;
  if (!parent_window)
    return connection_->GetCurrentFocusedWindow();
  return parent_window;
}

WmMoveResizeHandler* WaylandWindow::AsWmMoveResizeHandler() {
  return static_cast<WmMoveResizeHandler*>(this);
}

void WaylandWindow::AddSurfaceListener() {
  static struct wl_surface_listener surface_listener = {
      &WaylandWindow::Enter,
      &WaylandWindow::Leave,
  };
  wl_surface_add_listener(surface_.get(), &surface_listener, this);
}

void WaylandWindow::AddEnteredOutputId(struct wl_output* output) {
  const uint32_t entered_output_id =
      connection_->wayland_output_manager()->GetIdForOutput(output);
  DCHECK_NE(entered_output_id, 0u);
  auto result = entered_outputs_ids_.insert(entered_output_id);
  DCHECK(result.first != entered_outputs_ids_.end());
}

void WaylandWindow::RemoveEnteredOutputId(struct wl_output* output) {
  const uint32_t left_output_id =
      connection_->wayland_output_manager()->GetIdForOutput(output);
  auto entered_output_id_it = entered_outputs_ids_.find(left_output_id);
  // Workaround: when a user switches physical output between two displays,
  // a window does not necessarily receive enter events immediately or until
  // a user resizes/moves the window. It means that switching output between
  // displays in a single output mode results in leave events, but the surface
  // might not have received enter event before. Thus, remove the id of left
  // output only if it was stored before.
  if (entered_output_id_it != entered_outputs_ids_.end())
    entered_outputs_ids_.erase(entered_output_id_it);
}

void WaylandWindow::UpdateCursorPositionFromEvent(
    std::unique_ptr<Event> event) {
  DCHECK(event->IsLocatedEvent());
  auto* window = connection_->GetCurrentFocusedWindow();
  // This is a tricky part. Initially, Wayland sends events to surfaces the
  // events are targeted for. But, in order to fulfill Chromium's assumptions
  // about event targets, some of the events are rerouted and their locations
  // are converted.
  //
  // The event we got here is rerouted, but it hasn't had its location fixed
  // yet. Passing an event with fixed location won't help as well - its location
  // is converted in a different way: if mouse is moved outside a menu window
  // to the left, the location of such event includes negative values.
  //
  // In contrast, this method must translate coordinates of all events
  // in regards to top-level windows' coordinates as it's always located at
  // origin (0,0) from Chromium point of view (remember that Wayland doesn't
  // provide global coordinates to its clients). And it's totally fine to use it
  // as the target. Thus, the location of the |event| is always converted using
  // the top-level window's bounds as the target excluding cases, when the
  // mouse/touch is over a top-level window.
  if (parent_window_ && parent_window_ != window) {
    const gfx::Rect target_bounds = parent_window_->GetBounds();
    gfx::Rect own_bounds = GetBounds();
    // This is a bit trickier, and concerns nested menu windows. Whenever an
    // event is sent to the nested menu window, it's rerouted to a parent menu
    // window. Thus, in order to correctly translate its location, we must
    // choose correct values for the |own_bounds|. In this case, it must the
    // nested menu window, because |this| is the parent of that window.
    if (window == child_window_)
      own_bounds = child_window_->GetBounds();
    ConvertEventLocationToTargetWindowLocation(
        target_bounds.origin(), own_bounds.origin(), event->AsLocatedEvent());
  }
  auto* cursor_position = connection_->wayland_cursor_position();
  if (cursor_position) {
    cursor_position->OnCursorPositionChanged(
        event->AsLocatedEvent()->location());
  }
}

gfx::Rect WaylandWindow::AdjustPopupWindowPosition() const {
  auto* parent_window = parent_window_->xdg_popup()
                            ? parent_window_->parent_window_
                            : parent_window_;
  DCHECK(parent_window);
  // Chromium positions windows in screen coordinates, but Wayland requires them
  // to be in local surface coordinates aka relative to parent window.
  const gfx::Rect parent_bounds = parent_window_->GetBounds();
  gfx::Rect new_bounds =
      TranslateBoundsToParentCoordinates(bounds_, parent_bounds);

  // Chromium may decide to position nested menu windows on the left side
  // instead of the right side of parent menu windows when the size of the
  // window becomes larger than the display it is shown on. It's correct when
  // the window is located on one display and occupies the whole work area, but
  // as soon as it's moved and there is space on the right side, Chromium
  // continues positioning the nested menus on the left side relative to the
  // parent menu (Wayland does not provide clients with global coordinates).
  // Instead, reposition that window to be on the right side of the parent menu
  // window and let the compositor decide how to position it if it does not fit
  // a single display. However, there is one exception - if the window is
  // maximized, let Chromium position it on the left side as long as the Wayland
  // compositor may decide to position the nested window on the right side of
  // the parent menu window, which results in showing it on a second display if
  // more than one display is used.
  if (parent_window_->xdg_popup() && parent_window_->parent_window_ &&
      !parent_window_->parent_window_->IsMaximized()) {
    auto* top_level_window = parent_window_->parent_window_;
    DCHECK(top_level_window && !top_level_window->xdg_popup());
    if (new_bounds.x() <= 0 && !top_level_window->IsMaximized()) {
      // Position the child menu window on the right side of the parent window
      // and let the Wayland compositor decide how to do constraint
      // adjustements.
      int new_x = parent_bounds.width() - (new_bounds.width() + new_bounds.x());
      new_bounds.set_x(new_x);
    }
  }
  return new_bounds;
}

WaylandWindow* WaylandWindow::GetTopLevelWindow() {
  return parent_window_ ? parent_window_->GetTopLevelWindow() : this;
}

// static
void WaylandWindow::Enter(void* data,
                          struct wl_surface* wl_surface,
                          struct wl_output* output) {
  auto* window = static_cast<WaylandWindow*>(data);
  if (window) {
    DCHECK(window->surface_.get() == wl_surface);
    window->AddEnteredOutputId(output);
  }
}

// static
void WaylandWindow::Leave(void* data,
                          struct wl_surface* wl_surface,
                          struct wl_output* output) {
  auto* window = static_cast<WaylandWindow*>(data);
  if (window) {
    DCHECK(window->surface_.get() == wl_surface);
    window->RemoveEnteredOutputId(output);
  }
}

}  // namespace ui
