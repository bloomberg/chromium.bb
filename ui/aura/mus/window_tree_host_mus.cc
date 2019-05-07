// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_tree_host_mus.h"

#include <limits>
#include <utility>

#include "base/bind.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/input_method_mus.h"
#include "ui/aura/mus/mus_types.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus_delegate.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/platform_window/stub/stub_window.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(aura::WindowTreeHostMus*)

namespace aura {

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(WindowTreeHostMus*, kWindowTreeHostMusKey, nullptr)

// Start at the max and decrease as in SingleProcessMash these values must not
// overlap with values assigned by Ozone's PlatformWindow (which starts at 1
// and increases).
uint32_t next_accelerated_widget_id = std::numeric_limits<uint32_t>::max();

// This class handles the gesture events occurring on the root window and sends
// them to the content window during the window move. Typically gesture events
// will stop arriving once PerformWindowMove is invoked, but sometimes events
// are already queued and arrive to the root window. They should be handled
// by the content window. See https://crbug.com/943316.
class RemainingGestureEventHandler : public ui::EventHandler, WindowObserver {
 public:
  RemainingGestureEventHandler(Window* content_window, Window* root)
      : content_window_({content_window}), root_(root) {
    root_->AddPostTargetHandler(this);
    root_->AddObserver(this);
  }
  ~RemainingGestureEventHandler() override {
    if (root_)
      StopObserving();
  }

 private:
  void StopObserving() {
    root_->RemoveObserver(this);
    root_->RemovePostTargetHandler(this);
    root_ = nullptr;
  }

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (!content_window_.windows().empty())
      (*content_window_.windows().begin())->delegate()->OnGestureEvent(event);
  }
  // WindowObserver:
  void OnWindowDestroying(Window* window) override { StopObserving(); }

  WindowTracker content_window_;
  Window* root_;

  DISALLOW_COPY_AND_ASSIGN(RemainingGestureEventHandler);
};

// ScopedTouchTransferController controls the transfer of touch events for
// window move loop. It transfers touches before the window move starts, and
// then transfers them back to the original window when the window move ends.
// However this transferring back to the original shouldn't happen if the client
// wants to continue the dragging on another window (like attaching the dragged
// tab to another window).
class ScopedTouchTransferController {
 public:
  ScopedTouchTransferController(Window* source, Window* dest)
      : tracker_({source, dest}),
        remaining_gesture_event_handler_(source, dest),
        gesture_recognizer_(source->env()->gesture_recognizer()) {
    gesture_recognizer_->TransferEventsTo(
        source, dest, ui::TransferTouchesBehavior::kDontCancel);
  }
  ~ScopedTouchTransferController() {
    if (tracker_.windows().size() == 2) {
      Window* source = tracker_.Pop();
      Window* dest = tracker_.Pop();
      gesture_recognizer_->TransferEventsTo(
          dest, source, ui::TransferTouchesBehavior::kDontCancel);
    }
  }

 private:
  WindowTracker tracker_;
  RemainingGestureEventHandler remaining_gesture_event_handler_;
  ui::GestureRecognizer* gesture_recognizer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTouchTransferController);
};

void OnPerformWindowMoveDone(
    std::unique_ptr<ScopedTouchTransferController> controller,
    base::OnceCallback<void(bool)> callback,
    bool success) {
  controller.reset();
  std::move(callback).Run(success);
}

void OnDispatchKeyEventComplete(
    base::OnceCallback<void(ws::mojom::EventResult)> cb,
    bool result) {
  std::move(cb).Run(result ? ws::mojom::EventResult::HANDLED
                           : ws::mojom::EventResult::UNHANDLED);
}

