// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window.h"

#include <stdint.h>
#include <wayland-cursor.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/ozone/common/bitmap_cursor.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/common/wayland_overlay_config.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_frame_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_screen.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_cursor_shapes.h"
#include "ui/ozone/platform/wayland/mojom/wayland_overlay_config.mojom.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#include "ui/platform_window/wm/wm_drag_handler.h"
#include "ui/platform_window/wm/wm_drop_handler.h"

namespace ui {
namespace {

using mojom::CursorType;
using mojom::DragOperation;

bool OverlayStackOrderCompare(const wl::WaylandOverlayConfig& i,
                              const wl::WaylandOverlayConfig& j) {
  return i.z_order < j.z_order;
}

}  // namespace

WaylandWindow::WaylandWindow(PlatformWindowDelegate* delegate,
                             WaylandConnection* connection)
    : delegate_(delegate),
      connection_(connection),
      frame_manager_(std::make_unique<WaylandFrameManager>(this, connection)),
      wayland_overlay_delegation_enabled_(connection->viewporter() &&
                                          IsWaylandOverlayDelegationEnabled()),
      accelerated_widget_(
          connection->wayland_window_manager()->AllocateAcceleratedWidget()),
      ui_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  // Set a class property key, which allows |this| to be used for drag action.
  SetWmDragHandler(this, this);
}

WaylandWindow::~WaylandWindow() {
  CHECK(ui_task_runner_->BelongsToCurrentThread());
  shutting_down_ = true;

  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);

  if (wayland_overlay_delegation_enabled_) {
    connection_->wayland_window_manager()->RemoveSubsurface(
        GetWidget(), primary_subsurface_.get());
  }
  for (const auto& widget_subsurface : wayland_subsurfaces()) {
    connection_->wayland_window_manager()->RemoveSubsurface(
        GetWidget(), widget_subsurface.get());
  }
  if (root_surface_)
    connection_->wayland_window_manager()->RemoveWindow(GetWidget());

  // This might have already been hidden and another window has been shown.
  // Thus, the parent will have another child window. Do not reset it.
  if (parent_window_ && parent_window_->child_window() == this)
    parent_window_->set_child_window(nullptr);

  if (child_window_)
    child_window_->set_parent_window(nullptr);
}

void WaylandWindow::OnWindowLostCapture() {
  delegate_->OnLostCapture();
}

void WaylandWindow::UpdateWindowScale(bool update_bounds) {
  DCHECK(connection_->wayland_output_manager());

  auto preferred_outputs_id = GetPreferredEnteredOutputId();
  if (preferred_outputs_id == 0) {
    // If non of the output are entered, use primary output. This is what
    // WaylandScreen returns back to ScreenOzone.
    auto* primary_output =
        connection_->wayland_output_manager()->GetPrimaryOutput();
    // We don't know our primary output - WaylandScreen hasn't been created
    // yet.
    if (!primary_output)
      return;
    preferred_outputs_id = primary_output->output_id();
  }

  auto* output =
      connection_->wayland_output_manager()->GetOutput(preferred_outputs_id);
  // There can be a race between sending leave output event and destroying
  // wl_outputs. Thus, explicitly check if the output exist.
  if (!output)
    return;

  float new_scale = output->scale_factor();
  ui_scale_ = output->GetUIScaleFactor();

  float old_scale = window_scale();
  window_scale_ = new_scale;

  // We need to keep DIP size of the window the same whenever the scale changes.
  if (update_bounds)
    UpdateBoundsInDIP(gfx::ScaleToEnclosedRect(bounds_px_, 1.0 / old_scale));

  // Propagate update to the child windows
  if (child_window_)
    child_window_->UpdateWindowScale(update_bounds);
}

gfx::AcceleratedWidget WaylandWindow::GetWidget() const {
  return accelerated_widget_;
}

void WaylandWindow::SetWindowScale(float new_scale) {
  DCHECK_GE(new_scale, 0.f);
  window_scale_ = new_scale;
}

