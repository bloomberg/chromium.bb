// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/desktop_window_tree_host_mus.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/focus_synchronizer.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_recognizer_observer.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/mus/cursor_manager_owner.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/mus/mus_property_mirror.h"
#include "ui/views/mus/screen_position_client_mus.h"
#include "ui/views/mus/window_manager_frame_values.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_screen_position_client.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace views {

namespace {

// As the window manager renderers the non-client decorations this class does
// very little but honor kTopViewInset.
class ClientSideNonClientFrameView : public NonClientFrameView,
                                     public aura::WindowObserver {
 public:
  explicit ClientSideNonClientFrameView(Widget* widget) : widget_(widget) {
    // Not part of the accessibility node hierarchy because the window frame is
    // provided by the window manager.
    if (MusClient::Get()->use_remote_accessibility_host())
      GetViewAccessibility().OverrideIsIgnored(true);

    observed_.Add(window());
  }
  ~ClientSideNonClientFrameView() override {}

 private:
  gfx::Insets GetClientInsets() const {
    const int top_inset = window()->GetProperty(aura::client::kTopViewInset);
    return gfx::Insets(top_inset, 0, 0, 0);
  }

  // View:
  const char* GetClassName() const override {
    return "ClientSideNonClientFrameView";
  }

  // NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override {
    gfx::Rect result(GetLocalBounds());
    if (widget_->IsFullscreen())
      return result;
    result.Inset(GetClientInsets());
    return result;
  }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    if (widget_->IsFullscreen())
      return client_bounds;

    gfx::Rect outset_bounds = client_bounds;
    outset_bounds.Inset(-GetClientInsets());
    return outset_bounds;
  }
  int NonClientHitTest(const gfx::Point& point) override { return HTNOWHERE; }
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override {
    // The window manager provides the shape; do nothing.
  }
  void ResetWindowControls() override {
    // TODO(sky): push to wm?
  }

  // These have no implementation. The Window Manager handles the actual
  // rendering of the icon/title. See NonClientFrameViewMash. The values
  // associated with these methods are pushed to the server by the way of
  // NativeWidgetMus functions.
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

  gfx::Size CalculatePreferredSize() const override {
    return widget_->non_client_view()
        ->GetWindowBoundsForClientBounds(
            gfx::Rect(widget_->client_view()->GetPreferredSize()))
        .size();
  }
  gfx::Size GetMinimumSize() const override {
    return widget_->non_client_view()
        ->GetWindowBoundsForClientBounds(
            gfx::Rect(widget_->client_view()->GetMinimumSize()))
        .size();
  }
  gfx::Size GetMaximumSize() const override {
    gfx::Size max_size = widget_->client_view()->GetMaximumSize();
    gfx::Size converted_size =
        widget_->non_client_view()
            ->GetWindowBoundsForClientBounds(gfx::Rect(max_size))
            .size();
    return gfx::Size(max_size.width() == 0 ? 0 : converted_size.width(),
                     max_size.height() == 0 ? 0 : converted_size.height());
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    observed_.Remove(window);
  }

  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key == aura::client::kTopViewInset) {
      InvalidateLayout();
      widget_->GetRootView()->Layout();
    }
  }

  aura::Window* window() const {
    return widget_->GetNativeWindow()->GetRootWindow();
  }

  Widget* widget_;
  ScopedObserver<aura::Window, aura::WindowObserver> observed_{this};

  DISALLOW_COPY_AND_ASSIGN(ClientSideNonClientFrameView);
};

void OnMoveLoopEnd(bool* out_success,
                   base::Closure quit_closure,
                   bool in_success) {
  *out_success = in_success;
  quit_closure.Run();
}

}  // namespace

// WindowObserver installed on DesktopWindowTreeHostMus::window(). Mostly
// forwards interesting events to DesktopWindowTreeHostMus. To avoid having
// DesktopWindowTreeHostMus be a WindowObserver on two windows (which is mildly
// error prone), this helper class is used.
class DesktopWindowTreeHostMus::WindowTreeHostWindowObserver
    : public aura::WindowObserver {
 public:
  explicit WindowTreeHostWindowObserver(DesktopWindowTreeHostMus* host)
      : host_(host) {
    host->window()->AddObserver(this);
  }
  ~WindowTreeHostWindowObserver() override {
    host_->window()->RemoveObserver(this);
  }

  void set_is_waiting_for_restore(bool value) {
    is_waiting_for_restore_ = value;
  }
  bool is_waiting_for_restore() const { return is_waiting_for_restore_; }

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (window == host_->window())
      host_->OnWindowTreeHostWindowVisibilityChanged(visible);
  }
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (key == aura::client::kShowStateKey)
      is_waiting_for_restore_ = false;
  }
  void OnResizeLoopStarted(aura::Window* window) override {
    host_->native_widget_delegate_->OnNativeWidgetBeginUserBoundsChange();
  }
  void OnResizeLoopEnded(aura::Window* window) override {
    host_->native_widget_delegate_->OnNativeWidgetEndUserBoundsChange();
  }

 private:
  DesktopWindowTreeHostMus* host_;

  // True while waiting for the show state to change.
  bool is_waiting_for_restore_ = false;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostWindowObserver);
};