void OnDispatchKeyEventPostIMEComplete(
    base::OnceCallback<void(ws::mojom::EventResult)> cb,
    bool handled,
    bool stopped_propagation) {
  OnDispatchKeyEventComplete(std::move(cb), handled);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, public:

WindowTreeHostMus::WindowTreeHostMus(WindowTreeHostMusInitParams init_params)
    : WindowTreeHostPlatform(
          std::make_unique<Window>(nullptr,
                                   std::move(init_params.window_port))),
      display_id_(init_params.display_id),
      delegate_(init_params.window_tree_client),
      show_state_observer_(
          window(),
          base::BindRepeating(&WindowTreeHostMus::OnWindowShowStateDidChange,
                              base::Unretained(this))) {
  gfx::Rect bounds_in_pixels;
  window()->SetProperty(kWindowTreeHostMusKey, this);
  // TODO(sky): find a cleaner way to set this! Revisit this now that
  // constructor takes a Window.
  WindowPortMus* window_mus = WindowPortMus::Get(window());
  window_mus->window_ = window();
  // Apply the properties before initializing the window, that way the server
  // seems them at the time the window is created.
  for (auto& pair : init_params.properties)
    window_mus->SetPropertyFromServer(pair.first, &pair.second);
  // If window-server is hosting viz, then use the FrameSinkId from the server.
  // In other cases, let a valid FrameSinkId be selected by
  // context_factory_private().
  const bool force_software_compositor = false;
  ui::ExternalBeginFrameClient* external_begin_frame_client = nullptr;
  const bool are_events_in_pixels = false;
  CreateCompositor(window_mus->GenerateFrameSinkIdFromServerId(),
                   force_software_compositor, external_begin_frame_client,
                   are_events_in_pixels);
  gfx::AcceleratedWidget accelerated_widget;
// We need accelerated widget numbers to be different for each window and
// fit in the smallest sizeof(AcceleratedWidget) uint32_t has this property.
#if defined(OS_WIN) || defined(OS_ANDROID)
  accelerated_widget =
      reinterpret_cast<gfx::AcceleratedWidget>(next_accelerated_widget_id--);
#else
  accelerated_widget =
      static_cast<gfx::AcceleratedWidget>(next_accelerated_widget_id--);
#endif
  OnAcceleratedWidgetAvailable(accelerated_widget);

  delegate_->OnWindowTreeHostCreated(this);

  // Do not advertise accelerated widget; already set manually.
  const bool use_default_accelerated_widget = false;
  SetPlatformWindow(std::make_unique<ui::StubWindow>(
      this, use_default_accelerated_widget, bounds_in_pixels));

  if (!features::IsMojoImfEnabled()) {
    // NOTE: This creates one InputMethodMus per display, despite the
    // call to SetSharedInputMethod() below.
    input_method_mus_ = std::make_unique<InputMethodMus>(this, this);
    input_method_mus_->Init(init_params.window_tree_client->connector());
    SetSharedInputMethod(input_method_mus_.get());
  }

  compositor()->SetBackgroundColor(SK_ColorTRANSPARENT);

  // Mus windows are assumed hidden.
  compositor()->SetVisible(false);
}

WindowTreeHostMus::~WindowTreeHostMus() {
  DestroyCompositor();
  DestroyDispatcher();
}

// static
WindowTreeHostMus* WindowTreeHostMus::ForWindow(aura::Window* window) {
  if (!window)
    return nullptr;

  aura::Window* root = window->GetRootWindow();
  if (!root) {
    // During initial setup this function is called for the root, before the
    // WindowTreeHost has been registered so that GetRootWindow() returns null.
    // Fallback to checking window, in case it really is the root.
    return window->GetProperty(kWindowTreeHostMusKey);
  }

  return root->GetProperty(kWindowTreeHostMusKey);
}

void WindowTreeHostMus::DispatchKeyEventFromServer(
    ui::KeyEvent* event,
    base::OnceCallback<void(ws::mojom::EventResult)> cb) {
  ui::InputMethod* input_method = GetInputMethod();
  if (input_method) {
    ui::AsyncKeyDispatcher* dispatcher = input_method->GetAsyncKeyDispatcher();
    if (dispatcher) {
      dispatcher->DispatchKeyEventAsync(
          event, base::BindOnce(&OnDispatchKeyEventComplete, std::move(cb)));
      return;
    }
  }
  DispatchKeyEventPostIME(
      event, base::BindOnce(&OnDispatchKeyEventPostIMEComplete, std::move(cb)));
}

void WindowTreeHostMus::SetBounds(
    const gfx::Rect& bounds_in_dip,
    const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
  viz::LocalSurfaceIdAllocation actual_local_surface_id_allocation =
      local_surface_id_allocation;
  // This uses GetScaleFactorForNativeView() as it's called at a time when the
  // scale factor may not have been applied to the Compositor yet (the
  // call to WindowTreeHostPlatform::SetBoundsInPixels() updates the
  // Compositor).
  // Do not use ConvertRectToPixel, enclosing rects cause problems. In
  // particular, ConvertRectToPixel's result varies based on the location.
  // This *must* match the conversion used by ClientRoot, otherwise the two will
  // be out of sync. See // https://crbug.com/952095 for more details.
  const float dsf = ui::GetScaleFactorForNativeView(window());
  const gfx::Rect pixel_bounds(
      gfx::ScaleToFlooredPoint(bounds_in_dip.origin(), dsf),
      gfx::ScaleToCeiledSize(bounds_in_dip.size(), dsf));
  if (!is_server_setting_bounds_) {
    // Update the LocalSurfaceIdAllocation here, rather than in WindowTreeHost
    // as WindowTreeClient (the delegate) needs that information before
    // OnWindowTreeHostBoundsWillChange().
    if (!local_surface_id_allocation.IsValid() &&
        ShouldAllocateLocalSurfaceIdOnResize()) {
      if (pixel_bounds.size() != compositor()->size())
        window()->AllocateLocalSurfaceId();
      actual_local_surface_id_allocation =
          window()->GetLocalSurfaceIdAllocation();
    }
    delegate_->OnWindowTreeHostBoundsWillChange(this, bounds_in_dip);
  }
  bounds_in_dip_ = bounds_in_dip;
  WindowTreeHostPlatform::SetBoundsInPixels(pixel_bounds,
                                            actual_local_surface_id_allocation);
}

void WindowTreeHostMus::SetBoundsFromServer(
    const gfx::Rect& bounds,
    ui::WindowShowState state,
    const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
  base::AutoReset<bool> resetter(&is_server_setting_bounds_, true);
  // When there's a non-default |state|, we want to set that property and then
  // the bounds, so that by the time client code observes a bounds change the
  // show state is already updated, and by the time client code observes a state
  // change the bounds are already updated as well. To do this, we set the state
  // here, and as the first WindowObserver on |window()|, update the bounds.
  if (state != ui::SHOW_STATE_DEFAULT &&
      window()->GetProperty(aura::client::kShowStateKey) != state) {
    server_bounds_ = &bounds;
    server_lsia_ = &local_surface_id_allocation;
    window()->SetProperty(aura::client::kShowStateKey, state);
    DCHECK(!server_bounds_);
    DCHECK(!server_lsia_);
    return;
  }
  SetBounds(bounds, local_surface_id_allocation);
}

void WindowTreeHostMus::SetClientArea(
    const gfx::Insets& insets,
    const std::vector<gfx::Rect>& additional_client_area) {
  delegate_->OnWindowTreeHostClientAreaWillChange(this, insets,
                                                  additional_client_area);
}

void WindowTreeHostMus::SetOpacity(float value) {
  delegate_->OnWindowTreeHostSetOpacity(this, value);
}

void WindowTreeHostMus::DeactivateWindow() {
  delegate_->OnWindowTreeHostDeactivateWindow(this);
}

void WindowTreeHostMus::StackAbove(Window* window) {
  delegate_->OnWindowTreeHostStackAbove(this, window);
}

void WindowTreeHostMus::StackAtTop() {
  delegate_->OnWindowTreeHostStackAtTop(this);
}

void WindowTreeHostMus::PerformWindowMove(
    Window* content_window,
    ws::mojom::MoveLoopSource mus_source,
    const gfx::Point& cursor_location,
    int hit_test,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(window()->Contains(content_window));
  std::unique_ptr<ScopedTouchTransferController> scoped_controller;
  if (content_window != window()) {
    scoped_controller = std::make_unique<ScopedTouchTransferController>(
        content_window, window());
  }
  content_window->ReleaseCapture();
  delegate_->OnWindowTreeHostPerformWindowMove(
      this, mus_source, cursor_location, hit_test,
      base::BindOnce(&OnPerformWindowMoveDone, std::move(scoped_controller),
                     std::move(callback)));
}

void WindowTreeHostMus::CancelWindowMove() {
  delegate_->OnWindowTreeHostCancelWindowMove(this);
}

display::Display WindowTreeHostMus::GetDisplay() const {
  display::Display display;
  display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id_, &display);
  return display;
}

