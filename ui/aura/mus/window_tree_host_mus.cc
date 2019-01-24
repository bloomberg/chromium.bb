// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_tree_host_mus.h"

#include <limits>

#include "ui/aura/env.h"
#include "ui/aura/mus/input_method_mus.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus_delegate.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_recognizer_observer.h"
#include "ui/platform_window/stub/stub_window.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(aura::WindowTreeHostMus*);

namespace aura {

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(
    WindowTreeHostMus*, kWindowTreeHostMusKey, nullptr);

// Start at the max and decrease as in SingleProcessMash these values must not
// overlap with values assigned by Ozone's PlatformWindow (which starts at 1
// and increases).
uint32_t next_accelerated_widget_id = std::numeric_limits<uint32_t>::max();

// ScopedTouchTransferController controls the transfer of touch events for
// window move loop. It transfers touches before the window move starts, and
// then transfers them back to the original window when the window move ends.
// However this transferring back to the original shouldn't happen if the client
// wants to continue the dragging on another window (like attaching the dragged
// tab to another window).
class ScopedTouchTransferController : public ui::GestureRecognizerObserver {
 public:
  ScopedTouchTransferController(Window* source, Window* dest)
      : tracker_({source, dest}),
        gesture_recognizer_(source->env()->gesture_recognizer()) {
    gesture_recognizer_->TransferEventsTo(
        source, dest, ui::TransferTouchesBehavior::kDontCancel);
    gesture_recognizer_->AddObserver(this);
  }
  ~ScopedTouchTransferController() override {
    gesture_recognizer_->RemoveObserver(this);
    if (tracker_.windows().size() == 2) {
      Window* source = tracker_.Pop();
      Window* dest = tracker_.Pop();
      gesture_recognizer_->TransferEventsTo(
          dest, source, ui::TransferTouchesBehavior::kDontCancel);
    }
  }

 private:
  // ui::GestureRecognizerObserver:
  void OnActiveTouchesCanceledExcept(
      ui::GestureConsumer* not_cancelled) override {}
  void OnEventsTransferred(
      ui::GestureConsumer* current_consumer,
      ui::GestureConsumer* new_consumer,
      ui::TransferTouchesBehavior transfer_touches_behavior) override {
    if (tracker_.windows().size() <= 1)
      return;
    Window* dest = tracker_.windows()[1];
    if (current_consumer == dest)
      tracker_.Remove(dest);
  }
  void OnActiveTouchesCanceled(ui::GestureConsumer* consumer) override {}

  WindowTracker tracker_;

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

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, public:

WindowTreeHostMus::WindowTreeHostMus(WindowTreeHostMusInitParams init_params)
    : WindowTreeHostPlatform(
          std::make_unique<Window>(nullptr,
                                   std::move(init_params.window_port))),
      display_id_(init_params.display_id),
      delegate_(init_params.window_tree_client) {
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

  if (!init_params.use_classic_ime) {
    // NOTE: This creates one InputMethodMus per display, despite the
    // call to SetSharedInputMethod() below.
    input_method_ = std::make_unique<InputMethodMus>(this, this);
    input_method_->Init(init_params.window_tree_client->connector());
    SetSharedInputMethod(input_method_.get());
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

void WindowTreeHostMus::SetBoundsFromServerInPixels(
    const gfx::Rect& bounds_in_pixels,
    const viz::LocalSurfaceId& local_surface_id) {
  base::AutoReset<bool> resetter(&in_set_bounds_from_server_, true);
  // TODO(jonross): Update Mus to pass the correct allocation time.
  SetBoundsInPixels(
      bounds_in_pixels,
      viz::LocalSurfaceIdAllocation(local_surface_id, base::TimeTicks::Now()));
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
    base::OnceCallback<void(bool)> callback) {
  DCHECK(window()->Contains(content_window));
  std::unique_ptr<ScopedTouchTransferController> scoped_controller;
  if (content_window != window()) {
    scoped_controller = std::make_unique<ScopedTouchTransferController>(
        content_window, window());
  }
  content_window->ReleaseCapture();
  delegate_->OnWindowTreeHostPerformWindowMove(
      this, mus_source, cursor_location,
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

void WindowTreeHostMus::HideImpl() {
  WindowTreeHostPlatform::HideImpl();
  window()->Hide();
}

void WindowTreeHostMus::SetBoundsInPixels(
    const gfx::Rect& bounds,
    const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
  if (!in_set_bounds_from_server_)
    delegate_->OnWindowTreeHostBoundsWillChange(this, bounds);
  WindowTreeHostPlatform::SetBoundsInPixels(bounds,
                                            local_surface_id_allocation);
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

void WindowTreeHostMus::SetTextInputState(ui::mojom::TextInputStatePtr state) {
  WindowPortMus::Get(window())->SetTextInputState(std::move(state));
}

void WindowTreeHostMus::SetImeVisibility(bool visible,
                                         ui::mojom::TextInputStatePtr state) {
  WindowPortMus::Get(window())->SetImeVisibility(visible, std::move(state));
}

}  // namespace aura