uint32_t WaylandWindow::GetPreferredEnteredOutputId() {
  // Child windows don't store entered outputs. Instead, take the window's
  // root parent window and use its preferred output.
  if (parent_window_)
    return GetRootParentWindow()->GetPreferredEnteredOutputId();

  // It can be either a toplevel window that hasn't entered any outputs yet, or
  // still a non toplevel window that doesn't have a parent (for example, a
  // wl_surface that is being dragged).
  if (root_surface_->entered_outputs().empty())
    return 0;

  // PlatformWindowType::kPopup are created as toplevel windows as well.
  DCHECK(type() == PlatformWindowType::kWindow ||
         type() == PlatformWindowType::kPopup);

  // A window can be located on two or more displays. Thus, return the id of the
  // output that has the biggest scale factor. Otherwise, use the very first one
  // that was entered. This way, we can be sure that the contents of the Window
  // are rendered at correct dpi when a user moves the window between displays.
  uint32_t preferred_output_id = *root_surface_->entered_outputs().begin();
  for (uint32_t output_id : root_surface_->entered_outputs()) {
    auto* output_manager = connection_->wayland_output_manager();
    auto* output = output_manager->GetOutput(output_id);
    auto* preferred_output = output_manager->GetOutput(preferred_output_id);
    // The compositor may have told the surface to enter the output that the
    // client is not aware of.  In such an event, we cannot evaluate scales, and
    // can only return the default, which means falling back to the primary
    // display in the code that calls this.
    // DCHECKS below are kept for trying to catch the situation in developer's
    // builds and find the way to reproduce the issue.
    // See crbug.com/1323635
    DCHECK(output) << " output " << output_id << " not found!";
    DCHECK(preferred_output)
        << " output " << preferred_output_id << " not found!";
    if (!output || !preferred_output)
      return 0;
    if (output->scale_factor() > preferred_output->scale_factor())
      preferred_output_id = output_id;
  }

  return preferred_output_id;
}

void WaylandWindow::SetPointerFocus(bool focus) {
  has_pointer_focus_ = focus;

  // Whenever the window gets the pointer focus back, the cursor shape must be
  // updated. Otherwise, it is invalidated upon wl_pointer::leave and is not
  // restored by the Wayland compositor.
  if (has_pointer_focus_ && cursor_)
    UpdateCursorShape(cursor_);
}

void WaylandWindow::RemoveEnteredOutput(uint32_t output_id) {
  root_surface_->RemoveEnteredOutput(output_id);
}

bool WaylandWindow::StartDrag(
    const ui::OSExchangeData& data,
    int operations,
    mojom::DragEventSource source,
    gfx::NativeCursor cursor,
    bool can_grab_pointer,
    WmDragHandler::DragFinishedCallback drag_finished_callback,
    WmDragHandler::LocationDelegate* location_delegate) {
  if (!connection_->data_drag_controller()->StartSession(data, operations,
                                                         source)) {
    return false;
  }

  DCHECK(drag_finished_callback_.is_null());
  drag_finished_callback_ = std::move(drag_finished_callback);

  base::RunLoop drag_loop(base::RunLoop::Type::kNestableTasksAllowed);
  drag_loop_quit_closure_ = drag_loop.QuitClosure();

  auto alive = weak_ptr_factory_.GetWeakPtr();
  drag_loop.Run();
  if (!alive)
    return false;
  return true;
}

void WaylandWindow::CancelDrag() {
  if (drag_loop_quit_closure_.is_null())
    return;
  std::move(drag_loop_quit_closure_).Run();
}

void WaylandWindow::Show(bool inactive) {
  frame_manager_->MaybeProcessPendingFrame();
}

void WaylandWindow::Hide() {
  can_submit_frames_ = false;

  // Mutter compositor crashes if we don't remove subsurface roles when hiding.
  if (primary_subsurface_) {
    primary_subsurface()->Hide();
  }
  for (auto& subsurface : wayland_subsurfaces_) {
    subsurface->Hide();
  }
  frame_manager_->Hide();
}

void WaylandWindow::OnChannelDestroyed() {
  frame_manager_->ClearStates();
  base::circular_deque<std::pair<WaylandSubsurface*, wl::WaylandOverlayConfig>>
      subsurfaces_to_overlays;
  subsurfaces_to_overlays.reserve(wayland_subsurfaces_.size() +
                                  (primary_subsurface() ? 1 : 0));
  if (primary_subsurface())
    subsurfaces_to_overlays.emplace_back(primary_subsurface(),
                                         wl::WaylandOverlayConfig());
  for (auto& subsurface : wayland_subsurfaces_)
    subsurfaces_to_overlays.emplace_back(subsurface.get(),
                                         wl::WaylandOverlayConfig());

  frame_manager_->RecordFrame(
      std::make_unique<WaylandFrame>(root_surface(), wl::WaylandOverlayConfig(),
                                     std::move(subsurfaces_to_overlays)));
}

void WaylandWindow::Close() {
  delegate_->OnClosed();
}

bool WaylandWindow::IsVisible() const {
  NOTREACHED();
  return false;
}

void WaylandWindow::PrepareForShutdown() {
  if (drag_finished_callback_)
    OnDragSessionClose(DragOperation::kNone);
}

void WaylandWindow::SetBoundsInPixels(const gfx::Rect& bounds_px) {
  gfx::Rect adjusted_bounds_px = AdjustBoundsToConstraintsPx(bounds_px);
  if (bounds_px_ == adjusted_bounds_px)
    return;
  bounds_px_ = adjusted_bounds_px;

  if (update_visual_size_immediately_)
    UpdateVisualSize(bounds_px.size(), window_scale());
  delegate_->OnBoundsChanged(bounds_px_);
}

