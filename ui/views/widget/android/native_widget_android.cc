// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/android/native_widget_android.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/screen.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/views/drag_utils.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/android/android_focus_rules.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/tooltip_manager_aura.h"
#include "ui/views/widget/widget_aura_utils.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/window_reorderer.h"
#include "ui/wm/core/default_activation_client.h"
#include "ui/wm/core/default_screen_position_client.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/dispatcher_client.h"
#include "ui/wm/public/drag_drop_client.h"
#include "ui/wm/public/window_move_client.h"
#include "ui/wm/public/window_types.h"

// TODO(bshe): Most of the code is copied from NativeWidgetAura or
// DesktopNativeWidgetAura. Share more code instead of duplicate code when
// possible. crbug.com/554961.
namespace {

class WindowTreeClientImpl : public aura::client::WindowTreeClient {
 public:
  explicit WindowTreeClientImpl(aura::Window* root_window)
      : root_window_(root_window) {
    aura::client::SetWindowTreeClient(root_window_, this);
  }
  ~WindowTreeClientImpl() override {
    aura::client::SetWindowTreeClient(root_window_, nullptr);
  }

  // Overridden from client::WindowTreeClient:
  aura::Window* GetDefaultParent(aura::Window* context,
                                 aura::Window* window,
                                 const gfx::Rect& bounds) override {
    return root_window_;
  }

 private:
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientImpl);
};

// TODO(bshe|jonross): Get rid of nested message loop once crbug.com/523680 is
// fixed.
class AndroidDispatcherClient : public aura::client::DispatcherClient {
 public:
  AndroidDispatcherClient() {}
  ~AndroidDispatcherClient() override {}

  // aura::client::DispatcherClient:
  void PrepareNestedLoopClosures(base::MessagePumpDispatcher* dispatcher,
                                 base::Closure* run_closure,
                                 base::Closure* quit_closure) override {
    scoped_ptr<base::RunLoop> run_loop(new base::RunLoop());
    *quit_closure = run_loop->QuitClosure();
    *run_closure =
        base::Bind(&AndroidDispatcherClient::RunNestedDispatcher,
                   base::Unretained(this), base::Passed(&run_loop), dispatcher);
  }

 private:
  void RunNestedDispatcher(scoped_ptr<base::RunLoop> run_loop,
                           base::MessagePumpDispatcher* dispatcher) {
    base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
    base::MessageLoop::ScopedNestableTaskAllower allow(loop);
    run_loop->Run();
  }

  DISALLOW_COPY_AND_ASSIGN(AndroidDispatcherClient);
};

}  // namespace

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAndroid, public