void WindowTreeHostMus::SetPendingLocalSurfaceIdFromServer(
    const viz::LocalSurfaceIdAllocation& id) {
  DCHECK(id.IsValid());
  pending_local_surface_id_from_server_ = id;
}

base::Optional<viz::LocalSurfaceIdAllocation>
WindowTreeHostMus::TakePendingLocalSurfaceIdFromServer() {
  base::Optional<viz::LocalSurfaceIdAllocation> id;
  std::swap(id, pending_local_surface_id_from_server_);
  return id;
}

void WindowTreeHostMus::HideImpl() {
  WindowTreeHostPlatform::HideImpl();
  window()->Hide();
}

void WindowTreeHostMus::DispatchEvent(ui::Event* event) {
  DCHECK(!event->IsKeyEvent());
  WindowTreeHostPlatform::DispatchEvent(event);
}

void WindowTreeHostMus::OnClosed() {
}

void WindowTreeHostMus::OnActivationChanged(bool active) {
  if (active)
    GetInputMethod()->OnFocus();
  else
    GetInputMethod()->OnBlur();
  WindowTreeHostPlatform::OnActivationChanged(active);
}

void WindowTreeHostMus::OnCloseRequest() {
  OnHostCloseRequested();
}

int64_t WindowTreeHostMus::GetDisplayId() {
  return display_id_;
}

