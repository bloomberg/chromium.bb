// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_aura.h"

#include "base/bind.h"
#include "base/string_util.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/window_move_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/env.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/screen.h"
#include "ui/views/drag_utils.h"
#include "ui/views/ime/input_method_bridge.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/drop_helper.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/native_widget_helper_aura.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/tooltip_manager_aura.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_WIN)
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#endif

namespace views {

namespace {

aura::client::WindowType GetAuraWindowTypeForWidgetType(
    Widget::InitParams::Type type) {
  switch (type) {
    case Widget::InitParams::TYPE_WINDOW:
      return aura::client::WINDOW_TYPE_NORMAL;
    case Widget::InitParams::TYPE_PANEL:
      return aura::client::WINDOW_TYPE_PANEL;
    case Widget::InitParams::TYPE_CONTROL:
      return aura::client::WINDOW_TYPE_CONTROL;
    case Widget::InitParams::TYPE_WINDOW_FRAMELESS:
    case Widget::InitParams::TYPE_POPUP:
    case Widget::InitParams::TYPE_BUBBLE:
      return aura::client::WINDOW_TYPE_POPUP;
    case Widget::InitParams::TYPE_MENU:
      return aura::client::WINDOW_TYPE_MENU;
    case Widget::InitParams::TYPE_TOOLTIP:
      return aura::client::WINDOW_TYPE_TOOLTIP;
    default:
      NOTREACHED() << "Unhandled widget type " << type;
      return aura::client::WINDOW_TYPE_UNKNOWN;
  }
}

const gfx::Rect* GetRestoreBounds(aura::Window* window) {
  return window->GetProperty(aura::client::kRestoreBoundsKey);
}

void SetRestoreBounds(aura::Window* window, const gfx::Rect& bounds) {
  window->SetProperty(aura::client::kRestoreBoundsKey, new gfx::Rect(bounds));
}

}  // namespace

// Used when SetInactiveRenderingDisabled() is invoked to track when active
// status changes in such a way that we should enable inactive rendering.
class NativeWidgetAura::ActiveWindowObserver : public aura::WindowObserver {
 public:
  explicit ActiveWindowObserver(NativeWidgetAura* host) : host_(host) {
    host_->GetNativeView()->GetRootWindow()->AddObserver(this);
    host_->GetNativeView()->AddObserver(this);
  }
  virtual ~ActiveWindowObserver() {
    CleanUpObservers();
  }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE {
    if (window != host_->GetNativeView() ||
        key != aura::client::kRootWindowActiveWindowKey) {
      return;
    }
    aura::RootWindow* root_window = window->GetRootWindow();
    aura::Window* active =
        aura::client::GetActivationClient(root_window)->GetActiveWindow();
    if (!active || (active != host_->window_ &&
                    active->transient_parent() != host_->window_)) {
      host_->delegate_->EnableInactiveRendering();
    }
  }

  virtual void OnWindowRemovingFromRootWindow(aura::Window* window) OVERRIDE {
    if (window != host_->GetNativeView())
      return;
    CleanUpObservers();
  }

 private:
  void CleanUpObservers() {
    if (!host_)
      return;
    host_->GetNativeView()->GetRootWindow()->RemoveObserver(this);
    host_->GetNativeView()->RemoveObserver(this);
    host_ = NULL;
  }

  NativeWidgetAura* host_;

  DISALLOW_COPY_AND_ASSIGN(ActiveWindowObserver);
};

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, public:

NativeWidgetAura::NativeWidgetAura(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(desktop_helper_(
          ViewsDelegate::views_delegate ?
          ViewsDelegate::views_delegate->CreateNativeWidgetHelper(this) :
          NULL)),
      ALLOW_THIS_IN_INITIALIZER_LIST(window_(new aura::Window(this))),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)),
      can_activate_(true),
      cursor_(gfx::kNullCursor),
      saved_window_state_(ui::SHOW_STATE_DEFAULT) {
}

NativeWidgetAura::~NativeWidgetAura() {
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete delegate_;
  else
    CloseNow();
}