gfx::Rect WaylandWindow::GetBoundsInPixels() const {
  return bounds_px_;
}

void WaylandWindow::SetBoundsInDIP(const gfx::Rect& bounds) {
  SetBoundsInPixels(delegate_->ConvertRectToPixels(bounds));
}

gfx::Rect WaylandWindow::GetBoundsInDIP() const {
  return delegate_->ConvertRectToDIP(bounds_px_);
}

void WaylandWindow::OnSurfaceConfigureEvent() {
  if (can_submit_frames_)
    return;
  can_submit_frames_ = true;
  frame_manager_->MaybeProcessPendingFrame();
}

void WaylandWindow::SetTitle(const std::u16string& title) {}

void WaylandWindow::SetCapture() {
  // Wayland doesn't allow explicit grabs. Instead, it sends events to "entered"
  // windows. That is, if user enters their mouse pointer to a window, that
  // window starts to receive events. However, Chromium may want to reroute
  // these events to another window. In this case, tell the window manager that
  // this specific window has grabbed the events, and they will be rerouted in
  // WaylandWindow::DispatchEvent method.
  if (!HasCapture())
    connection_->wayland_window_manager()->GrabLocatedEvents(this);
}

void WaylandWindow::ReleaseCapture() {
  if (HasCapture())
    connection_->wayland_window_manager()->UngrabLocatedEvents(this);
  // See comment in SetCapture() for details on wayland and grabs.
}

bool WaylandWindow::HasCapture() const {
  return connection_->wayland_window_manager()->located_events_grabber() ==
         this;
}

void WaylandWindow::ToggleFullscreen() {}

void WaylandWindow::Maximize() {}

void WaylandWindow::Minimize() {}

void WaylandWindow::Restore() {}

PlatformWindowState WaylandWindow::GetPlatformWindowState() const {
  // Remove normal state for all the other types of windows as it's only the
  // WaylandToplevelWindow that supports state changes.
  return PlatformWindowState::kNormal;
}

void WaylandWindow::Activate() {}

void WaylandWindow::Deactivate() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SetUseNativeFrame(bool use_native_frame) {
  // Do nothing here since only shell surfaces can handle server-side
  // decoration.
}

bool WaylandWindow::ShouldUseNativeFrame() const {
  // Always returns false here since only shell surfaces can handle server-side
  // decoration.
  return false;
}

void WaylandWindow::SetCursor(scoped_refptr<PlatformCursor> platform_cursor) {
  DCHECK(platform_cursor);

  if (cursor_ == platform_cursor)
    return;

  UpdateCursorShape(BitmapCursor::FromPlatformCursor(platform_cursor));
}

void WaylandWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WaylandWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void WaylandWindow::SetRestoredBoundsInDIP(const gfx::Rect& bounds) {
  restored_size_dip_ = bounds.size();
}

gfx::Rect WaylandWindow::GetRestoredBoundsInDIP() const {
  return gfx::Rect(restored_size_dip_);
}

bool WaylandWindow::ShouldWindowContentsBeTransparent() const {
  // Wayland compositors always support translucency.
  return true;
}

void WaylandWindow::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool WaylandWindow::IsTranslucentWindowOpacitySupported() const {
  // Wayland compositors always support translucency.
  return true;
}

void WaylandWindow::SetDecorationInsets(const gfx::Insets* insets_px) {
  if ((!frame_insets_px_ && !insets_px) ||
      (frame_insets_px_ && insets_px && *frame_insets_px_ == *insets_px)) {
    return;
  }
  if (insets_px)
    frame_insets_px_ = *insets_px;
  else
    frame_insets_px_ = absl::nullopt;
  UpdateDecorations();
  connection_->ScheduleFlush();
}

void WaylandWindow::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                   const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WaylandWindow::SizeConstraintsChanged() {}

bool WaylandWindow::ShouldUpdateWindowShape() const {
  return false;
}

bool WaylandWindow::CanDispatchEvent(const PlatformEvent& event) {
  if (event->IsMouseEvent() || event->IsPinchEvent())
    return has_pointer_focus_;
  if (event->IsKeyEvent())
    return has_keyboard_focus_;
  if (event->IsTouchEvent())
    return has_touch_focus_;
  if (event->IsScrollEvent())
    return has_pointer_focus_;
  return false;
}