DesktopWindowTreeHostMus::DesktopWindowTreeHostMus(
    aura::WindowTreeHostMusInitParams init_params,
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : aura::WindowTreeHostMus(std::move(init_params)),
      native_widget_delegate_(native_widget_delegate),
      desktop_native_widget_aura_(desktop_native_widget_aura),
      close_widget_factory_(this) {
  MusClient::Get()->AddObserver(this);
  MusClient::Get()->window_tree_client()->focus_synchronizer()->AddObserver(
      this);
  content_window()->AddObserver(this);
  // DesktopNativeWidgetAura registers the association between |content_window_|
  // and Widget, but code may also want to go from the root (window()) to the
  // Widget. This call enables that.
  NativeWidgetAura::RegisterNativeWidgetForWindow(desktop_native_widget_aura,
                                                  window());

  window_tree_host_window_observer_ =
      std::make_unique<WindowTreeHostWindowObserver>(this);
  // TODO: use display id and bounds if available, likely need to pass in
  // InitParams for that.
}

DesktopWindowTreeHostMus::~DesktopWindowTreeHostMus() {
  window_tree_host_window_observer_.reset();

  content_window()->RemoveObserver(this);
  MusClient::Get()->RemoveObserver(this);
  MusClient::Get()->window_tree_client()->focus_synchronizer()->RemoveObserver(
      this);
  desktop_native_widget_aura_->OnDesktopWindowTreeHostDestroyed(this);
}

void DesktopWindowTreeHostMus::SendClientAreaToServer() {
  if (!ShouldSendClientAreaToServer())
    return;

  NonClientView* non_client_view =
      native_widget_delegate_->AsWidget()->non_client_view();
  if (!non_client_view || !non_client_view->client_view())
    return;

  View* client_view = non_client_view->client_view();
  if (!observed_client_view_.IsObserving(client_view))
    observed_client_view_.Add(client_view);

  const gfx::Rect client_area_rect(non_client_view->client_view()->bounds());
  SetClientArea(
      gfx::Insets(
          client_area_rect.y(), client_area_rect.x(),
          non_client_view->bounds().height() - client_area_rect.bottom(),
          non_client_view->bounds().width() - client_area_rect.right()),
      std::vector<gfx::Rect>());
}

bool DesktopWindowTreeHostMus::IsFocusClientInstalledOnFocusSynchronizer()
    const {
  return MusClient::Get()
             ->window_tree_client()
             ->focus_synchronizer()
             ->active_focus_client() == aura::client::GetFocusClient(window());
}

float DesktopWindowTreeHostMus::GetScaleFactor() const {
  // TODO(sky): GetDisplayNearestWindow() should take a const aura::Window*.
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(const_cast<aura::Window*>(window()))
      .device_scale_factor();
}

bool DesktopWindowTreeHostMus::IsWaitingForRestoreToComplete() const {
  return window_tree_host_window_observer_->is_waiting_for_restore();
}

bool DesktopWindowTreeHostMus::ShouldSendClientAreaToServer() const {
  if (!auto_update_client_area_)
    return false;

  return desktop_native_widget_aura_->widget_type() ==
         Widget::InitParams::TYPE_WINDOW;
}

void DesktopWindowTreeHostMus::RestoreToPreminimizedState() {
  DCHECK(IsMinimized());
  window_tree_host_window_observer_->set_is_waiting_for_restore(true);
  base::AutoReset<bool> setter(&is_updating_window_visibility_, true);
  window()->Show();
}

void DesktopWindowTreeHostMus::OnWindowTreeHostWindowVisibilityChanged(
    bool visible) {
  if (is_updating_window_visibility_)
    return;

  // Call Show()/Hide() so that the visibility state is mirrored correctly. This
  // function makes it so that calling Show()/Hide() on window() is the same as
  // calling Show()/Hide() on the Widget.
  base::AutoReset<bool> setter(&is_updating_window_visibility_, true);
  if (visible)
    Show(ui::SHOW_STATE_INACTIVE, gfx::Rect());
  else
    Hide();
}

void DesktopWindowTreeHostMus::UpdateMinAndMaxSize() {
  gfx::Size min_size = content_window()->delegate()->GetMinimumSize();
  gfx::Size max_size = content_window()->delegate()->GetMaximumSize();
  if (min_size_ == min_size && max_size_ == max_size)
    return;
  min_size_ = min_size;
  max_size_ = max_size;
  // Setting the property to |window()| to propagate those properties to the
  // window server.
  if (min_size_ == gfx::Size())
    window()->ClearProperty(aura::client::kMinimumSize);
  else
    window()->SetProperty(aura::client::kMinimumSize, new gfx::Size(min_size_));
  if (max_size_ == gfx::Size())
    window()->ClearProperty(aura::client::kMaximumSize);
  else
    window()->SetProperty(aura::client::kMaximumSize, new gfx::Size(max_size_));
}

void DesktopWindowTreeHostMus::Init(const Widget::InitParams& params) {
  const bool translucent =
      MusClient::ShouldMakeWidgetWindowsTranslucent(params);
  content_window()->SetTransparent(translucent);
  window()->SetTransparent(translucent);

  // The window manager may provide the initial show state, for example for
  // Chrome OS lock screen windows. https://crbug.com/899055
  if (params.show_state != ui::SHOW_STATE_DEFAULT)
    window()->SetProperty(aura::client::kShowStateKey, params.show_state);

  if (!params.bounds.IsEmpty()) {
    // Init the scale now (before InitHost below), it is used by SetBoundsInDIP.
    IntializeDeviceScaleFactor(GetDisplay().device_scale_factor());
    SetBoundsInDIP(params.bounds);
  }

  cursor_manager_owner_ = std::make_unique<CursorManagerOwner>(window());
  InitHost();

  NativeWidgetAura::SetShadowElevationFromInitParams(window(), params);

  // Widget's |InitParams::parent| has different meanings depending on the
  // NativeWidgetPrivate implementation that the Widget creates (each Widget
  // creates a NativeWidgetPrivate). When DesktopNativeWidgetAura is used as
  // the NativeWidgetPrivate implementation, |InitParams::parent| means the
  // entirety of the contents of the new Widget should be stacked above the
  // entirety of the contents of the Widget for |InitParams::parent|, and
  // the new Widget should be deleted when the Widget for
  // |InitParams::parent| is deleted. Aura and mus provide support for
  // transient windows, which provides both the stacking and ownership needed to
  // support |InitParams::parent|.
  //
  // DesktopNativeWidgetAura internally creates two aura::Windows (one by
  // WindowTreeHost, the other in |DesktopNativeWidgetAura::content_window_|).
  // To have the entirety of the contents of the Widget appear on top of the
  // entirety of the contents of another Widget, the stacking is done on the
  // WindowTreeHost's window. For these reasons, the following code uses the
  // Window associated with the WindowTreeHost of the |params.parent|.
  //
  // Views/Aura provide support for child-modal windows. Child-modal windows
  // are windows that are modal to their transient parent. Because this code
  // implements |InitParams::parent| in terms of transient parents, it means
  // it is not possible to support both |InitParams::parent| as well as a
  // child-modal window. This is *only* an issue if a Widget that uses a
  // DesktopNativeWidgetAura needs to be child-modal to another window. At
  // the current time NativeWidgetAura is always used for child-modal windows,
  // so this isn't an issue.
  //
  // If we end up needing to use DesktopNativeWidgetAura for child-modal
  // Widgets then we need something different. Possibilities include:
  // . Have mus ignore child-modal windows and instead implement child-modal
  //   entirely in the client (this is what we do on Windows). To get this
  //   right likely means we need the ability to disable windows (see
  //   HWNDMessageHandler::InitModalType() for how Windows OS does this).
  // . Implement |InitParams::parent| using a different (new) API.
  if (params.parent && params.parent->GetHost()) {
    aura::client::GetTransientWindowClient()->AddTransientChild(
        params.parent->GetHost()->window(), window());
  }

  if (!params.accept_events)
    window()->SetEventTargetingPolicy(ws::mojom::EventTargetingPolicy::NONE);

  // Sets the has-content info for the occlusion tracker that runs on the Window
  // Service side.
  content_window()->SetProperty(
      aura::client::kClientWindowHasContent,
      params.layer_type != ui::LAYER_NOT_DRAWN &&
          params.opacity == Widget::InitParams::OPAQUE_WINDOW);
}

void DesktopWindowTreeHostMus::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  if (params.parent && params.parent->GetHost()) {
    parent_ = static_cast<DesktopWindowTreeHostMus*>(params.parent->GetHost());
    parent_->children_.insert(this);
  }
  native_widget_delegate_->OnNativeWidgetCreated();
}