// static
gfx::Font NativeWidgetAura::GetWindowTitleFont() {
#if defined(OS_WIN)
  NONCLIENTMETRICS ncm;
  base::win::GetNonClientMetrics(&ncm);
  l10n_util::AdjustUIFont(&(ncm.lfCaptionFont));
  base::win::ScopedHFONT caption_font(CreateFontIndirect(&(ncm.lfCaptionFont)));
  return gfx::Font(caption_font);
#else
  return gfx::Font();
#endif
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, internal::NativeWidgetPrivate implementation:

void NativeWidgetAura::InitNativeWidget(const Widget::InitParams& params) {
  ownership_ = params.ownership;

  if (desktop_helper_.get())
    desktop_helper_->PreInitialize(params);

  window_->set_user_data(this);
  window_->SetType(GetAuraWindowTypeForWidgetType(params.type));
  window_->SetProperty(aura::client::kShowStateKey, params.show_state);
  if (params.type == Widget::InitParams::TYPE_BUBBLE)
    aura::client::SetHideOnDeactivate(window_, true);
  window_->SetTransparent(params.transparent);
  window_->Init(params.layer_type);
  if (params.type == Widget::InitParams::TYPE_CONTROL)
    window_->Show();

  delegate_->OnNativeWidgetCreated();
  if (desktop_helper_.get() && desktop_helper_->GetRootWindow()) {
    window_->SetParent(desktop_helper_->GetRootWindow());
  } else if (params.child) {
    window_->SetParent(params.GetParent());
  } else {
    // Set up the transient child before the window is added. This way the
    // LayoutManager knows the window has a transient parent.
    gfx::NativeView parent = params.GetParent();
    if (parent && parent->type() != aura::client::WINDOW_TYPE_UNKNOWN) {
      parent->AddTransientChild(window_);

      // TODO(erg): The StackingClient interface implies that there is only a
      // single root window. Solving this would require setting a stacking
      // client per root window instead. For now, we hax our way around this by
      // forcing the parent to be the root window instead of passing NULL as
      // the parent which will dispatch to the stacking client.
      if (desktop_helper_.get())
        parent = parent->GetRootWindow();
      else
        parent = NULL;
    }
    // SetAlwaysOnTop before SetParent so that always-on-top container is used.
    SetAlwaysOnTop(params.keep_on_top);
    window_->SetParent(parent);
  }

  // Wait to set the bounds until we have a parent. That way we can know our
  // true state/bounds (the LayoutManager may enforce a particular
  // state/bounds).
  if (IsMaximized())
    SetRestoreBounds(window_, params.bounds);
  else
    SetBounds(params.bounds);
  window_->set_ignore_events(!params.accept_events);
  can_activate_ =
      params.can_activate && params.type != Widget::InitParams::TYPE_CONTROL;
  DCHECK(GetWidget()->GetRootView());
#if !defined(OS_MACOSX)
  if (params.type != Widget::InitParams::TYPE_TOOLTIP)
    tooltip_manager_.reset(new views::TooltipManagerAura(this));
#endif  // !defined(OS_MACOSX)

  drop_helper_.reset(new DropHelper(GetWidget()->GetRootView()));
  if (params.type != Widget::InitParams::TYPE_TOOLTIP &&
      params.type != Widget::InitParams::TYPE_POPUP) {
    aura::client::SetDragDropDelegate(window_, this);
  }

  aura::client::SetActivationDelegate(window_, this);

  if (desktop_helper_.get()) {
    desktop_helper_->PostInitialize();
    // TODO(erg): Move this somewhere else?
    desktop_helper_->ShowRootWindow();
  }
}

NonClientFrameView* NativeWidgetAura::CreateNonClientFrameView() {
  return NULL;
}

void NativeWidgetAura::UpdateFrameAfterFrameChange() {
}

bool NativeWidgetAura::ShouldUseNativeFrame() const {
  // There is only one frame type for aura.
  return false;
}

void NativeWidgetAura::FrameTypeChanged() {
  // This is called when the Theme has changed; forward the event to the root
  // widget.
  GetWidget()->ThemeChanged();
  GetWidget()->GetRootView()->SchedulePaint();
}

Widget* NativeWidgetAura::GetWidget() {
  return delegate_->AsWidget();
}

const Widget* NativeWidgetAura::GetWidget() const {
  return delegate_->AsWidget();
}

gfx::NativeView NativeWidgetAura::GetNativeView() const {
  return window_;
}

gfx::NativeWindow NativeWidgetAura::GetNativeWindow() const {
  return window_;
}

Widget* NativeWidgetAura::GetTopLevelWidget() {
  NativeWidgetPrivate* native_widget = GetTopLevelNativeWidget(GetNativeView());
  return native_widget ? native_widget->GetWidget() : NULL;
}

const ui::Compositor* NativeWidgetAura::GetCompositor() const {
  return window_->layer()->GetCompositor();
}

ui::Compositor* NativeWidgetAura::GetCompositor() {
  return window_->layer()->GetCompositor();
}

void NativeWidgetAura::CalculateOffsetToAncestorWithLayer(
    gfx::Point* offset,
    ui::Layer** layer_parent) {
  if (layer_parent)
    *layer_parent = window_->layer();
}

void NativeWidgetAura::ViewRemoved(View* view) {
  DCHECK(drop_helper_.get() != NULL);
  drop_helper_->ResetTargetViewIfEquals(view);
}

void NativeWidgetAura::SetNativeWindowProperty(const char* name, void* value) {
  if (window_)
    window_->SetNativeWindowProperty(name, value);
}

void* NativeWidgetAura::GetNativeWindowProperty(const char* name) const {
  return window_ ? window_->GetNativeWindowProperty(name) : NULL;
}

TooltipManager* NativeWidgetAura::GetTooltipManager() const {
  return tooltip_manager_.get();
}

bool NativeWidgetAura::IsScreenReaderActive() const {
  // http://crbug.com/102570
  //NOTIMPLEMENTED();
  return false;
}

void NativeWidgetAura::SendNativeAccessibilityEvent(
    View* view,
    ui::AccessibilityTypes::Event event_type) {
  // http://crbug.com/102570
  //NOTIMPLEMENTED();
}

void NativeWidgetAura::SetCapture(unsigned int flags) {
  window_->SetCapture(flags);
}

void NativeWidgetAura::ReleaseCapture() {
  window_->ReleaseCapture();
}

bool NativeWidgetAura::HasCapture(unsigned int flags) const {
  return window_->HasCapture(flags);
}

InputMethod* NativeWidgetAura::CreateInputMethod() {
  aura::RootWindow* root_window = window_->GetRootWindow();
  ui::InputMethod* host =
      root_window->GetProperty(aura::client::kRootWindowInputMethodKey);
  InputMethod* input_method = new InputMethodBridge(this, host);
  input_method->Init(GetWidget());
  return input_method;
}

void NativeWidgetAura::CenterWindow(const gfx::Size& size) {
  gfx::Rect parent_bounds(window_->parent()->GetBoundsInRootWindow());
  // When centering window, we take the intersection of the host and
  // the parent. We assume the root window represents the visible
  // rect of a single screen.
  gfx::Rect work_area =
      gfx::Screen::GetMonitorNearestWindow(window_).work_area();
  parent_bounds = parent_bounds.Intersect(work_area);

  // If |window_|'s transient parent's bounds are big enough to fit it, then we
  // center it with respect to the transient parent.
  if (window_->transient_parent()) {
    gfx::Rect transient_parent_rect = window_->transient_parent()->
        GetBoundsInRootWindow().Intersect(work_area);
    if (transient_parent_rect.height() >= size.height() &&
        transient_parent_rect.width() >= size.width())
      parent_bounds = transient_parent_rect;
  }

  gfx::Rect window_bounds(
      parent_bounds.x() + (parent_bounds.width() - size.width()) / 2,
      parent_bounds.y() + (parent_bounds.height() - size.height()) / 2,
      size.width(),
      size.height());
  window_bounds = window_bounds.AdjustToFit(work_area);

  // Convert the bounds back relative to the parent.
  gfx::Point origin = window_bounds.origin();
  aura::Window::ConvertPointToWindow(window_->GetRootWindow(),
      window_->parent(), &origin);
  window_bounds.set_origin(origin);
  window_->SetBounds(window_bounds);
}

void NativeWidgetAura::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  // The interface specifies returning restored bounds, not current bounds.
  *bounds = GetRestoredBounds();
  *show_state = window_->GetProperty(aura::client::kShowStateKey);
}