uint32_t WaylandWindow::DispatchEvent(const PlatformEvent& native_event) {
  Event* event = static_cast<Event*>(native_event);

  if (event->IsLocatedEvent()) {
    auto* event_grabber =
        connection_->wayland_window_manager()->located_events_grabber();
    auto* root_parent_window = GetRootParentWindow();

    UpdateCursorPositionFromEvent(event);

    // We must reroute the events to the event grabber iff these windows belong
    // to the same root parent window. For example, there are 2 top level
    // Wayland windows. One of them (window_1) has a child menu window that is
    // the event grabber. If the mouse is moved over the window_1, it must
    // reroute the events to the event grabber. If the mouse is moved over the
    // window_2, the events mustn't be rerouted, because that belongs to another
    // stack of windows. Remember that Wayland sends local surface coordinates,
    // and continuing rerouting all the events may result in events sent to the
    // grabber even though the mouse is over another root window.
    //
    bool send_to_grabber =
        event_grabber &&
        root_parent_window == event_grabber->GetRootParentWindow();
    if (send_to_grabber) {
      ConvertEventLocationToTargetWindowLocation(
          event_grabber->GetBoundsInDIP().origin(), GetBoundsInDIP().origin(),
          event->AsLocatedEvent());
    }

    // Wayland sends locations in DIP so they need to be translated to
    // physical pixels.
    event->AsLocatedEvent()->set_location_f(gfx::ScalePoint(
        event->AsLocatedEvent()->location_f(), window_scale(), window_scale()));

    if (send_to_grabber)
      return event_grabber->DispatchEventToDelegate(native_event);
  }

  // Dispatch all keyboard events to the root window.
  if (event->IsKeyEvent())
    return GetRootParentWindow()->DispatchEventToDelegate(event);

  return DispatchEventToDelegate(native_event);
}

void WaylandWindow::HandleSurfaceConfigure(uint32_t serial) {
  NOTREACHED()
      << "Only shell surfaces must receive HandleSurfaceConfigure calls.";
}

void WaylandWindow::HandleToplevelConfigure(int32_t widht,
                                            int32_t height,
                                            bool is_maximized,
                                            bool is_fullscreen,
                                            bool is_activated) {
  NOTREACHED()
      << "Only shell toplevels must receive HandleToplevelConfigure calls.";
}

void WaylandWindow::HandleAuraToplevelConfigure(int32_t x,
                                                int32_t y,
                                                int32_t width,
                                                int32_t height,
                                                bool is_maximized,
                                                bool is_fullscreen,
                                                bool is_activated) {
  NOTREACHED()
      << "Only shell toplevels must receive HandleAuraToplevelConfigure calls.";
}

void WaylandWindow::HandlePopupConfigure(const gfx::Rect& bounds_dip) {
  NOTREACHED() << "Only shell popups must receive HandlePopupConfigure calls.";
}

void WaylandWindow::UpdateVisualSize(const gfx::Size& size_px,
                                     float scale_factor) {
  if (visual_size_px_ == size_px)
    return;
  visual_size_px_ = size_px;
  UpdateWindowMask();

  if (apply_pending_state_on_update_visual_size_) {
    root_surface_->ApplyPendingState();
    connection_->ScheduleFlush();
  }
}

void WaylandWindow::OnCloseRequest() {
  delegate_->OnCloseRequest();
}

absl::optional<std::vector<gfx::Rect>> WaylandWindow::GetWindowShape() const {
  return absl::nullopt;
}

void WaylandWindow::OnDragEnter(const gfx::PointF& point,
                                std::unique_ptr<OSExchangeData> data,
                                int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;

  // TODO(crbug.com/1102857): get the real event modifier here.
  drop_handler->OnDragEnter(point, std::move(data), operation,
                            /*modifiers=*/0);
}

int WaylandWindow::OnDragMotion(const gfx::PointF& point, int operation) {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return 0;

  // TODO(crbug.com/1102857): get the real event modifier here.
  return drop_handler->OnDragMotion(point, operation,
                                    /*modifiers=*/0);
}

void WaylandWindow::OnDragDrop() {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;
  // TODO(crbug.com/1102857): get the real event modifier here.
  drop_handler->OnDragDrop({}, /*modifiers=*/0);
}

void WaylandWindow::OnDragLeave() {
  WmDropHandler* drop_handler = GetWmDropHandler(*this);
  if (!drop_handler)
    return;
  drop_handler->OnDragLeave();
}

void WaylandWindow::OnDragSessionClose(DragOperation operation) {
  DCHECK(drag_finished_callback_);
  std::move(drag_finished_callback_).Run(operation);
  connection()->event_source()->ResetPointerFlags();
  std::move(drag_loop_quit_closure_).Run();
}

void WaylandWindow::UpdateBoundsInDIP(const gfx::Rect& bounds_dip) {
  // This method is used to update the content size by calling WindowWindow's
  // SetBounds, instead of WaylandToplevelWindow's override, which sends a
  // request to the compositor.
  WaylandWindow::SetBoundsInPixels(delegate_->ConvertRectToPixels(bounds_dip));
}

