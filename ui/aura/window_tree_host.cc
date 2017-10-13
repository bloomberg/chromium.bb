// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host.h"

#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/switches.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_port.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/layout.h"
#include "ui/base/view_prop.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/icc_profile.h"

namespace aura {

const char kWindowTreeHostForAcceleratedWidget[] =
    "__AURA_WINDOW_TREE_HOST_ACCELERATED_WIDGET__";

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHost, public:

WindowTreeHost::~WindowTreeHost() {
  if (display::Screen::GetScreen())
    display::Screen::GetScreen()->RemoveObserver(this);
  DCHECK(!compositor_) << "compositor must be destroyed before root window";
  if (owned_input_method_) {
    delete input_method_;
    input_method_ = nullptr;
  }
}

// static
WindowTreeHost* WindowTreeHost::GetForAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  return reinterpret_cast<WindowTreeHost*>(
      ui::ViewProp::GetValue(widget, kWindowTreeHostForAcceleratedWidget));
}

void WindowTreeHost::InitHost() {
  InitCompositor();
  UpdateRootWindowSizeInPixels(GetBoundsInPixels().size());
  Env::GetInstance()->NotifyHostInitialized(this);
}

void WindowTreeHost::AddObserver(WindowTreeHostObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowTreeHost::RemoveObserver(WindowTreeHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

ui::EventSink* WindowTreeHost::event_sink() {
  return dispatcher_.get();
}

gfx::Transform WindowTreeHost::GetRootTransform() const {
  float scale = ui::GetDeviceScaleFactor(window()->layer());
  gfx::Transform transform;
  transform.Scale(scale, scale);
  transform *= window()->layer()->transform();
  return transform;
}

void WindowTreeHost::SetRootTransform(const gfx::Transform& transform) {
  window()->SetTransform(transform);
  UpdateRootWindowSizeInPixels(GetBoundsInPixels().size());
}

gfx::Transform WindowTreeHost::GetInverseRootTransform() const {
  gfx::Transform invert;
  gfx::Transform transform = GetRootTransform();
  if (!transform.GetInverse(&invert))
    return transform;
  return invert;
}

gfx::Transform WindowTreeHost::GetRootTransformForLocalEventCoordinates()
    const {
  return GetRootTransform();
}

gfx::Transform WindowTreeHost::GetInverseRootTransformForLocalEventCoordinates()
    const {
  gfx::Transform invert;
  gfx::Transform transform = GetRootTransformForLocalEventCoordinates();
  if (!transform.GetInverse(&invert))
    return transform;
  return invert;
}

void WindowTreeHost::SetOutputSurfacePaddingInPixels(
    const gfx::Insets& padding_in_pixels) {
  if (output_surface_padding_in_pixels_ == padding_in_pixels)
    return;

  output_surface_padding_in_pixels_ = padding_in_pixels;
  OnHostResizedInPixels(GetBoundsInPixels().size());
}

void WindowTreeHost::UpdateRootWindowSizeInPixels(
    const gfx::Size& host_size_in_pixels) {
  gfx::Rect bounds(output_surface_padding_in_pixels_.left(),
                   output_surface_padding_in_pixels_.top(),
                   host_size_in_pixels.width(), host_size_in_pixels.height());
  float scale_factor = ui::GetDeviceScaleFactor(window()->layer());
  gfx::RectF new_bounds =
      gfx::ScaleRect(gfx::RectF(bounds), 1.0f / scale_factor);
  window()->layer()->transform().TransformRect(&new_bounds);
  window()->SetBounds(gfx::ToEnclosingRect(new_bounds));
}

void WindowTreeHost::ConvertDIPToScreenInPixels(gfx::Point* point) const {
  ConvertDIPToPixels(point);
  gfx::Point location = GetLocationOnScreenInPixels();
  point->Offset(location.x(), location.y());
}

void WindowTreeHost::ConvertScreenInPixelsToDIP(gfx::Point* point) const {
  gfx::Point location = GetLocationOnScreenInPixels();
  point->Offset(-location.x(), -location.y());
  ConvertPixelsToDIP(point);
}

void WindowTreeHost::ConvertDIPToPixels(gfx::Point* point) const {
  auto point_3f = gfx::Point3F(gfx::PointF(*point));
  GetRootTransform().TransformPoint(&point_3f);
  *point = gfx::ToFlooredPoint(point_3f.AsPointF());
}

void WindowTreeHost::ConvertPixelsToDIP(gfx::Point* point) const {
  auto point_3f = gfx::Point3F(gfx::PointF(*point));
  GetInverseRootTransform().TransformPoint(&point_3f);
  *point = gfx::ToFlooredPoint(point_3f.AsPointF());
}

void WindowTreeHost::SetCursor(gfx::NativeCursor cursor) {
  last_cursor_ = cursor;
  // A lot of code seems to depend on NULL cursors actually showing an arrow,
  // so just pass everything along to the host.
  SetCursorNative(cursor);
}

void WindowTreeHost::OnCursorVisibilityChanged(bool show) {
  // Clear any existing mouse hover effects when the cursor becomes invisible.
  // Note we do not need to dispatch a mouse enter when the cursor becomes
  // visible because that can only happen in response to a mouse event, which
  // will trigger its own mouse enter.
  if (!show) {
    ui::EventDispatchDetails details = dispatcher()->DispatchMouseExitAtPoint(
        nullptr, dispatcher()->GetLastMouseLocationInRoot(),
        ui::EF_CURSOR_HIDE);
    if (details.dispatcher_destroyed)
      return;
  }

  OnCursorVisibilityChangedNative(show);
}

void WindowTreeHost::MoveCursorToLocationInDIP(
    const gfx::Point& location_in_dip) {
  gfx::Point host_location(location_in_dip);
  ConvertDIPToPixels(&host_location);
  MoveCursorToInternal(location_in_dip, host_location);
}

void WindowTreeHost::MoveCursorToLocationInPixels(
    const gfx::Point& location_in_pixels) {
  gfx::Point root_location(location_in_pixels);
  ConvertPixelsToDIP(&root_location);
  MoveCursorToInternal(root_location, location_in_pixels);
}

ui::InputMethod* WindowTreeHost::GetInputMethod() {
  if (!input_method_) {
    input_method_ =
        ui::CreateInputMethod(this, GetAcceleratedWidget()).release();
    owned_input_method_ = true;
  }
  return input_method_;
}

void WindowTreeHost::SetSharedInputMethod(ui::InputMethod* input_method) {
  DCHECK(!input_method_);
  input_method_ = input_method;
  owned_input_method_ = false;
}

ui::EventDispatchDetails WindowTreeHost::DispatchKeyEventPostIME(
    ui::KeyEvent* event) {
  // If dispatch to IME is already disabled we shouldn't reach here.
  DCHECK(!dispatcher_->should_skip_ime());
  dispatcher_->set_skip_ime(true);
  // We should bypass event rewriters here as they've been tried before.
  ui::EventDispatchDetails dispatch_details =
      event_sink()->OnEventFromSource(event);
  if (!dispatch_details.dispatcher_destroyed)
    dispatcher_->set_skip_ime(false);
  return dispatch_details;
}

void WindowTreeHost::Show() {
  // Ensure that compositor has been properly initialized, see InitCompositor()
  // and InitHost().
  DCHECK(compositor());
  DCHECK_EQ(compositor()->root_layer(), window()->layer());
  compositor()->SetVisible(true);
  ShowImpl();
  window()->Show();
}

void WindowTreeHost::Hide() {
  HideImpl();
  if (compositor())
    compositor()->SetVisible(false);
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHost, protected:

WindowTreeHost::WindowTreeHost() : WindowTreeHost(nullptr) {
}

WindowTreeHost::WindowTreeHost(std::unique_ptr<WindowPort> window_port)
    : window_(new Window(nullptr, std::move(window_port))),
      last_cursor_(ui::CursorType::kNull),
      input_method_(nullptr),
      owned_input_method_(false) {
  display::Screen::GetScreen()->AddObserver(this);
}

void WindowTreeHost::DestroyCompositor() {
  if (compositor_) {
    compositor_->RemoveObserver(this);
    compositor_.reset();
  }
}

void WindowTreeHost::DestroyDispatcher() {
  delete window_;
  window_ = nullptr;
  dispatcher_.reset();

  // TODO(beng): this comment is no longer quite valid since this function
  // isn't called from WED, and WED isn't a subclass of Window. So it seems
  // like we could just rely on ~Window now.
  // Destroy child windows while we're still valid. This is also done by
  // ~Window, but by that time any calls to virtual methods overriden here (such
  // as GetRootWindow()) result in Window's implementation. By destroying here
  // we ensure GetRootWindow() still returns this.
  //window()->RemoveOrDestroyChildren();
}

void WindowTreeHost::CreateCompositor(const viz::FrameSinkId& frame_sink_id,
                                      bool force_software_compositor,
                                      bool external_begin_frames_enabled) {
  DCHECK(Env::GetInstance());
  ui::ContextFactory* context_factory = Env::GetInstance()->context_factory();
  DCHECK(context_factory);
  ui::ContextFactoryPrivate* context_factory_private =
      Env::GetInstance()->context_factory_private();
  bool enable_surface_synchronization =
      aura::Env::GetInstance()->mode() == aura::Env::Mode::MUS ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSurfaceSynchronization);
  compositor_.reset(new ui::Compositor(
      (!context_factory_private || frame_sink_id.is_valid())
          ? frame_sink_id
          : context_factory_private->AllocateFrameSinkId(),
      context_factory, context_factory_private,
      base::ThreadTaskRunnerHandle::Get(), enable_surface_synchronization,
      ui::IsPixelCanvasRecordingEnabled(), external_begin_frames_enabled,
      force_software_compositor));
  compositor_->AddObserver(this);
  if (!dispatcher()) {
    window()->Init(ui::LAYER_NOT_DRAWN);
    window()->set_host(this);
    window()->SetName("RootWindow");
    dispatcher_.reset(new WindowEventDispatcher(this));
  }
}

void WindowTreeHost::InitCompositor() {
  DCHECK(!compositor_->root_layer());
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window());
  compositor_->SetScaleAndSize(display.device_scale_factor(),
                               GetBoundsInPixels().size(),
                               window()->GetLocalSurfaceId());
  compositor_->SetRootLayer(window()->layer());
  compositor_->SetDisplayColorSpace(display.color_space());
}

void WindowTreeHost::OnAcceleratedWidgetAvailable() {
  compositor_->SetAcceleratedWidget(GetAcceleratedWidget());
  prop_.reset(new ui::ViewProp(GetAcceleratedWidget(),
                               kWindowTreeHostForAcceleratedWidget, this));
}

void WindowTreeHost::OnHostMovedInPixels(
    const gfx::Point& new_location_in_pixels) {
  TRACE_EVENT1("ui", "WindowTreeHost::OnHostMovedInPixels", "origin",
               new_location_in_pixels.ToString());

  for (WindowTreeHostObserver& observer : observers_)
    observer.OnHostMovedInPixels(this, new_location_in_pixels);
}

void WindowTreeHost::OnHostResizedInPixels(
    const gfx::Size& new_size_in_pixels) {
  gfx::Size adjusted_size(new_size_in_pixels);
  adjusted_size.Enlarge(output_surface_padding_in_pixels_.width(),
                        output_surface_padding_in_pixels_.height());

  // The compositor should have the same size as the native root window host.
  // Get the latest scale from display because it might have been changed.
  compositor_->SetScaleAndSize(ui::GetScaleFactorForNativeView(window()),
                               adjusted_size, window()->GetLocalSurfaceId());

  gfx::Size layer_size = GetBoundsInPixels().size();
  // The layer, and the observers should be notified of the
  // transformed size of the root window.
  UpdateRootWindowSizeInPixels(layer_size);
  for (WindowTreeHostObserver& observer : observers_)
    observer.OnHostResized(this);
}

void WindowTreeHost::OnHostWorkspaceChanged() {
  for (WindowTreeHostObserver& observer : observers_)
    observer.OnHostWorkspaceChanged(this);
}

void WindowTreeHost::OnHostDisplayChanged() {
  if (!compositor_)
    return;
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window());
  compositor_->SetDisplayColorSpace(display.color_space());
}