void NativeWidgetAura::SetWindowTitle(const string16& title) {
  window_->set_title(title);
}

void NativeWidgetAura::SetWindowIcons(const SkBitmap& window_icon,
                                     const SkBitmap& app_icon) {
  // Aura doesn't have window icons.
}

void NativeWidgetAura::SetAccessibleName(const string16& name) {
  // http://crbug.com/102570
  //NOTIMPLEMENTED();
}

void NativeWidgetAura::SetAccessibleRole(ui::AccessibilityTypes::Role role) {
  // http://crbug.com/102570
  //NOTIMPLEMENTED();
}

void NativeWidgetAura::SetAccessibleState(ui::AccessibilityTypes::State state) {
  // http://crbug.com/102570
  //NOTIMPLEMENTED();
}

void NativeWidgetAura::InitModalType(ui::ModalType modal_type) {
  if (modal_type != ui::MODAL_TYPE_NONE)
    window_->SetProperty(aura::client::kModalKey, modal_type);
}

gfx::Rect NativeWidgetAura::GetWindowScreenBounds() const {
  gfx::Rect bounds = window_->GetBoundsInRootWindow();
  if (desktop_helper_.get())
    bounds = desktop_helper_->ChangeRootWindowBoundsToScreenBounds(bounds);
  return bounds;
}