bool WaylandWindow::Initialize(PlatformWindowInitProperties properties) {
  root_surface_ = std::make_unique<WaylandSurface>(connection_, this);
  if (!root_surface_->Initialize()) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }

  if (properties.inhibit_keyboard_shortcuts)
    root_surface_->InhibitKeyboardShortcuts();

  // Update visual size in tests immediately if the test config is set.
  // Otherwise, such tests as interactive_ui_tests fail.
  if (!update_visual_size_immediately_)
    set_update_visual_size_immediately(UseTestConfigForPlatformWindows());

  // Properties contain DIP bounds but the buffer scale is initially 1 so it's
  // OK to assign.  The bounds will be recalculated when the buffer scale
  // changes.
  bounds_px_ = properties.bounds;
  opacity_ = properties.opacity;
  type_ = properties.type;

  connection_->wayland_window_manager()->AddWindow(GetWidget(), this);

  if (!OnInitialize(std::move(properties)))
    return false;

  if (wayland_overlay_delegation_enabled_) {
    primary_subsurface_ =
        std::make_unique<WaylandSubsurface>(connection_, this);
    if (!primary_subsurface_->surface())
      return false;
    connection_->wayland_window_manager()->AddSubsurface(
        GetWidget(), primary_subsurface_.get());
  }

  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(GetWidget());

  std::vector<gfx::Rect> region{gfx::Rect{bounds_px_.size()}};
  root_surface_->SetOpaqueRegion(&region);
  root_surface_->ApplyPendingState();
  connection_->ScheduleFlush();

  return true;
}

void WaylandWindow::SetWindowGeometry(gfx::Rect bounds) {}

void WaylandWindow::UpdateDecorations() {}

WaylandWindow* WaylandWindow::GetRootParentWindow() {
  return parent_window_ ? parent_window_->GetRootParentWindow() : this;
}

void WaylandWindow::OnEnteredOutput() {
  // Wayland does weird things for menus so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (AsWaylandPopup())
    return;

  UpdateWindowScale(true);
}

void WaylandWindow::OnLeftOutput() {
  // Wayland does weird things for menus so instead of tracking outputs that
  // we entered or left, we take that from the parent window and ignore this
  // event.
  if (AsWaylandPopup())
    return;
  // Do not update the window scale where. It'll be updated when entring a new
  // output.
}

void WaylandWindow::UpdateCursorPositionFromEvent(const Event* orig_event) {
  DCHECK(orig_event->IsLocatedEvent());

  // This is a tricky part. Initially, Wayland sends events to surfaces the
  // events are targeted for. But, in order to fulfill Chromium's assumptions
  // about event targets, some of the events are rerouted and their locations
  // are converted. The event we got here is rerouted and it has had its
  // location fixed.
  //
  // Basically, this method must translate coordinates of all events
  // in regards to top-level windows' coordinates as it's always located at
  // origin (0,0) from Chromium point of view (remember that wl_shell/xdg_shell
  // doesn't provide global coordinates to its clients). And it's totally fine
  // to use it as the target. Thus, the location of the |event| is always
  // converted using the top-level window's bounds as the target excluding
  // cases, when the mouse/touch is over a top-level window.
  auto* cursor_position = connection_->wayland_cursor_position();
  if (!cursor_position)
    return;
  const LocatedEvent* located_event = orig_event->AsLocatedEvent();
  std::unique_ptr<Event> event;

  auto* toplevel_window = GetRootParentWindow();
  if (toplevel_window != this) {
    event = Event::Clone(*orig_event);
    ConvertEventLocationToTargetWindowLocation(
        toplevel_window->GetBoundsInDIP().origin(), GetBoundsInDIP().origin(),
        event->AsLocatedEvent());

    located_event = event->AsLocatedEvent();
  }

  cursor_position->OnCursorPositionChanged(
      located_event->location() +
      toplevel_window->GetBoundsInDIP().origin().OffsetFromOrigin());
}

WaylandWindow* WaylandWindow::GetTopMostChildWindow() {
  return child_window_ ? child_window_->GetTopMostChildWindow() : this;
}

bool WaylandWindow::IsOpaqueWindow() const {
  return opacity_ == ui::PlatformWindowOpacity::kOpaqueWindow;
}

bool WaylandWindow::IsActive() const {
  // Please read the comment where the IsActive method is declared.
  return false;
}

WaylandPopup* WaylandWindow::AsWaylandPopup() {
  return nullptr;
}

uint32_t WaylandWindow::DispatchEventToDelegate(
    const PlatformEvent& native_event) {
  bool handled = DispatchEventFromNativeUiEvent(
      native_event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                                   base::Unretained(delegate_)));
  return handled ? POST_DISPATCH_STOP_PROPAGATION : POST_DISPATCH_NONE;
}

std::unique_ptr<WaylandSurface> WaylandWindow::TakeWaylandSurface() {
  DCHECK(shutting_down_);
  DCHECK(root_surface_);
  root_surface_->UnsetRootWindow();
  return std::move(root_surface_);
}