void DesktopWindowTreeHostMus::OnActiveWindowChanged(bool active) {
  // This function is called when there is a change in the active window the
  // FocusClient for window() is associated with. This needs to potentially
  // propagate to mus (the change may originate locally, not from mus).
  // Propagating to the server is done by resetting the ActiveFocusClient.
  if (active && !IsFocusClientInstalledOnFocusSynchronizer()) {
    MusClient::Get()
        ->window_tree_client()
        ->focus_synchronizer()
        ->SetActiveFocusClient(aura::client::GetFocusClient(window()),
                               window());
  } else if (!active && IsFocusClientInstalledOnFocusSynchronizer()) {
    MusClient::Get()
        ->window_tree_client()
        ->focus_synchronizer()
        ->SetActiveFocusClient(nullptr, nullptr);
  }
}

void DesktopWindowTreeHostMus::OnWidgetInitDone() {
  // Because of construction order it's possible the bounds have changed before
  // the NonClientView was created, which means we may not have sent the
  // client-area and hit-test-mask.
  SendClientAreaToServer();

  MusClient::Get()->OnCaptureClientSet(
      aura::client::GetCaptureClient(window()));

  // These views are not part of the accessibility node hierarchy because the
  // window frame is provided by the window manager.
  Widget* widget = native_widget_delegate_->AsWidget();
  if (MusClient::Get()->use_remote_accessibility_host()) {
    if (widget->non_client_view())
      widget->non_client_view()->GetViewAccessibility().OverrideIsIgnored(true);
    if (widget->client_view())
      widget->client_view()->GetViewAccessibility().OverrideIsIgnored(true);
  }

  MusClient::Get()->OnWidgetInitDone(widget);
}