NativeWidgetAndroid::NativeWidgetAndroid(
    internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      window_(new aura::Window(this)),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      destroying_(false),
      cursor_(gfx::kNullCursor),
      saved_window_state_(ui::SHOW_STATE_DEFAULT),
      close_widget_factory_(this) {
  aura::client::SetFocusChangeObserver(window_, this);
  aura::client::SetActivationChangeObserver(window_, this);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAndroid, internal::NativeWidgetPrivate implementation:

void NativeWidgetAndroid::InitNativeWidget(const Widget::InitParams& params) {
  ownership_ = params.ownership;
  NativeWidgetAura::RegisterNativeWidgetForWindow(this, window_);

  window_->SetType(GetAuraWindowTypeForWidgetType(params.type));
  window_->Init(params.layer_type);
  wm::SetShadowType(window_, wm::SHADOW_TYPE_NONE);
  window_->Show();

  // TODO(bshe): Get rid of the hard coded size. Tracked in crbug.com/551923.
  host_.reset(aura::WindowTreeHost::Create(gfx::Rect(0, 0, 800, 600)));
  host_->InitHost();
  host_->AddObserver(this);

  window_tree_client_.reset(new WindowTreeClientImpl(host_->window()));

  focus_client_.reset(new wm::FocusController(new AndroidFocusRules));
  aura::client::SetFocusClient(host_->window(), focus_client_.get());
  host_->window()->AddPreTargetHandler(focus_client_.get());

  new wm::DefaultActivationClient(host_->window());

  capture_client_.reset(
      new aura::client::DefaultCaptureClient(host_->window()));

  screen_position_client_.reset(new wm::DefaultScreenPositionClient);
  aura::client::SetScreenPositionClient(host_->window(),
                                        screen_position_client_.get());
  dispatcher_client_.reset(new AndroidDispatcherClient);
  aura::client::SetDispatcherClient(host_->window(), dispatcher_client_.get());

  delegate_->OnNativeWidgetCreated(false);

  DCHECK(GetWidget()->GetRootView());
  if (params.type != Widget::InitParams::TYPE_TOOLTIP)
    tooltip_manager_.reset(new views::TooltipManagerAura(GetWidget()));

  if (params.type != Widget::InitParams::TYPE_TOOLTIP &&
      params.type != Widget::InitParams::TYPE_POPUP) {
    aura::client::SetDragDropDelegate(window_, this);
  }

  aura::client::SetActivationDelegate(window_, this);

  host_->window()->AddChild(window_);
  window_reorderer_.reset(
      new WindowReorderer(window_, GetWidget()->GetRootView()));

  // TODO(bshe): figure out how to add cursor manager, drag drop client and all
  // the necessary parts that exists in desktop_native_widget_aura.
}

void NativeWidgetAndroid::OnWidgetInitDone() {}

NonClientFrameView* NativeWidgetAndroid::CreateNonClientFrameView() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool NativeWidgetAndroid::ShouldUseNativeFrame() const {
  // There is only one frame type for aura.
  return false;
}

bool NativeWidgetAndroid::ShouldWindowContentsBeTransparent() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetAndroid::FrameTypeChanged() {
  NOTIMPLEMENTED();
}

Widget* NativeWidgetAndroid::GetWidget() {
  return delegate_->AsWidget();
}

const Widget* NativeWidgetAndroid::GetWidget() const {
  return delegate_->AsWidget();
}

gfx::NativeView NativeWidgetAndroid::GetNativeView() const {
  return window_;
}

gfx::NativeWindow NativeWidgetAndroid::GetNativeWindow() const {
  return window_;
}

Widget* NativeWidgetAndroid::GetTopLevelWidget() {
  return GetWidget();
}

const ui::Compositor* NativeWidgetAndroid::GetCompositor() const {
  return host_->compositor();
}

const ui::Layer* NativeWidgetAndroid::GetLayer() const {
  return GetNativeWindow()->layer();
}

void NativeWidgetAndroid::ReorderNativeViews() {
  window_reorderer_->ReorderChildWindows();
}

void NativeWidgetAndroid::ViewRemoved(View* view) {
  // TODO(bshe): Implement drag and drop. crbug.com/554029.
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::SetNativeWindowProperty(const char* name,
                                                  void* value) {
  GetNativeWindow()->SetNativeWindowProperty(name, value);
}

void* NativeWidgetAndroid::GetNativeWindowProperty(const char* name) const {
  return GetNativeWindow()->GetNativeWindowProperty(name);
}

TooltipManager* NativeWidgetAndroid::GetTooltipManager() const {
  return tooltip_manager_.get();
}

void NativeWidgetAndroid::SetCapture() {
  GetNativeWindow()->SetCapture();
}

void NativeWidgetAndroid::ReleaseCapture() {
  GetNativeWindow()->ReleaseCapture();
}

bool NativeWidgetAndroid::HasCapture() const {
  return GetNativeWindow()->HasCapture();
}

ui::InputMethod* NativeWidgetAndroid::GetInputMethod() {
  return host_->GetInputMethod();
}

void NativeWidgetAndroid::CenterWindow(const gfx::Size& size) {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
}

bool NativeWidgetAndroid::SetWindowTitle(const base::string16& title) {
  if (GetNativeWindow()->title() == title)
    return false;
  GetNativeWindow()->SetTitle(title);
  return true;
}

void NativeWidgetAndroid::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                         const gfx::ImageSkia& app_icon) {
  // TODO(bshe): Implement this. See crbug.com/554953.
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::InitModalType(ui::ModalType modal_type) {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
}

gfx::Rect NativeWidgetAndroid::GetWindowBoundsInScreen() const {
  return GetNativeWindow()->GetBoundsInScreen();
}

gfx::Rect NativeWidgetAndroid::GetClientAreaBoundsInScreen() const {
  // View-to-screen coordinate system transformations depend on this returning
  // the full window bounds, for example View::ConvertPointToScreen().
  return GetNativeWindow()->GetBoundsInScreen();
}

gfx::Rect NativeWidgetAndroid::GetRestoredBounds() const {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void NativeWidgetAndroid::SetBounds(const gfx::Rect& bounds) {
  // TODO(bshe): This may not work. We may need to resize SurfaceView too. See
  // crbug.com/554952.
  host_->SetBounds(bounds);
}

void NativeWidgetAndroid::SetSize(const gfx::Size& size) {
  gfx::Rect bounds = host_->GetBounds();
  SetBounds(gfx::Rect(bounds.origin(), size));
}

void NativeWidgetAndroid::StackAbove(gfx::NativeView native_view) {
  // TODO(bshe): Implements window stacking logic. See crbug.com/554047
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::StackAtTop() {
  // TODO(bshe): Implements window stacking logic. See crbug.com/554047
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::StackBelow(gfx::NativeView native_view) {
  // TODO(bshe): Implements window stacking logic. See crbug.com/554047
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::SetShape(SkRegion* region) {
  GetNativeWindow()->layer()->SetAlphaShape(make_scoped_ptr(region));
}

void NativeWidgetAndroid::Close() {
  // TODO(bshe): This might not be right. See crbug.com/554259.
  DCHECK(ownership_ == Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  GetNativeWindow()->SuppressPaint();
  Hide();
  GetNativeWindow()->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_NONE);

  if (!close_widget_factory_.HasWeakPtrs()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&NativeWidgetAndroid::CloseNow,
                              close_widget_factory_.GetWeakPtr()));
  }
}

void NativeWidgetAndroid::CloseNow() {
  // TODO(bshe): This might not be right. See crbug.com/554259.
  host_->RemoveObserver(this);
  host_.reset();
  delete window_;
}

void NativeWidgetAndroid::Show() {
  host_->Show();
  GetNativeWindow()->Show();
}

void NativeWidgetAndroid::Hide() {
  host_->Hide();
  GetNativeWindow()->Hide();
}

void NativeWidgetAndroid::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::ShowWithWindowState(ui::WindowShowState state) {
  NOTIMPLEMENTED();
}

bool NativeWidgetAndroid::IsVisible() const {
  return GetNativeWindow()->IsVisible();
}

void NativeWidgetAndroid::Activate() {
  aura::client::GetActivationClient(host_->window())
      ->ActivateWindow(GetNativeWindow());
}

void NativeWidgetAndroid::Deactivate() {
  aura::client::GetActivationClient(host_->window())
      ->DeactivateWindow(GetNativeWindow());
}

bool NativeWidgetAndroid::IsActive() const {
  return wm::IsActiveWindow(GetNativeWindow());
}

void NativeWidgetAndroid::SetAlwaysOnTop(bool on_top) {
  GetNativeWindow()->SetProperty(aura::client::kAlwaysOnTopKey, on_top);
}

bool NativeWidgetAndroid::IsAlwaysOnTop() const {
  return GetNativeWindow()->GetProperty(aura::client::kAlwaysOnTopKey);
}

void NativeWidgetAndroid::SetVisibleOnAllWorkspaces(bool always_visible) {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::Maximize() {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::Minimize() {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
}

bool NativeWidgetAndroid::IsMaximized() const {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
  return false;
}

bool NativeWidgetAndroid::IsMinimized() const {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetAndroid::Restore() {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::SetFullscreen(bool fullscreen) {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
}

bool NativeWidgetAndroid::IsFullscreen() const {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetAndroid::SetOpacity(unsigned char opacity) {
  GetNativeWindow()->layer()->SetOpacity(opacity / 255.0);
}

void NativeWidgetAndroid::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::FlashFrame(bool flash) {
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::RunShellDrag(
    View* view,
    const ui::OSExchangeData& data,
    const gfx::Point& location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::SchedulePaintInRect(const gfx::Rect& rect) {
  GetNativeWindow()->SchedulePaintInRect(rect);
}

void NativeWidgetAndroid::SetCursor(gfx::NativeCursor cursor) {
  cursor_ = cursor;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(host_->window());
  if (cursor_client)
    cursor_client->SetCursor(cursor);
}

bool NativeWidgetAndroid::IsMouseEventsEnabled() const {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(host_->window());
  return cursor_client ? cursor_client->IsMouseEventsEnabled() : true;
}

void NativeWidgetAndroid::ClearNativeFocus() {
  aura::client::FocusClient* client = aura::client::GetFocusClient(window_);
  if (client && window_->Contains(client->GetFocusedWindow()))
    client->ResetFocusWithinActiveWindow(window_);
}

gfx::Rect NativeWidgetAndroid::GetWorkAreaBoundsInScreen() const {
  return gfx::Screen::GetScreenFor(window_)
      ->GetDisplayNearestWindow(window_)
      .work_area();
}

Widget::MoveLoopResult NativeWidgetAndroid::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
  return Widget::MOVE_LOOP_SUCCESSFUL;
}

void NativeWidgetAndroid::EndMoveLoop() {
  // TODO(bshe): Implement this. See crbug.com/554208.
  NOTIMPLEMENTED();
}

void NativeWidgetAndroid::SetVisibilityChangedAnimationsEnabled(bool value) {
  GetNativeWindow()->SetProperty(aura::client::kAnimationsDisabledKey, !value);
}

void NativeWidgetAndroid::SetVisibilityAnimationDuration(
    const base::TimeDelta& duration) {
  wm::SetWindowVisibilityAnimationDuration(GetNativeWindow(), duration);
}

void NativeWidgetAndroid::SetVisibilityAnimationTransition(
    Widget::VisibilityTransition transition) {
  wm::WindowVisibilityAnimationTransition wm_transition = wm::ANIMATE_NONE;
  switch (transition) {
    case Widget::ANIMATE_SHOW:
      wm_transition = wm::ANIMATE_SHOW;
      break;
    case Widget::ANIMATE_HIDE:
      wm_transition = wm::ANIMATE_HIDE;
      break;
    case Widget::ANIMATE_BOTH:
      wm_transition = wm::ANIMATE_BOTH;
      break;
    case Widget::ANIMATE_NONE:
      wm_transition = wm::ANIMATE_NONE;
      break;
  }
  wm::SetWindowVisibilityAnimationTransition(GetNativeWindow(), wm_transition);
}

ui::NativeTheme* NativeWidgetAndroid::GetNativeTheme() const {
  return ui::NativeThemeAura::instance();
}

void NativeWidgetAndroid::OnRootViewLayout() {
  NOTIMPLEMENTED();
}

bool NativeWidgetAndroid::IsTranslucentWindowOpacitySupported() const {
  return true;
}

void NativeWidgetAndroid::OnSizeConstraintsChanged() {
  window_->SetProperty(aura::client::kCanMaximizeKey,
                       GetWidget()->widget_delegate()->CanMaximize());
  window_->SetProperty(aura::client::kCanMinimizeKey,
                       GetWidget()->widget_delegate()->CanMinimize());
  window_->SetProperty(aura::client::kCanResizeKey,
                       GetWidget()->widget_delegate()->CanResize());
}

void NativeWidgetAndroid::RepostNativeEvent(gfx::NativeEvent native_event) {
  OnEvent(native_event);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAndroid, aura::WindowDelegate implementation:

gfx::Size NativeWidgetAndroid::GetMinimumSize() const {
  return delegate_->GetMinimumSize();
}

gfx::Size NativeWidgetAndroid::GetMaximumSize() const {
  // If a window have a maximum size, the window should not be
  // maximizable.
  DCHECK(delegate_->GetMaximumSize().IsEmpty() ||
         !window_->GetProperty(aura::client::kCanMaximizeKey));
  return delegate_->GetMaximumSize();
}

void NativeWidgetAndroid::OnBoundsChanged(const gfx::Rect& old_bounds,
                                          const gfx::Rect& new_bounds) {
  // Assume that if the old bounds was completely empty a move happened. This
  // handles the case of a maximize animation acquiring the layer (acquiring a
  // layer results in clearing the bounds).
  if (old_bounds.origin() != new_bounds.origin() ||
      (old_bounds == gfx::Rect(0, 0, 0, 0) && !new_bounds.IsEmpty())) {
    delegate_->OnNativeWidgetMove();
  }
  if (old_bounds.size() != new_bounds.size())
    delegate_->OnNativeWidgetSizeChanged(new_bounds.size());
}

gfx::NativeCursor NativeWidgetAndroid::GetCursor(const gfx::Point& point) {
  return cursor_;
}

int NativeWidgetAndroid::GetNonClientComponent(const gfx::Point& point) const {
  return delegate_->GetNonClientComponent(point);
}

bool NativeWidgetAndroid::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  views::WidgetDelegate* widget_delegate = GetWidget()->widget_delegate();
  if (widget_delegate &&
      !widget_delegate->ShouldDescendIntoChildForEventHandling(child, location))
    return false;

  // Don't descend into |child| if there is a view with a Layer that contains
  // the point and is stacked above |child|s layer.
  typedef std::vector<ui::Layer*> Layers;
  const Layers& root_layers(delegate_->GetRootLayers());
  if (root_layers.empty())
    return true;

  Layers::const_iterator child_layer_iter(
      std::find(window_->layer()->children().begin(),
                window_->layer()->children().end(), child->layer()));
  if (child_layer_iter == window_->layer()->children().end())
    return true;

  for (std::vector<ui::Layer*>::const_reverse_iterator i = root_layers.rbegin();
       i != root_layers.rend(); ++i) {
    ui::Layer* layer = *i;
    if (layer->visible() && layer->bounds().Contains(location)) {
      Layers::const_iterator root_layer_iter(
          std::find(window_->layer()->children().begin(),
                    window_->layer()->children().end(), layer));
      if (root_layer_iter > child_layer_iter)
        return false;
    }
  }
  return true;
}

bool NativeWidgetAndroid::CanFocus() {
  return ShouldActivate();
}

void NativeWidgetAndroid::OnCaptureLost() {
  delegate_->OnMouseCaptureLost();
}

void NativeWidgetAndroid::OnPaint(const ui::PaintContext& context) {
  delegate_->OnNativeWidgetPaint(context);
}

void NativeWidgetAndroid::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  GetWidget()->DeviceScaleFactorChanged(device_scale_factor);
}

void NativeWidgetAndroid::OnWindowDestroying(aura::Window* window) {
  delegate_->OnNativeWidgetDestroying();

  // If the aura::Window is destroyed, we can no longer show tooltips.
  tooltip_manager_.reset();
}

void NativeWidgetAndroid::OnWindowDestroyed(aura::Window* window) {
  window_ = nullptr;
  delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

void NativeWidgetAndroid::OnWindowTargetVisibilityChanged(bool visible) {
  delegate_->OnNativeWidgetVisibilityChanged(visible);
}

bool NativeWidgetAndroid::HasHitTestMask() const {
  return delegate_->HasHitTestMask();
}

void NativeWidgetAndroid::GetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  delegate_->GetHitTestMask(mask);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAndroid, ui::EventHandler implementation:

void NativeWidgetAndroid::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(window_);
  // Renderer may send a key event back to us if the key event wasn't handled,
  // and the window may be invisible by that time.
  if (!window_->IsVisible())
    return;

  FocusManager* focus_manager = GetWidget()->GetFocusManager();
  delegate_->OnKeyEvent(event);
  if (!event->handled() && focus_manager)
    focus_manager->OnKeyEvent(*event);
  event->SetHandled();
}

void NativeWidgetAndroid::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(window_);
  DCHECK(window_->IsVisible());
  if (event->type() == ui::ET_MOUSEWHEEL) {
    delegate_->OnMouseEvent(event);
    if (event->handled())
      return;
  }

  if (tooltip_manager_.get())
    tooltip_manager_->UpdateTooltip();
  TooltipManagerAura::UpdateTooltipManagerForCapture(GetWidget());
  delegate_->OnMouseEvent(event);
}

void NativeWidgetAndroid::OnScrollEvent(ui::ScrollEvent* event) {
  delegate_->OnScrollEvent(event);
}

void NativeWidgetAndroid::OnGestureEvent(ui::GestureEvent* event) {
  DCHECK(window_);
  DCHECK(window_->IsVisible() || event->IsEndingEvent());
  delegate_->OnGestureEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAndroid, aura::client::ActivationDelegate implementation:

bool NativeWidgetAndroid::ShouldActivate() const {
  return delegate_->CanActivate();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAndroid, aura::client::ActivationChangeObserver
// implementation:

void NativeWidgetAndroid::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  DCHECK(window_ == gained_active || window_ == lost_active);
  if (GetWidget()->GetFocusManager()) {
    if (window_ == gained_active)
      GetWidget()->GetFocusManager()->RestoreFocusedView();
    else if (window_ == lost_active)
      GetWidget()->GetFocusManager()->StoreFocusedView(true);
  }
  delegate_->OnNativeWidgetActivationChanged(window_ == gained_active);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAndroid, aura::client::FocusChangeObserver:

void NativeWidgetAndroid::OnWindowFocused(aura::Window* gained_focus,
                                          aura::Window* lost_focus) {
  if (window_ == gained_focus)
    delegate_->OnNativeFocus();
  else if (window_ == lost_focus)
    delegate_->OnNativeBlur();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAndroid, aura::WindowDragDropDelegate implementation:

void NativeWidgetAndroid::OnDragEntered(const ui::DropTargetEvent& event) {
  // TODO: Implement drag and drop. crbug.com/554029.
  NOTIMPLEMENTED();
}

int NativeWidgetAndroid::OnDragUpdated(const ui::DropTargetEvent& event) {
  // TODO: Implement drag and drop. crbug.com/554029.
  NOTIMPLEMENTED();
  return 0;
}

void NativeWidgetAndroid::OnDragExited() {
  // TODO: Implement drag and drop. crbug.com/554029.
  NOTIMPLEMENTED();
}

int NativeWidgetAndroid::OnPerformDrop(const ui::DropTargetEvent& event) {
  // TODO: Implement drag and drop. crbug.com/554029.
  NOTIMPLEMENTED();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAndroid, aura::WindowTreeHostObserver implementation:

void NativeWidgetAndroid::OnHostCloseRequested(
    const aura::WindowTreeHost* host) {
  GetWidget()->Close();
}

void NativeWidgetAndroid::OnHostResized(const aura::WindowTreeHost* host) {
  gfx::Rect new_bounds = gfx::Rect(host_->window()->bounds().size());
  GetNativeWindow()->SetBounds(new_bounds);
  delegate_->OnNativeWidgetSizeChanged(new_bounds.size());
}

void NativeWidgetAndroid::OnHostMoved(const aura::WindowTreeHost* host,
                                      const gfx::Point& new_origin) {
  TRACE_EVENT1("views", "NativeWidgetAndroid::OnHostMoved", "new_origin",
               new_origin.ToString());

  delegate_->OnNativeWidgetMove();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAndroid, protected:

NativeWidgetAndroid::~NativeWidgetAndroid() {
  destroying_ = true;
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete delegate_;
  else
    CloseNow();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAndroid, private:

bool NativeWidgetAndroid::IsDocked() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetAndroid::SetInitialFocus(ui::WindowShowState show_state) {
  // The window does not get keyboard messages unless we focus it.
  if (!GetWidget()->SetInitialFocus(show_state))
    window_->Focus();
}

}  // namespace views