gfx::Rect NativeWidgetAura::GetClientAreaScreenBounds() const {
  // View-to-screen coordinate system transformations depend on this returning
  // the full window bounds, for example View::ConvertPointToScreen().
  gfx::Rect bounds = window_->GetBoundsInRootWindow();
  if (desktop_helper_.get())
    bounds = desktop_helper_->ChangeRootWindowBoundsToScreenBounds(bounds);
  return bounds;
}

gfx::Rect NativeWidgetAura::GetRestoredBounds() const {
  // Restored bounds should only be relevant if the window is minimized or
  // maximized. However, in some places the code expectes GetRestoredBounds()
  // to return the current window bounds if the window is not in either state.
  if (!IsMinimized() && !IsMaximized() && !IsFullscreen())
    return window_->bounds();
  gfx::Rect* restore_bounds =
      window_->GetProperty(aura::client::kRestoreBoundsKey);
  return restore_bounds ? *restore_bounds : window_->bounds();
}

void NativeWidgetAura::SetBounds(const gfx::Rect& in_bounds) {
  gfx::Rect bounds = in_bounds;
  if (desktop_helper_.get())
    bounds = desktop_helper_->ModifyAndSetBounds(bounds);
  window_->SetBounds(bounds);
}

void NativeWidgetAura::SetSize(const gfx::Size& size) {
  window_->SetBounds(gfx::Rect(window_->bounds().origin(), size));
}

void NativeWidgetAura::StackAbove(gfx::NativeView native_view) {
  if (window_->parent() && window_->parent() == native_view->parent())
    window_->parent()->StackChildAbove(window_, native_view);
}

void NativeWidgetAura::StackAtTop() {
  window_->parent()->StackChildAtTop(window_);
}

void NativeWidgetAura::StackBelow(gfx::NativeView native_view) {
  if (window_->parent() && window_->parent() == native_view->parent())
    window_->parent()->StackChildBelow(window_, native_view);
}