std::unique_ptr<corewm::Tooltip> DesktopWindowTreeHostMus::CreateTooltip() {
  return std::make_unique<corewm::TooltipAura>();
}

std::unique_ptr<aura::client::DragDropClient>
DesktopWindowTreeHostMus::CreateDragDropClient(
    DesktopNativeCursorManager* cursor_manager) {
  // aura-mus handles installing a DragDropClient.
  return nullptr;
}

std::unique_ptr<aura::client::ScreenPositionClient>
DesktopWindowTreeHostMus::CreateScreenPositionClient() {
  return std::make_unique<ScreenPositionClientMus>(this);
}

void DesktopWindowTreeHostMus::Close() {
  if (close_widget_factory_.HasWeakPtrs())
    return;

  // Even though we don't close immediately, we need to hide immediately
  // (otherwise events may be processed, which is unexpected).
  Hide();

  // This has to happen *after* Hide() above, otherwise animations won't work.
  content_window()->Hide();

  // Close doesn't delete this immediately, as 'this' may still be on the stack
  // resulting in possible crashes when the stack unwindes.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DesktopWindowTreeHostMus::CloseNow,
                                close_widget_factory_.GetWeakPtr()));
}

void DesktopWindowTreeHostMus::CloseNow() {
  MusClient::Get()->OnCaptureClientUnset(
      aura::client::GetCaptureClient(window()));

  native_widget_delegate_->OnNativeWidgetDestroying();

  // If we have children, close them. Use a copy for iteration because they'll
  // remove themselves from |children_|.
  std::set<DesktopWindowTreeHostMus*> children_copy = children_;
  for (DesktopWindowTreeHostMus* child : children_copy)
    child->CloseNow();
  DCHECK(children_.empty());

  if (parent_) {
    parent_->children_.erase(this);
    parent_ = nullptr;
  }

  DestroyCompositor();
  desktop_native_widget_aura_->OnHostClosed();
}

aura::WindowTreeHost* DesktopWindowTreeHostMus::AsWindowTreeHost() {
  return this;
}

void DesktopWindowTreeHostMus::Show(ui::WindowShowState show_state,
                                    const gfx::Rect& restore_bounds) {
  // Only notify if the visibility is really changing.
  const bool notify_visibility_change =
      is_updating_window_visibility_ || !IsVisible();
  if (notify_visibility_change)
    native_widget_delegate_->OnNativeWidgetVisibilityChanging(true);

  // NOTE: this code is called from Widget::Show() (no args). Widget::Show()
  // supplies ui::SHOW_STATE_DEFAULT as the |show_state| after the first call.
  // If SHOW_STATE_DEFAULT is supplied, and the Window is currently minimized,
  // the window should be restored to its preminimized state.

  if (show_state == ui::SHOW_STATE_MAXIMIZED && !restore_bounds.IsEmpty()) {
    window()->SetProperty(aura::client::kRestoreBoundsKey,
                          new gfx::Rect(restore_bounds));
  }
  if (show_state == ui::SHOW_STATE_MAXIMIZED ||
      show_state == ui::SHOW_STATE_FULLSCREEN) {
    window()->SetProperty(aura::client::kShowStateKey, show_state);
    window_tree_host_window_observer_->set_is_waiting_for_restore(false);
  } else if (show_state == ui::SHOW_STATE_DEFAULT && IsMinimized()) {
    RestoreToPreminimizedState();
  } else if (show_state == ui::SHOW_STATE_MINIMIZED && !IsMinimized()) {
    Minimize();
  }
  // DesktopWindowTreeHostMus is unique in that it calls window()->Show() here.
  // All other implementations call window()->Show() from the constructor. This
  // is necessary as window()'s visibility is mirrored in the server, on other
  // platforms it's the visibility of the AcceleratedWidget that matters and
  // dictates what is actually drawn on screen.
  {
    base::AutoReset<bool> setter(&is_updating_window_visibility_, true);
    window()->Show();
  }
  if (compositor())
    compositor()->SetVisible(true);

  // |content_window_| is the Window that will be focused by way of Activate().
  // Ensure |content_window_| is visible before the call to Activate(),
  // otherwise focus goes to window().
  content_window()->Show();

  if (show_state != ui::SHOW_STATE_MINIMIZED)
    UpdateMinAndMaxSize();

  if (notify_visibility_change)
    native_widget_delegate_->OnNativeWidgetVisibilityChanged(true);

  if (native_widget_delegate_->CanActivate()) {
    if (show_state != ui::SHOW_STATE_INACTIVE &&
        show_state != ui::SHOW_STATE_MINIMIZED) {
      Activate();
    }

    // SetInitialFocus() should be always be called, even for
    // SHOW_STATE_INACTIVE. If the window has to stay inactive, the method will
    // do the right thing.
    // Activate() might fail if the window is non-activatable. In this case, we
    // should pass SHOW_STATE_INACTIVE to SetInitialFocus() to stop the initial
    // focused view from getting focused. See crbug.com/515594 for example.
    native_widget_delegate_->SetInitialFocus(
        IsActive() ? show_state : ui::SHOW_STATE_INACTIVE);
  }
}