bool WaylandWindow::RequestSubsurface() {
  auto subsurface = std::make_unique<WaylandSubsurface>(connection_, this);
  if (!subsurface->surface())
    return false;
  connection_->wayland_window_manager()->AddSubsurface(GetWidget(),
                                                       subsurface.get());
  subsurface_stack_above_.push_back(subsurface.get());
  auto result = wayland_subsurfaces_.emplace(std::move(subsurface));
  DCHECK(result.second);
  return true;
}

bool WaylandWindow::ArrangeSubsurfaceStack(size_t above, size_t below) {
  while (wayland_subsurfaces_.size() < above + below) {
    if (!RequestSubsurface())
      return false;
  }

  DCHECK(subsurface_stack_below_.size() + subsurface_stack_above_.size() >=
         above + below);

  if (subsurface_stack_above_.size() < above) {
    auto splice_start = subsurface_stack_below_.begin();
    for (size_t i = 0; i < below; ++i)
      ++splice_start;
    subsurface_stack_above_.splice(subsurface_stack_above_.end(),
                                   subsurface_stack_below_, splice_start,
                                   subsurface_stack_below_.end());

  } else if (subsurface_stack_below_.size() < below) {
    auto splice_start = subsurface_stack_above_.end();
    for (size_t i = 0; i < below - subsurface_stack_below_.size(); ++i)
      --splice_start;
    subsurface_stack_below_.splice(subsurface_stack_below_.end(),
                                   subsurface_stack_above_, splice_start,
                                   subsurface_stack_above_.end());
  }

  DCHECK(subsurface_stack_below_.size() >= below);
  DCHECK(subsurface_stack_above_.size() >= above);
  return true;
}

bool WaylandWindow::CommitOverlays(
    uint32_t frame_id,
    std::vector<wl::WaylandOverlayConfig>& overlays) {
  if (overlays.empty())
    return true;

  // |overlays| is sorted from bottom to top.
  std::sort(overlays.begin(), overlays.end(), OverlayStackOrderCompare);

  // Find the location where z_oder becomes non-negative.
  wl::WaylandOverlayConfig value;
  auto split = std::lower_bound(overlays.begin(), overlays.end(), value,
                                OverlayStackOrderCompare);
  DCHECK(split == overlays.end() || (*split).z_order >= 0);
  size_t num_primary_planes =
      (split != overlays.end() && (*split).z_order == 0) ? 1 : 0;
  size_t num_background_planes =
      (overlays.front().z_order == INT32_MIN) ? 1 : 0;

  size_t above = (overlays.end() - split) - num_primary_planes;
  size_t below = (split - overlays.begin()) - num_background_planes;

  // Re-arrange the list of subsurfaces to fit the |overlays|. Request extra
  // subsurfaces if needed.
  if (!ArrangeSubsurfaceStack(above, below))
    return false;

  gfx::SizeF visual_size = (*overlays.begin()).bounds_rect.size();
  float buffer_scale = (*overlays.begin()).surface_scale_factor;
  auto& rounded_clip_bounds = (*overlays.begin()).rounded_clip_bounds;

  if (!wayland_overlay_delegation_enabled_) {
    DCHECK_EQ(overlays.size(), 1u);
    frame_manager_->RecordFrame(std::make_unique<WaylandFrame>(
        frame_id, root_surface(), std::move(*overlays.begin())));
    return true;
  }

  base::circular_deque<std::pair<WaylandSubsurface*, wl::WaylandOverlayConfig>>
      subsurfaces_to_overlays;
  subsurfaces_to_overlays.reserve(
      std::max(overlays.size() - num_background_planes,
               wayland_subsurfaces_.size() + 1));

  subsurfaces_to_overlays.emplace_back(
      primary_subsurface(),
      num_primary_planes ? std::move(*split) : wl::WaylandOverlayConfig());

  {
    // Iterate through |subsurface_stack_below_|, setup subsurfaces and place
    // them in corresponding order. Commit wl_buffers once a subsurface is
    // configured.
    auto overlay_iter = split - 1;
    for (auto iter = subsurface_stack_below_.begin();
         iter != subsurface_stack_below_.end(); ++iter, --overlay_iter) {
      if (overlay_iter >= overlays.begin() + num_background_planes) {
        subsurfaces_to_overlays.emplace_front(*iter, std::move(*overlay_iter));
      } else {
        // If there're more subsurfaces requested that we don't need at the
        // moment, hide them.
        subsurfaces_to_overlays.emplace_front(*iter,
                                              wl::WaylandOverlayConfig());
      }
    }

    // Iterate through |subsurface_stack_above_|, setup subsurfaces and place
    // them in corresponding order. Commit wl_buffers once a subsurface is
    // configured.
    overlay_iter = split + num_primary_planes;
    for (auto iter = subsurface_stack_above_.begin();
         iter != subsurface_stack_above_.end(); ++iter, ++overlay_iter) {
      if (overlay_iter < overlays.end()) {
        subsurfaces_to_overlays.emplace_back(*iter, std::move(*overlay_iter));
      } else {
        // If there're more subsurfaces requested that we don't need at the
        // moment, hide them.
        subsurfaces_to_overlays.emplace_back(*iter, wl::WaylandOverlayConfig());
      }
    }
  }

  // Configuration of the root_surface
  wl::WaylandOverlayConfig root_config;
  if (num_background_planes) {
    root_config = std::move(overlays.front());
  } else {
    root_config = wl::WaylandOverlayConfig(
        gfx::OverlayPlaneData(
            INT32_MIN, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
            gfx::RectF(visual_size), gfx::RectF(),
            root_surface()->use_blending(), gfx::Rect(),
            root_surface()->opacity(), gfx::OverlayPriorityHint::kNone,
            rounded_clip_bounds, gfx::ColorSpace(), absl::nullopt),
        nullptr, root_surface()->buffer_id(), buffer_scale);
  }

  frame_manager_->RecordFrame(std::make_unique<WaylandFrame>(
      frame_id, root_surface(), std::move(root_config),
      std::move(subsurfaces_to_overlays)));

  return true;
}