void NativeWidgetAura::SetShape(gfx::NativeRegion region) {
  // No need for this. Just delete and ignore.
  delete region;
}

void NativeWidgetAura::Close() {
  // |window_| may already be deleted by parent window. This can happen
  // when this widget is child widget or has transient parent
  // and ownership is WIDGET_OWNS_NATIVE_WIDGET.
  DCHECK(window_ ||
         ownership_ == Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  if (window_) {
    window_->SuppressPaint();
    Hide();
    window_->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_NONE);
  }

  if (!close_widget_factory_.HasWeakPtrs()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&NativeWidgetAura::CloseNow,
                   close_widget_factory_.GetWeakPtr()));
  }
}

void NativeWidgetAura::CloseNow() {
  delete window_;
}

void NativeWidgetAura::Show() {
  ShowWithWindowState(ui::SHOW_STATE_INACTIVE);
}

void NativeWidgetAura::Hide() {
  window_->Hide();
}

void NativeWidgetAura::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  SetRestoreBounds(window_, restored_bounds);
  ShowWithWindowState(ui::SHOW_STATE_MAXIMIZED);
}

void NativeWidgetAura::ShowWithWindowState(ui::WindowShowState state) {
  if (state == ui::SHOW_STATE_MAXIMIZED || state == ui::SHOW_STATE_FULLSCREEN)
    window_->SetProperty(aura::client::kShowStateKey, state);
  window_->Show();
  if (can_activate_) {
    if (state != ui::SHOW_STATE_INACTIVE)
      Activate();
    // SetInitialFocus() should be always be called, even for
    // SHOW_STATE_INACTIVE. When a frameless modal dialog is created by
    // a widget of TYPE_WINDOW_FRAMELESS, Widget::Show() will call into
    // this function with the window state SHOW_STATE_INACTIVE,
    // SetInitialFoucs() has to be called so that the dialog can get focus.
    // This also matches NativeWidgetWin which invokes SetInitialFocus
    // regardless of show state.
    SetInitialFocus();
  }
}

bool NativeWidgetAura::IsVisible() const {
  return window_->IsVisible();
}

void NativeWidgetAura::Activate() {
  aura::client::GetActivationClient(window_->GetRootWindow())->ActivateWindow(
      window_);
}

void NativeWidgetAura::Deactivate() {
  aura::client::GetActivationClient(window_->GetRootWindow())->DeactivateWindow(
      window_);
}

bool NativeWidgetAura::IsActive() const {
  return aura::client::GetActivationClient(window_->GetRootWindow())->
      GetActiveWindow() == window_;
}

void NativeWidgetAura::SetAlwaysOnTop(bool on_top) {
  window_->SetProperty(aura::client::kAlwaysOnTopKey, on_top);
}

void NativeWidgetAura::Maximize() {
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
}

void NativeWidgetAura::Minimize() {
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
}

bool NativeWidgetAura::IsMaximized() const {
  return window_->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MAXIMIZED;
}

bool NativeWidgetAura::IsMinimized() const {
  return window_->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MINIMIZED;
}

void NativeWidgetAura::Restore() {
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
}

void NativeWidgetAura::SetFullscreen(bool fullscreen) {
  if (IsFullscreen() == fullscreen)
    return;  // Nothing to do.

  // Save window state before entering full screen so that it could restored
  // when exiting full screen.
  if (fullscreen)
    saved_window_state_ = window_->GetProperty(aura::client::kShowStateKey);

  window_->SetProperty(
      aura::client::kShowStateKey,
      fullscreen ? ui::SHOW_STATE_FULLSCREEN : saved_window_state_);
}

bool NativeWidgetAura::IsFullscreen() const {
  return window_->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_FULLSCREEN;
}

void NativeWidgetAura::SetOpacity(unsigned char opacity) {
  window_->layer()->SetOpacity(opacity / 255.0);
}

void NativeWidgetAura::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::FlashFrame(bool flash) {
  NOTIMPLEMENTED();
}