bool DesktopWindowTreeHostMus::IsVisible() const {
  // Go through the DesktopNativeWidgetAura::IsVisible() for checking
  // visibility of the parent as it has additional checks beyond checking the
  // aura::Window.
  return window()->IsVisible() &&
         (!parent_ ||
          static_cast<const internal::NativeWidgetPrivate*>(
              parent_->desktop_native_widget_aura_)
              ->IsVisible());
}

void DesktopWindowTreeHostMus::SetSize(const gfx::Size& size) {
  // Use bounds_in_dip(), as the origin of window() is always at (0, 0).
  gfx::Rect screen_bounds = bounds_in_dip();
  screen_bounds.set_size(size);
  SetBoundsInDIP(screen_bounds);
}

void DesktopWindowTreeHostMus::StackAbove(aura::Window* relative) {
  // Windows and X11 check for |relative| being nullptr and fail silently. It
  // also looks like |relative| is usually multiple children deep in the root
  // window, which we must pass instead.
  if (relative && relative->GetRootWindow())
    WindowTreeHostMus::StackAbove(relative->GetRootWindow());
}

void DesktopWindowTreeHostMus::StackAtTop() {
  // Request to the server to stack our current mus window at the top. Our
  // window() is a root, and we can't reach up past it so we can't just request
  // a Reorder(), which is what we'd do to reorder our own subwindows.
  WindowTreeHostMus::StackAtTop();
}

void DesktopWindowTreeHostMus::CenterWindow(const gfx::Size& size) {
  gfx::Rect bounds_to_center_in = GetWorkAreaBoundsInScreen();

  // If there is a transient parent and it fits |size|, then center over it.
  if (wm::GetTransientParent(content_window())) {
    gfx::Rect transient_parent_bounds =
        wm::GetTransientParent(content_window())->GetBoundsInScreen();
    if (transient_parent_bounds.height() >= size.height() &&
        transient_parent_bounds.width() >= size.width()) {
      bounds_to_center_in = transient_parent_bounds;
    }
  }

  gfx::Rect resulting_bounds(bounds_to_center_in);
  resulting_bounds.ClampToCenteredSize(size);
  SetBoundsInDIP(resulting_bounds);
}

void DesktopWindowTreeHostMus::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  // Implementation matches that of NativeWidgetAura.
  *bounds = GetRestoredBounds();
  if (IsWaitingForRestoreToComplete()) {
    // The real state is not known, use ui::SHOW_STATE_NORMAL to avoid saving
    // the minimized state.
    *show_state = ui::SHOW_STATE_NORMAL;
  } else {
    *show_state = window()->GetProperty(aura::client::kShowStateKey);
  }
}

gfx::Rect DesktopWindowTreeHostMus::GetWindowBoundsInScreen() const {
  return bounds_in_dip();
}

gfx::Rect DesktopWindowTreeHostMus::GetClientAreaBoundsInScreen() const {
  // View-to-screen coordinate system transformations depend on this returning
  // the full window bounds, for example View::ConvertPointToScreen().
  return GetWindowBoundsInScreen();
}

gfx::Rect DesktopWindowTreeHostMus::GetRestoredBounds() const {
  // Restored bounds should only be relevant if the window is minimized,
  // maximized, or fullscreen. However, in some places the code expects
  // GetRestoredBounds() to return the current window bounds if the window is
  // not in either state.
  if (IsMinimized() || IsMaximized() || IsFullscreen()) {
    // Restore bounds are in screen coordinates, no need to convert.
    gfx::Rect* restore_bounds =
        window()->GetProperty(aura::client::kRestoreBoundsKey);
    if (restore_bounds)
      return *restore_bounds;
  }
  return GetWindowBoundsInScreen();
}

std::string DesktopWindowTreeHostMus::GetWorkspace() const {
  // Only used on x11.
  return std::string();
}

gfx::Rect DesktopWindowTreeHostMus::GetWorkAreaBoundsInScreen() const {
  return GetDisplay().work_area();
}

void DesktopWindowTreeHostMus::SetShape(
    std::unique_ptr<Widget::ShapeRects> native_shape) {
  MusClient::Get()->window_tree_client()->SetShape(
      aura::WindowMus::Get(window()), std::move(native_shape));
}

void DesktopWindowTreeHostMus::Activate() {
  if (!IsVisible() && !IsMinimized())
    return;

  // Activate() is expected to restore a minimized window.
  if (IsMinimized())
    RestoreToPreminimizedState();

  // This should result in OnActiveFocusClientChanged() being called, which
  // triggers a call to DesktopNativeWidgetAura::HandleActivationChanged(),
  // which focuses the right window.
  MusClient::Get()
      ->window_tree_client()
      ->focus_synchronizer()
      ->SetActiveFocusClient(aura::client::GetFocusClient(window()), window());
  if (is_active_)
    window()->SetProperty(aura::client::kDrawAttentionKey, false);
}