void WaylandWindow::UpdateCursorShape(scoped_refptr<BitmapCursor> cursor) {
  DCHECK(cursor);
  absl::optional<int32_t> shape =
      WaylandZcrCursorShapes::ShapeFromType(cursor->type());

  // Round cursor scale factor to ceil as wl_surface.set_buffer_scale accepts
  // only integers.
  if (cursor->type() == CursorType::kNone) {  // Hide the cursor.
    connection_->SetCursorBitmap(
        {}, gfx::Point(), std::ceil(cursor->cursor_image_scale_factor()));
  } else if (cursor->platform_data()) {  // Check for theme-provided cursor.
    connection_->SetPlatformCursor(
        reinterpret_cast<wl_cursor*>(cursor->platform_data()),
        std::ceil(cursor->cursor_image_scale_factor()));
  } else if (connection_->zcr_cursor_shapes() &&
             shape.has_value()) {  // Check for Wayland server-side cursor
                                   // support (e.g. exo for lacros).
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Lacros should not load image assets for default cursors. See
    // `BitmapCursorFactory::GetDefaultCursor()`.
    DCHECK(cursor->bitmaps().empty());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    connection_->zcr_cursor_shapes()->SetCursorShape(shape.value());
  } else {  // Use client-side bitmap cursors as fallback.
    // Translate physical pixels to DIPs.
    gfx::Point hotspot_in_dips =
        gfx::ScaleToRoundedPoint(cursor->hotspot(), 1.0f / ui_scale_);
    connection_->SetCursorBitmap(
        cursor->bitmaps(), hotspot_in_dips,
        std::ceil(cursor->cursor_image_scale_factor()));
  }
  // The new cursor needs to be stored last to avoid deleting the old cursor
  // while it's still in use.
  cursor_ = cursor;
}

void WaylandWindow::ProcessPendingBoundsDip(uint32_t serial) {
  if (pending_bounds_dip_.IsEmpty() &&
      GetPlatformWindowState() == PlatformWindowState::kMinimized &&
      pending_configures_.empty()) {
    // In exo, widget creation is deferred until the surface has contents and
    // |initial_show_state_| for a widget is ignored. Exo sends a configure
    // callback with empty bounds expecting client to suggest a size.
    // For the window activated from minimized state,
    // the saved window placement should be set as window geometry.
    gfx::Rect bounds_in_dip = GetBoundsInDIP();
    // As per spec, width and height must be greater than zero.
    if (bounds_in_dip.IsEmpty())
      bounds_in_dip = gfx::Rect(0, 0, 1, 1);
    SetWindowGeometry(bounds_in_dip);
    AckConfigure(serial);
    root_surface()->Commit();
  } else if (delegate()->ConvertRectToPixels(pending_bounds_dip_) ==
                 GetBoundsInPixels() &&
             pending_configures_.empty()) {
    // If |pending_bounds_dip_| matches the current window bounds, and
    // |pending_configures_| is empty, which implies that the window is already
    // rendering at |pending_bounds_dip_|, then a new frame matching it may take
    // some time to arrive, despite the window delegate receives the updated
    // bounds. Without a new frame, UpdateVisualSize() is not invoked, leaving
    // this configure sequence unacknowledged. E.g: With static window content,
    // a configure sequence that does not change the window size will not cause
    // the window to redraw. Hence, acknowledge this configure sequence now to
    // tell the Wayland compositor that the requested configuration for this
    // window has been applied.
    SetWindowGeometry(pending_bounds_dip_);
    AckConfigure(serial);
    connection()->ScheduleFlush();
  } else if (!pending_configures_.empty() &&
             pending_bounds_dip_.size() ==
                 pending_configures_.back().bounds_dip.size()) {
    // There is an existing pending_configure with the same size, do not push a
    // new one. Instead, update the serial of the pending_configure.
    pending_configures_.back().serial = serial;
  } else {
    // Otherwise, push the pending |configure| to |pending_configures_|, wait
    // for a frame update, which will invoke UpdateVisualSize().
    LOG_IF(WARNING, pending_configures_.size() > 100u)
        << "The queue of configures is longer than 100!";
    pending_configures_.push_back({pending_bounds_dip_, serial});
    // The Wayland compositor can generate xdg-shell.configure events more
    // frequently than frame updates from gpu process. Throttle
    // ApplyPendingBounds() such that we forward new bounds to
    // PlatformWindowDelegate at most once per frame.
    if (pending_configures_.size() <= 1)
      ApplyPendingBounds();
  }
}