bool NativeWidgetAura::IsAccessibleWidget() const {
  // http://crbug.com/102570
  //NOTIMPLEMENTED();
  return false;
}

void NativeWidgetAura::RunShellDrag(View* view,
                                    const ui::OSExchangeData& data,
                                    const gfx::Point& location,
                                    int operation) {
  views::RunShellDrag(window_, data, location, operation);
}

void NativeWidgetAura::SchedulePaintInRect(const gfx::Rect& rect) {
  if (window_)
    window_->SchedulePaintInRect(rect);
}

void NativeWidgetAura::SetCursor(gfx::NativeCursor cursor) {
  cursor_ = cursor;
  window_->GetRootWindow()->SetCursor(cursor);
}

void NativeWidgetAura::ClearNativeFocus() {
  if (window_ && window_->GetFocusManager() &&
      window_->Contains(window_->GetFocusManager()->GetFocusedWindow()))
    window_->GetFocusManager()->SetFocusedWindow(window_, NULL);
}

void NativeWidgetAura::FocusNativeView(gfx::NativeView native_view) {
  window_->GetFocusManager()->SetFocusedWindow(native_view, NULL);
}

gfx::Rect NativeWidgetAura::GetWorkAreaBoundsInScreen() const {
  return gfx::Screen::GetMonitorNearestWindow(GetNativeView()).work_area();
}

void NativeWidgetAura::SetInactiveRenderingDisabled(bool value) {
  if (!value)
    active_window_observer_.reset();
  else
    active_window_observer_.reset(new ActiveWindowObserver(this));
}

Widget::MoveLoopResult NativeWidgetAura::RunMoveLoop() {
  if (window_->parent() &&
      aura::client::GetWindowMoveClient(window_->parent())) {
    SetCapture(ui::CW_LOCK_MOUSE);
    aura::client::GetWindowMoveClient(window_->parent())->RunMoveLoop(window_);
    return Widget::MOVE_LOOP_SUCCESSFUL;
  }
  return Widget::MOVE_LOOP_CANCELED;
}

void NativeWidgetAura::EndMoveLoop() {
  if (window_->parent() &&
      aura::client::GetWindowMoveClient(window_->parent())) {
    aura::client::GetWindowMoveClient(window_->parent())->EndMoveLoop();
  }
}