void DesktopWindowTreeHostMus::Deactivate() {
  if (!is_active_)
    return;

  // Reset the active focus client, which will trigger resetting active status.
  // This is done so that we deactivate immediately.
  DCHECK(IsFocusClientInstalledOnFocusSynchronizer());
  MusClient::Get()
      ->window_tree_client()
      ->focus_synchronizer()
      ->SetActiveFocusClient(nullptr, nullptr);
  DCHECK(!is_active_);

  // Then ask the window manager to deactivate, which effectively means pick
  // another window to activate.
  DeactivateWindow();
}

bool DesktopWindowTreeHostMus::IsActive() const {
  return is_active_;
}

void DesktopWindowTreeHostMus::Maximize() {
  window()->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
}

void DesktopWindowTreeHostMus::Minimize() {
  window()->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);

  // When minimized, this should no longer be active.
  if (IsFocusClientInstalledOnFocusSynchronizer()) {
    MusClient::Get()
        ->window_tree_client()
        ->focus_synchronizer()
        ->SetActiveFocusClient(nullptr, nullptr);
  }
}

void DesktopWindowTreeHostMus::Restore() {
  window()->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
}

bool DesktopWindowTreeHostMus::IsMaximized() const {
  return window()->GetProperty(aura::client::kShowStateKey) ==
         ui::SHOW_STATE_MAXIMIZED;
}

bool DesktopWindowTreeHostMus::IsMinimized() const {
  return window()->GetProperty(aura::client::kShowStateKey) ==
             ui::SHOW_STATE_MINIMIZED &&
         !IsWaitingForRestoreToComplete();
}

bool DesktopWindowTreeHostMus::HasCapture() const {
  // Capture state is held by DesktopNativeWidgetAura::content_window_.
  // DesktopNativeWidgetAura::HasCapture() calls content_window_->HasCapture(),
  // and this. That means this function can always return true.
  return true;
}

void DesktopWindowTreeHostMus::SetAlwaysOnTop(bool always_on_top) {
  window()->SetProperty(aura::client::kAlwaysOnTopKey, always_on_top);
}

bool DesktopWindowTreeHostMus::IsAlwaysOnTop() const {
  return window()->GetProperty(aura::client::kAlwaysOnTopKey);
}

void DesktopWindowTreeHostMus::SetVisibleOnAllWorkspaces(bool always_visible) {
  // Not applicable to chromeos.
}

bool DesktopWindowTreeHostMus::IsVisibleOnAllWorkspaces() const {
  return false;
}

bool DesktopWindowTreeHostMus::SetWindowTitle(const base::string16& title) {
  WidgetDelegate* widget_delegate =
      native_widget_delegate_->AsWidget()->widget_delegate();
  const bool show = widget_delegate && widget_delegate->ShouldShowWindowTitle();
  if (window()->GetTitle() == title &&
      window()->GetProperty(aura::client::kTitleShownKey) == show) {
    return false;
  }
  window()->SetProperty(aura::client::kTitleShownKey, show);
  window()->SetTitle(title);
  return true;
}

void DesktopWindowTreeHostMus::ClearNativeFocus() {
  aura::client::FocusClient* client = aura::client::GetFocusClient(window());
  if (client && window()->Contains(client->GetFocusedWindow()))
    client->ResetFocusWithinActiveWindow(window());
}

Widget::MoveLoopResult DesktopWindowTreeHostMus::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  ws::mojom::MoveLoopSource mus_source =
      source == Widget::MOVE_LOOP_SOURCE_MOUSE
          ? ws::mojom::MoveLoopSource::MOUSE
          : ws::mojom::MoveLoopSource::TOUCH;

  bool success = false;
  // Don't use display::Screen::GetCursorScreenPoint() -- that's incorrect for
  // touch events. Rather the cursor location can be computed from window's
  // location with drag_offset.
  gfx::Point cursor_location = window()->GetBoundsInScreen().origin() +
                               gfx::ToFlooredVector2d(drag_offset);
  WindowTreeHostMus::PerformWindowMove(
      content_window(), mus_source, cursor_location, HTCAPTION,
      base::BindOnce(&OnMoveLoopEnd, &success, run_loop.QuitClosure()));

  run_loop.Run();

  return success ? Widget::MOVE_LOOP_SUCCESSFUL : Widget::MOVE_LOOP_CANCELED;
}

void DesktopWindowTreeHostMus::EndMoveLoop() {
  WindowTreeHostMus::CancelWindowMove();
}

void DesktopWindowTreeHostMus::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  window()->SetProperty(aura::client::kAnimationsDisabledKey, !value);
}

NonClientFrameView* DesktopWindowTreeHostMus::CreateNonClientFrameView() {
  if (!ShouldSendClientAreaToServer())
    return nullptr;

  // Initialize kTopViewInset to a default value. Further updates will come
  // from Ash. This is necessary so that during app window creation,
  // GetWindowBoundsForClientBounds() can calculate correctly.
  SetTopViewInsetToDefault();
  return new ClientSideNonClientFrameView(native_widget_delegate_->AsWidget());
}

bool DesktopWindowTreeHostMus::ShouldUseNativeFrame() const {
  return false;
}

bool DesktopWindowTreeHostMus::ShouldWindowContentsBeTransparent() const {
  return false;
}