bool WindowTreeHostMus::ShouldAllocateLocalSurfaceIdOnResize() {
  return WindowPortMus::Get(window())->window_mus_type() ==
             WindowMusType::TOP_LEVEL &&
         (window()->GetLocalSurfaceIdAllocation().IsValid() ||
          pending_local_surface_id_from_server_);
}

gfx::Rect WindowTreeHostMus::GetTransformedRootWindowBoundsInPixels(
    const gfx::Size& size_in_pixels) const {
  // Special case asking for the current size to ensure the aura::Window is
  // given the same size as |bounds_in_dip_|. To do otherwise could result in
  // rounding errors and the aura::Window given a different size.
  if (size_in_pixels == GetBoundsInPixels().size() &&
      window()->layer()->transform().IsIdentity()) {
    return gfx::Rect(bounds_in_dip_.size());
  }
  return WindowTreeHostPlatform::GetTransformedRootWindowBoundsInPixels(
      size_in_pixels);
}

void WindowTreeHostMus::SetTextInputState(ui::mojom::TextInputStatePtr state) {
  WindowPortMus::Get(window())->SetTextInputState(std::move(state));
}

void WindowTreeHostMus::SetImeVisibility(bool visible,
                                         ui::mojom::TextInputStatePtr state) {
  WindowPortMus::Get(window())->SetImeVisibility(visible, std::move(state));
}

bool WindowTreeHostMus::ConnectToImeEngine(
    ime::mojom::ImeEngineRequest engine_request,
    ime::mojom::ImeEngineClientPtr client) {
  delegate_->ConnectToImeEngine(std::move(engine_request), std::move(client));
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, protected:

void WindowTreeHostMus::SetBoundsInPixels(
    const gfx::Rect& bounds,
    const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
  // As UI code operates in DIPs (as does the window-service APIs), this
  // function is very seldomly used, and so converts to DIPs.
  SetBounds(
      gfx::ConvertRectToDIP(ui::GetScaleFactorForNativeView(window()), bounds),
      local_surface_id_allocation);
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, private:

void WindowTreeHostMus::OnWindowShowStateDidChange() {
  if (!server_bounds_)
    return;

  DCHECK(is_server_setting_bounds_);
  DCHECK(server_lsia_);
  SetBounds(*server_bounds_, *server_lsia_);
  server_bounds_ = nullptr;
  server_lsia_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus::WindowShowStateChangeObserver, public:

WindowTreeHostMus::WindowShowStateChangeObserver::WindowShowStateChangeObserver(
    aura::Window* window,
    base::RepeatingClosure show_state_changed_callback)
    : show_state_changed_callback_(show_state_changed_callback) {
  window->AddObserver(this);
}

WindowTreeHostMus::WindowShowStateChangeObserver::
    ~WindowShowStateChangeObserver() = default;

void WindowTreeHostMus::WindowShowStateChangeObserver::OnWindowPropertyChanged(
    aura::Window* window,
    const void* key,
    intptr_t old) {
  if (key == client::kShowStateKey)
    show_state_changed_callback_.Run();
}

}  // namespace aura