void WindowTreeHost::OnHostCloseRequested() {
  for (WindowTreeHostObserver& observer : observers_)
    observer.OnHostCloseRequested(this);
}

void WindowTreeHost::OnHostActivated() {
  Env::GetInstance()->NotifyHostActivated(this);
}

void WindowTreeHost::OnHostLostWindowCapture() {
  // It is possible for this function to be called during destruction, after the
  // root window has already been destroyed (e.g. when the ui::PlatformWindow is
  // destroyed, and during destruction, it loses capture. See more details in
  // http://crbug.com/770670)
  if (!window())
    return;
  Window* capture_window = client::GetCaptureWindow(window());
  if (capture_window && capture_window->GetRootWindow() == window())
    capture_window->ReleaseCapture();
}

ui::EventSink* WindowTreeHost::GetEventSink() {
  return dispatcher_.get();
}

void WindowTreeHost::OnDisplayAdded(const display::Display& new_display) {}

void WindowTreeHost::OnDisplayRemoved(const display::Display& old_display) {}

void WindowTreeHost::OnDisplayMetricsChanged(const display::Display& display,
                                             uint32_t metrics) {
  if (metrics & DisplayObserver::DISPLAY_METRIC_COLOR_SPACE) {
    display::Screen* screen = display::Screen::GetScreen();
    if (compositor_ &&
        display.id() == screen->GetDisplayNearestView(window()).id()) {
      compositor_->SetDisplayColorSpace(display.color_space());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHost, private:

void WindowTreeHost::MoveCursorToInternal(const gfx::Point& root_location,
                                          const gfx::Point& host_location) {
  last_cursor_request_position_in_host_ = host_location;
  MoveCursorToScreenLocationInPixels(host_location);
  client::CursorClient* cursor_client = client::GetCursorClient(window());
  if (cursor_client) {
    const display::Display& display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window());
    cursor_client->SetDisplay(display);
  }
  dispatcher()->OnCursorMovedToRootLocation(root_location);
}

void WindowTreeHost::OnCompositingDidCommit(ui::Compositor* compositor) {}

void WindowTreeHost::OnCompositingStarted(ui::Compositor* compositor,
                                          base::TimeTicks start_time) {
  if (!synchronizing_with_child_on_next_frame_)
    return;
  synchronizing_with_child_on_next_frame_ = false;
  dispatcher_->HoldPointerMoves();
  holding_pointer_moves_ = true;
}

void WindowTreeHost::OnCompositingEnded(ui::Compositor* compositor) {
  if (!holding_pointer_moves_)
    return;
  dispatcher_->ReleasePointerMoves();
  holding_pointer_moves_ = false;
}

void WindowTreeHost::OnCompositingLockStateChanged(ui::Compositor* compositor) {
}

void WindowTreeHost::OnCompositingChildResizing(ui::Compositor* compositor) {
  synchronizing_with_child_on_next_frame_ = true;
}

void WindowTreeHost::OnCompositingShuttingDown(ui::Compositor* compositor) {
  compositor->RemoveObserver(this);
}

}  // namespace aura