void DesktopWindowTreeHostMus::FrameTypeChanged() {
  native_widget_delegate_->AsWidget()->ThemeChanged();
}

void DesktopWindowTreeHostMus::SetFullscreen(bool fullscreen) {
  if (IsFullscreen() == fullscreen)
    return;  // Nothing to do.

  // Retrieve restore bounds before leaving fullscreen.
  gfx::Rect restore_bounds;
  if (!fullscreen)
    restore_bounds = GetRestoredBounds();

  // Change the fullscreen state.
  wm::SetWindowFullscreen(window(), fullscreen);

  // Preset bounds with heuristic size to provide synchronous bounds change
  // after the switch to/from fullscreen.
  if (fullscreen) {
    window()->SetProperty(aura::client::kRestoreBoundsKey,
                          new gfx::Rect(GetWindowBoundsInScreen()));
    SetBoundsInDIP(GetDisplay().bounds());
  } else {
    SetTopViewInsetToDefault();
    SetBoundsInDIP(restore_bounds);
  }
}

bool DesktopWindowTreeHostMus::IsFullscreen() const {
  return window()->GetProperty(aura::client::kShowStateKey) ==
         ui::SHOW_STATE_FULLSCREEN;
}

void DesktopWindowTreeHostMus::SetOpacity(float opacity) {
  WindowTreeHostMus::SetOpacity(opacity);
}

void DesktopWindowTreeHostMus::SetAspectRatio(const gfx::SizeF& aspect_ratio) {
  window()->SetProperty(aura::client::kAspectRatio,
                        new gfx::SizeF(aspect_ratio));
}

void DesktopWindowTreeHostMus::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                              const gfx::ImageSkia& app_icon) {
  // In Ash, the app icon is always used in preference to the window icon, so
  // ignore the window icon. The max size that ash needs is 24dip, which is
  // kIconSize in caption_container_view.cc.
  constexpr gfx::Size kMaxUsefulAppIconSize(24, 24);
  DCHECK_EQ(app_icon.width(), app_icon.height());
  gfx::ImageSkia app_icon_resized =
      app_icon.width() > kMaxUsefulAppIconSize.width()
          ? gfx::ImageSkiaOperations::CreateResizedImage(
                app_icon, skia::ImageOperations::RESIZE_BEST,
                kMaxUsefulAppIconSize)
          : app_icon;

  window()->GetRootWindow()->SetProperty(aura::client::kAppIconSmallKey,
                                         new gfx::ImageSkia(app_icon_resized));
}

void DesktopWindowTreeHostMus::InitModalType(ui::ModalType modal_type) {
  // See comment in Init() related to |InitParams::parent| as to why this DCHECK
  // is here.
  DCHECK_NE(modal_type, ui::MODAL_TYPE_CHILD);
  window()->SetProperty(aura::client::kModalKey, modal_type);
}

void DesktopWindowTreeHostMus::FlashFrame(bool flash_frame) {
  window()->SetProperty(aura::client::kDrawAttentionKey, flash_frame);
}

bool DesktopWindowTreeHostMus::IsAnimatingClosed() const {
  return false;
}

bool DesktopWindowTreeHostMus::IsTranslucentWindowOpacitySupported() const {
  return true;
}

void DesktopWindowTreeHostMus::SizeConstraintsChanged() {
  int32_t behavior = ws::mojom::kResizeBehaviorNone;
  Widget* widget = native_widget_delegate_->AsWidget();
  if (widget->widget_delegate())
    behavior = widget->widget_delegate()->GetResizeBehavior();
  window()->SetProperty(aura::client::kResizeBehaviorKey, behavior);
  UpdateMinAndMaxSize();
}

bool DesktopWindowTreeHostMus::ShouldUpdateWindowTransparency() const {
  // Needed so the window manager can render the client decorations.
  return false;
}

bool DesktopWindowTreeHostMus::ShouldUseDesktopNativeCursorManager() const {
  // We manage the cursor ourself.
  return false;
}

bool DesktopWindowTreeHostMus::ShouldCreateVisibilityController() const {
  // Window manager takes care of all top-level window animations.
  return false;
}

void DesktopWindowTreeHostMus::SetBoundsInDIP(const gfx::Rect& bounds_in_dip) {
  // Do not use ConvertRectToPixel, enclosing rects cause problems.
  SetBounds(bounds_in_dip, viz::LocalSurfaceIdAllocation());
}

void DesktopWindowTreeHostMus::OnCanActivateChanged() {
  MusClient::Get()->window_tree_client()->SetCanFocus(
      window(), native_widget_delegate_->CanActivate());
}

void DesktopWindowTreeHostMus::OnWindowManagerFrameValuesChanged() {
  NonClientView* non_client_view =
      native_widget_delegate_->AsWidget()->non_client_view();
  if (non_client_view) {
    non_client_view->Layout();
    non_client_view->SchedulePaint();
  }

  SendClientAreaToServer();
}

void DesktopWindowTreeHostMus::OnActiveFocusClientChanged(
    aura::client::FocusClient* focus_client,
    aura::Window* focus_client_root) {
  if (focus_client_root == this->window()) {
    is_active_ = true;
    desktop_native_widget_aura_->HandleActivationChanged(true);
  } else if (is_active_) {
    is_active_ = false;
    desktop_native_widget_aura_->HandleActivationChanged(false);
  }
}