void NativeWidgetAura::SetVisibilityChangedAnimationsEnabled(bool value) {
  window_->SetProperty(aura::client::kAnimationsDisabledKey, !value);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, views::InputMethodDelegate implementation:

void NativeWidgetAura::DispatchKeyEventPostIME(const KeyEvent& key) {
  FocusManager* focus_manager = GetWidget()->GetFocusManager();
  if (focus_manager)
    focus_manager->MaybeResetMenuKeyState(key);
  if (delegate_->OnKeyEvent(key) || !focus_manager)
    return;
  focus_manager->OnKeyEvent(key);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, aura::WindowDelegate implementation:

gfx::Size NativeWidgetAura::GetMinimumSize() const {
  return delegate_->GetMinimumSize();
}

void NativeWidgetAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                       const gfx::Rect& new_bounds) {
  if (old_bounds.origin() != new_bounds.origin())
    delegate_->OnNativeWidgetMove();
  if (old_bounds.size() != new_bounds.size())
    delegate_->OnNativeWidgetSizeChanged(new_bounds.size());
}

void NativeWidgetAura::OnFocus() {
  // In aura, it is possible for child native widgets to take input and focus,
  // this differs from the behavior on windows.
  GetWidget()->GetInputMethod()->OnFocus();
  delegate_->OnNativeFocus(window_);
}

void NativeWidgetAura::OnBlur() {
  // Not only top level native widget can take input and focus, child
  // widgets are allowed also.
  GetWidget()->GetInputMethod()->OnBlur();
  delegate_->OnNativeBlur(window_->GetFocusManager()->GetFocusedWindow());
}

bool NativeWidgetAura::OnKeyEvent(aura::KeyEvent* event) {
  if (event->is_char()) {
    // If a ui::InputMethod object is attched to the root window, character
    // events are handled inside the object and are not passed to this function.
    // If such object is not attached, character events might be sent (e.g. on
    // Windows). In this case, we just skip these.
    return false;
  }
  // Renderer may send a key event back to us if the key event wasn't handled,
  // and the window may be invisible by that time.
  if (!window_->IsVisible())
    return false;
  InputMethod* input_method = GetWidget()->GetInputMethod();
  DCHECK(input_method);
  KeyEvent views_event(event);
  input_method->DispatchKeyEvent(views_event);
  return true;
}

gfx::NativeCursor NativeWidgetAura::GetCursor(const gfx::Point& point) {
  return cursor_;
}

int NativeWidgetAura::GetNonClientComponent(const gfx::Point& point) const {
  return delegate_->GetNonClientComponent(point);
}

bool NativeWidgetAura::OnMouseEvent(aura::MouseEvent* event) {
  DCHECK(window_->IsVisible());
  if (event->type() == ui::ET_MOUSEWHEEL) {
    MouseWheelEvent wheel_event(event);
    return delegate_->OnMouseEvent(wheel_event);
  }
  if (event->type() == ui::ET_SCROLL) {
    ScrollEvent scroll_event(static_cast<aura::ScrollEvent*>(event));
    if (delegate_->OnMouseEvent(scroll_event))
      return true;

    // Convert unprocessed scroll events into wheel events.
    MouseWheelEvent wheel_event(scroll_event);
    return delegate_->OnMouseEvent(wheel_event);
  }
  MouseEvent mouse_event(event);
  if (tooltip_manager_.get())
    tooltip_manager_->UpdateTooltip();
  return delegate_->OnMouseEvent(mouse_event);
}

ui::TouchStatus NativeWidgetAura::OnTouchEvent(aura::TouchEvent* event) {
  DCHECK(window_->IsVisible());
  TouchEvent touch_event(event);
  return delegate_->OnTouchEvent(touch_event);
}

ui::GestureStatus NativeWidgetAura::OnGestureEvent(aura::GestureEvent* event) {
  DCHECK(window_->IsVisible());
  GestureEvent gesture_event(event);
  return delegate_->OnGestureEvent(gesture_event);
}

bool NativeWidgetAura::CanFocus() {
  return can_activate_;
}

void NativeWidgetAura::OnCaptureLost() {
  delegate_->OnMouseCaptureLost();
}

void NativeWidgetAura::OnPaint(gfx::Canvas* canvas) {
  delegate_->OnNativeWidgetPaint(canvas);
}

void NativeWidgetAura::OnWindowDestroying() {
  delegate_->OnNativeWidgetDestroying();

  // If the aura::Window is destroyed, we can no longer show tooltips.
  tooltip_manager_.reset();
}

void NativeWidgetAura::OnWindowDestroyed() {
  window_ = NULL;
  tooltip_manager_.reset();
  delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

void NativeWidgetAura::OnWindowVisibilityChanged(bool visible) {
  delegate_->OnNativeWidgetVisibilityChanged(visible);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, aura::ActivationDelegate implementation:

bool NativeWidgetAura::ShouldActivate(const aura::Event* event) {
  return can_activate_ && delegate_->CanActivate();
}

void NativeWidgetAura::OnActivated() {
  if (GetWidget()->HasFocusManager())
    GetWidget()->GetFocusManager()->RestoreFocusedView();
  delegate_->OnNativeWidgetActivationChanged(true);
  if (IsVisible() && GetWidget()->non_client_view())
    GetWidget()->non_client_view()->SchedulePaint();
}

void NativeWidgetAura::OnLostActive() {
  if (GetWidget()->HasFocusManager())
    GetWidget()->GetFocusManager()->StoreFocusedView();
  delegate_->OnNativeWidgetActivationChanged(false);
  if (IsVisible() && GetWidget()->non_client_view())
    GetWidget()->non_client_view()->SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, aura::WindowDragDropDelegate implementation:

void NativeWidgetAura::OnDragEntered(const aura::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != NULL);
  last_drop_operation_ = drop_helper_->OnDragOver(event.data(),
      event.location(), event.source_operations());
}

int NativeWidgetAura::OnDragUpdated(const aura::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != NULL);
  last_drop_operation_ = drop_helper_->OnDragOver(event.data(),
      event.location(), event.source_operations());
  return last_drop_operation_;
}

void NativeWidgetAura::OnDragExited() {
  DCHECK(drop_helper_.get() != NULL);
  drop_helper_->OnDragExit();
}

int NativeWidgetAura::OnPerformDrop(const aura::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != NULL);
  return drop_helper_->OnDrop(event.data(), event.location(),
      last_drop_operation_);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, private:

void NativeWidgetAura::SetInitialFocus() {
  // The window does not get keyboard messages unless we focus it.
  if (!GetWidget()->SetInitialFocus())
    window_->Focus();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
void Widget::NotifyLocaleChanged() {
  // Deliberately not implemented.
}

// static
void Widget::CloseAllSecondaryWidgets() {
  // Deliberately not implemented.
}

bool Widget::ConvertRect(const Widget* source,
                         const Widget* target,
                         gfx::Rect* rect) {
  return false;
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, public:

// static
NativeWidgetPrivate* NativeWidgetPrivate::CreateNativeWidget(
    internal::NativeWidgetDelegate* delegate) {
  return new NativeWidgetAura(delegate);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeView(
    gfx::NativeView native_view) {
  return reinterpret_cast<NativeWidgetAura*>(native_view->user_data());
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
    gfx::NativeWindow native_window) {
  return reinterpret_cast<NativeWidgetAura*>(native_window->user_data());
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetTopLevelNativeWidget(
    gfx::NativeView native_view) {
  aura::Window* window = native_view;
  NativeWidgetPrivate* top_level_native_widget = NULL;
  while (window) {
    NativeWidgetPrivate* native_widget = GetNativeWidgetForNativeView(window);
    if (native_widget)
      top_level_native_widget = native_widget;
    window = window->parent();
  }
  return top_level_native_widget;
}

// static
void NativeWidgetPrivate::GetAllChildWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* children) {
  {
    // Code expects widget for |native_view| to be added to |children|.
    NativeWidgetAura* native_widget = static_cast<NativeWidgetAura*>(
        GetNativeWidgetForNativeView(native_view));
    if (native_widget->GetWidget())
      children->insert(native_widget->GetWidget());
  }

  const aura::Window::Windows& child_windows = native_view->children();
  for (aura::Window::Windows::const_iterator i = child_windows.begin();
       i != child_windows.end(); ++i) {
    NativeWidgetAura* native_widget =
        static_cast<NativeWidgetAura*>(GetNativeWidgetForNativeView(*i));
    if (native_widget)
      children->insert(native_widget->GetWidget());
  }
}

// static
void NativeWidgetPrivate::ReparentNativeView(gfx::NativeView native_view,
                                             gfx::NativeView new_parent) {
  DCHECK(native_view != new_parent);

  gfx::NativeView previous_parent = native_view->parent();
  if (previous_parent == new_parent)
    return;

  Widget::Widgets widgets;
  GetAllChildWidgets(native_view, &widgets);

  // First notify all the widgets that they are being disassociated
  // from their previous parent.
  for (Widget::Widgets::iterator it = widgets.begin();
      it != widgets.end(); ++it) {
    (*it)->NotifyNativeViewHierarchyChanged(false, previous_parent);
  }

  native_view->SetParent(new_parent);

  // And now, notify them that they have a brand new parent.
  for (Widget::Widgets::iterator it = widgets.begin();
      it != widgets.end(); ++it) {
    (*it)->NotifyNativeViewHierarchyChanged(true, new_parent);
  }
}

// static
bool NativeWidgetPrivate::IsMouseButtonDown() {
  return aura::Env::GetInstance()->is_mouse_button_down();
}

}  // namespace internal
}  // namespace views