gfx::Rect WaylandWindow::AdjustBoundsToConstraintsPx(
    const gfx::Rect& bounds_px) {
  gfx::Rect adjusted_bounds_px = bounds_px;
  if (const auto min_size = delegate_->GetMinimumSizeForWindow()) {
    gfx::Size min_size_in_px =
        delegate()->ConvertRectToPixels(gfx::Rect(*min_size)).size();
    if (min_size_in_px.width() > 0 &&
        adjusted_bounds_px.width() < min_size_in_px.width()) {
      adjusted_bounds_px.set_width(min_size_in_px.width());
    }
    if (min_size_in_px.height() > 0 &&
        adjusted_bounds_px.height() < min_size_in_px.height()) {
      adjusted_bounds_px.set_height(min_size_in_px.height());
    }
  }
  if (const auto max_size = delegate_->GetMaximumSizeForWindow()) {
    gfx::Size max_size_in_px =
        delegate()->ConvertRectToPixels(gfx::Rect(*max_size)).size();
    if (max_size_in_px.width() > 0 &&
        adjusted_bounds_px.width() > max_size_in_px.width()) {
      adjusted_bounds_px.set_width(max_size_in_px.width());
    }
    if (max_size_in_px.height() > 0 &&
        adjusted_bounds_px.height() > max_size_in_px.height()) {
      adjusted_bounds_px.set_height(max_size_in_px.height());
    }
  }
  return adjusted_bounds_px;
}

gfx::Rect WaylandWindow::AdjustBoundsToConstraintsDIP(
    const gfx::Rect& bounds_dip) {
  gfx::Rect adjusted_bounds_dip = bounds_dip;
  if (const auto min_size_dip = delegate_->GetMinimumSizeForWindow()) {
    if (min_size_dip->width() > 0 &&
        adjusted_bounds_dip.width() < min_size_dip->width()) {
      adjusted_bounds_dip.set_width(min_size_dip->width());
    }
    if (min_size_dip->height() > 0 &&
        adjusted_bounds_dip.height() < min_size_dip->height()) {
      adjusted_bounds_dip.set_height(min_size_dip->height());
    }
  }
  if (const auto max_size_dip = delegate_->GetMaximumSizeForWindow()) {
    if (max_size_dip->width() > 0 &&
        adjusted_bounds_dip.width() > max_size_dip->width()) {
      adjusted_bounds_dip.set_width(max_size_dip->width());
    }
    if (max_size_dip->height() > 0 &&
        adjusted_bounds_dip.height() > max_size_dip->height()) {
      adjusted_bounds_dip.set_height(max_size_dip->height());
    }
  }
  return adjusted_bounds_dip;
}

bool WaylandWindow::ProcessVisualSizeUpdate(const gfx::Size& size_px,
                                            float scale_factor) {
  // TODO(crbug.com/1307501): Optimize this to be less expensive. Maybe
  // precompute in pixels for configure events. pending_configures_ can have 10s
  // of elements in it for several frames under some conditions.
  // The `pending_configures_` should store px size instead of dip.
  auto result = std::find_if(
      pending_configures_.begin(), pending_configures_.end(),
      [this, &size_px, &scale_factor](auto& configure) {
        // Since size_px comes from SetBounds via UpdateVisualSize in
        // WaylandTopLevelWindow, we also need to adjust it for bounds to see if
        // we match.
        return AdjustBoundsToConstraintsPx(
                   gfx::ScaleToEnclosingRect(configure.bounds_dip,
                                             scale_factor))
                       .size() == size_px &&
               configure.set;
      });

  if (result != pending_configures_.end()) {
    auto serial = result->serial;
    SetWindowGeometry(result->bounds_dip);
    AckConfigure(serial);
    connection()->ScheduleFlush();
    pending_configures_.erase(pending_configures_.begin(), ++result);
    return true;
  }
  return false;
}

void WaylandWindow::ApplyPendingBounds() {
  DCHECK(!pending_configures_.empty());
  for (auto& configure : pending_configures_)
    configure.set = true;
  UpdateBoundsInDIP(pending_configures_.back().bounds_dip);
}

bool WaylandWindow::HasPendingConfigures() const {
  return !pending_configures_.empty();
}

}  // namespace ui