void DesktopWindowTreeHostMus::OnWindowPropertyChanged(aura::Window* window,
                                                       const void* key,
                                                       intptr_t old) {
  DCHECK_EQ(window, content_window());
  DCHECK(!window->GetRootWindow() || this->window() == window->GetRootWindow());
  if (!this->window())
    return;

  // Allow mus clients to mirror widget window properties to their root windows.
  MusPropertyMirror* property_mirror = MusClient::Get()->mus_property_mirror();
  if (property_mirror) {
    property_mirror->MirrorPropertyFromWidgetWindowToRootWindow(
        window, this->window(), key);
  }
}

void DesktopWindowTreeHostMus::ShowImpl() {
  // This code path is hit when the server initiated the change. In such a case
  // the window should not be made active. If the server wants the window to be
  // active, it will make the window active.
  Show(ui::SHOW_STATE_INACTIVE, gfx::Rect());
}

void DesktopWindowTreeHostMus::HideImpl() {
  // If |is_updating_window_visibility_| is true, this is being called in
  // response to window()'s visibility changing, in which case we need to
  // continue on to complete processing of the hide.
  if (!IsVisible() && !is_updating_window_visibility_)
    return;

  native_widget_delegate_->OnNativeWidgetVisibilityChanging(false);
  {
    base::AutoReset<bool> setter(&is_updating_window_visibility_, true);
    WindowTreeHostMus::HideImpl();
  }
  native_widget_delegate_->OnNativeWidgetVisibilityChanged(false);

  // When hiding we can't possibly be active any more. Reset the FocusClient,
  // which effectively triggers giving up focus (and activation). Mus will
  // eventually generate a focus event, but that's async. This should be done
  // after the window gets hidden actually, since some code (like
  // WindowActivityWatcher) assumes closing window is already invisible when the
  // focus is lost. See https://crbug.com/896080.
  if (IsFocusClientInstalledOnFocusSynchronizer()) {
    MusClient::Get()
        ->window_tree_client()
        ->focus_synchronizer()
        ->SetActiveFocusClient(nullptr, nullptr);
  }
}

void DesktopWindowTreeHostMus::SetBounds(
    const gfx::Rect& bounds,
    const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
  gfx::Rect final_bounds = bounds;
  // If the server initiated the bounds change, then we need to honor it.
  if (!is_server_setting_bounds() && bounds_in_dip().size() != bounds.size()) {
    gfx::Size size = bounds.size();
    size.SetToMax(native_widget_delegate_->GetMinimumSize());
    const gfx::Size max_size = native_widget_delegate_->GetMaximumSize();
    if (!max_size.IsEmpty())
      size.SetToMin(max_size);
    final_bounds.set_size(size);
    UpdateMinAndMaxSize();
  }
  WindowTreeHostMus::SetBounds(final_bounds, local_surface_id_allocation);
}

void DesktopWindowTreeHostMus::SetBoundsInPixels(
    const gfx::Rect& bounds_in_pixels,
    const viz::LocalSurfaceIdAllocation& local_surface_id_allocation) {
  // NOTE: in typical usage SetBounds() is called and not this, but as
  // WindowTreeHost exposes SetBoundsInPixels() this function may be called too.
  // If the server initiated the bounds change, then we need to honor it.
  gfx::Rect final_bounds_in_pixels = bounds_in_pixels;
  if (!is_server_setting_bounds() &&
      GetBoundsInPixels().size() != bounds_in_pixels.size()) {
    gfx::Size size = bounds_in_pixels.size();
    size.SetToMax(gfx::ConvertSizeToPixel(
        GetScaleFactor(), native_widget_delegate_->GetMinimumSize()));
    const gfx::Size max_size_in_pixels = gfx::ConvertSizeToPixel(
        GetScaleFactor(), native_widget_delegate_->GetMaximumSize());
    if (!max_size_in_pixels.IsEmpty())
      size.SetToMin(max_size_in_pixels);
    final_bounds_in_pixels.set_size(size);
    UpdateMinAndMaxSize();
  }
  WindowTreeHostMus::SetBoundsInPixels(final_bounds_in_pixels,
                                       local_surface_id_allocation);
}

void DesktopWindowTreeHostMus::OnViewBoundsChanged(View* observed_view) {
  DCHECK_EQ(
      observed_view,
      native_widget_delegate_->AsWidget()->non_client_view()->client_view());

  SendClientAreaToServer();
}

void DesktopWindowTreeHostMus::OnViewIsDeleting(View* observed_view) {
  observed_client_view_.Remove(observed_view);
}

void DesktopWindowTreeHostMus::SetTopViewInsetToDefault() {
  const auto& values = WindowManagerFrameValues::instance();
  window()->SetProperty(aura::client::kTopViewInset,
                        IsMaximized() ? values.maximized_insets.top()
                                      : values.normal_insets.top());
}

aura::Window* DesktopWindowTreeHostMus::content_window() {
  return desktop_native_widget_aura_->content_window();
}

}  // namespace views
