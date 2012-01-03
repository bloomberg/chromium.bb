// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_aura.h"

#include "base/bind.h"
#include "base/string_util.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/font.h"
#include "ui/gfx/screen.h"
#include "ui/views/ime/input_method_bridge.h"
#include "ui/views/widget/drop_helper.h"
#include "ui/views/widget/native_widget_delegate.h"
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
    case Widget::InitParams::TYPE_WINDOW_FRAMELESS:
    case Widget::InitParams::TYPE_CONTROL:
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

typedef void (*WidgetCallback)(Widget*);

void ForEachNativeWidgetInternal(aura::Window* window,
                                 WidgetCallback callback) {
  Widget* widget = Widget::GetWidgetForNativeWindow(window);
  if (widget)
    callback(widget);

  const aura::Window::Windows& children = window->children();
  for (aura::Window::Windows::const_iterator it = children.begin();
       it != children.end(); ++it) {
    ForEachNativeWidgetInternal(*it, callback);
  }
}

void ForEachNativeWidget(WidgetCallback callback) {
  ForEachNativeWidgetInternal(aura::RootWindow::GetInstance(), callback);
}

void NotifyLocaleChangedCallback(Widget* widget) {
    widget->LocaleChanged();
}

void CloseAllSecondaryWidgetsCallback(Widget* widget) {
  if (widget->is_secondary_widget())
    widget->Close();
}

}  // namespace

// Used when SetInactiveRenderingDisabled() is invoked to track when active
// status changes in such a way that we should enable inactive rendering.
class NativeWidgetAura::ActiveWindowObserver : public aura::WindowObserver {
 public:
  explicit ActiveWindowObserver(NativeWidgetAura* host) : host_(host) {
    aura::RootWindow::GetInstance()->AddObserver(this);
  }
  virtual ~ActiveWindowObserver() {
    aura::RootWindow::GetInstance()->RemoveObserver(this);
  }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const char* key,
                                       void* old) OVERRIDE {
    if (key != aura::client::kRootWindowActiveWindow)
      return;
    aura::Window* active =
        aura::client::GetActivationClient()->GetActiveWindow();
    if (!active || (active != host_->window_ &&
                    active->transient_parent() != host_->window_)) {
      host_->delegate_->EnableInactiveRendering();
    }
  }

 private:
  NativeWidgetAura* host_;

  DISALLOW_COPY_AND_ASSIGN(ActiveWindowObserver);
};

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, public:

NativeWidgetAura::NativeWidgetAura(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(window_(new aura::Window(this))),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)),
      can_activate_(true),
#if defined(USE_X11)
      should_handle_menu_key_release_(false),
#endif
      cursor_(gfx::kNullCursor) {
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
  window_->set_user_data(this);
  Widget::InitParams::Type window_type =
      params.child ? Widget::InitParams::TYPE_CONTROL : params.type;
  window_->SetType(GetAuraWindowTypeForWidgetType(window_type));
  // TODO(jamescook): Should this use params.show_state instead?
  window_->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  window_->Init(params.create_texture_for_layer ?
                    ui::Layer::LAYER_HAS_TEXTURE :
                    ui::Layer::LAYER_HAS_NO_TEXTURE);
  if (window_type == Widget::InitParams::TYPE_CONTROL)
    window_->Show();

  window_->layer()->SetFillsBoundsOpaquely(!params.transparent);
  delegate_->OnNativeWidgetCreated();
  window_->SetBounds(params.bounds);
  if (window_type == Widget::InitParams::TYPE_CONTROL) {
    window_->SetParent(params.GetParent());
  } else {
    // Set up the transient child before the window is added. This way the
    // LayoutManager knows the window has a transient parent.
    gfx::NativeView parent = params.GetParent();
    if (parent)
      parent->AddTransientChild(window_);
    // SetAlwaysOnTop before SetParent so that always-on-top container is used.
    SetAlwaysOnTop(params.keep_on_top);
    window_->SetParent(NULL);
  }
  window_->set_ignore_events(!params.accept_events);
  // TODO(beng): do this some other way.
  delegate_->OnNativeWidgetSizeChanged(params.bounds.size());
  can_activate_ = params.can_activate;
  DCHECK(GetWidget()->GetRootView());
  if (params.type != Widget::InitParams::TYPE_TOOLTIP) {
    tooltip_manager_.reset(new views::TooltipManagerAura(this));
  }

  drop_helper_.reset(new DropHelper(GetWidget()->GetRootView()));
  if (params.type != Widget::InitParams::TYPE_TOOLTIP &&
      params.type != Widget::InitParams::TYPE_POPUP) {
    aura::client::SetDragDropDelegate(window_, this);
  }

  aura::client::SetActivationDelegate(window_, this);
}

NonClientFrameView* NativeWidgetAura::CreateNonClientFrameView() {
  return NULL;
}

void NativeWidgetAura::UpdateFrameAfterFrameChange() {
  // We don't support changing the frame type.
  NOTREACHED();
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

void NativeWidgetAura::ReorderLayers() {
}

void NativeWidgetAura::ViewRemoved(View* view) {
  DCHECK(drop_helper_.get() != NULL);
  drop_helper_->ResetTargetViewIfEquals(view);
}

void NativeWidgetAura::SetNativeWindowProperty(const char* name, void* value) {
  if (window_)
    window_->SetProperty(name, value);
}

void* NativeWidgetAura::GetNativeWindowProperty(const char* name) const {
  return window_ ? window_->GetProperty(name) : NULL;
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

void NativeWidgetAura::SetMouseCapture() {
  window_->SetCapture();
}

void NativeWidgetAura::ReleaseMouseCapture() {
  window_->ReleaseCapture();
}

bool NativeWidgetAura::HasMouseCapture() const {
  return window_->HasCapture();
}

InputMethod* NativeWidgetAura::CreateInputMethod() {
  aura::RootWindow* root_window = aura::RootWindow::GetInstance();
  ui::InputMethod* host = reinterpret_cast<ui::InputMethod*>(
      root_window->GetProperty(aura::client::kRootWindowInputMethod));
  InputMethod* input_method = new InputMethodBridge(this, host);
  input_method->Init(GetWidget());
  return input_method;
}

void NativeWidgetAura::CenterWindow(const gfx::Size& size) {
  const gfx::Rect parent_bounds = window_->parent()->bounds();
  window_->SetBounds(gfx::Rect((parent_bounds.width() - size.width())/2,
                               (parent_bounds.height() - size.height())/2,
                               size.width(),
                               size.height()));
}

void NativeWidgetAura::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  // The interface specifies returning restored bounds, not current bounds.
  *bounds = GetRestoredBounds();
  *show_state = static_cast<ui::WindowShowState>(
      window_->GetIntProperty(aura::client::kShowStateKey));
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

void NativeWidgetAura::BecomeModal() {
  window_->SetIntProperty(aura::client::kModalKey, 1);
}

gfx::Rect NativeWidgetAura::GetWindowScreenBounds() const {
  return window_->GetScreenBounds();
}

gfx::Rect NativeWidgetAura::GetClientAreaScreenBounds() const {
  // In Aura, the entire window is the client area.
  return window_->GetScreenBounds();
}

gfx::Rect NativeWidgetAura::GetRestoredBounds() const {
  gfx::Rect* restore_bounds = reinterpret_cast<gfx::Rect*>(
      window_->GetProperty(aura::client::kRestoreBoundsKey));
  return restore_bounds ? *restore_bounds : window_->bounds();
}

void NativeWidgetAura::SetBounds(const gfx::Rect& bounds) {
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

void NativeWidgetAura::SetShape(gfx::NativeRegion region) {
  // No need for this.
}

void NativeWidgetAura::Close() {
  // |window_| may already be deleted by parent window. This can happen
  // when this widget is child widget or has transient parent
  // and ownership is WIDGET_OWNS_NATIVE_WIDGET.
  DCHECK(window_ ||
         ownership_ == Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  if (window_) {
    Hide();
    window_->SetIntProperty(aura::client::kModalKey, 0);
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
  window_->SetBounds(restored_bounds);
  ShowWithWindowState(ui::SHOW_STATE_MAXIMIZED);
}

void NativeWidgetAura::ShowWithWindowState(ui::WindowShowState state) {
  if (state == ui::SHOW_STATE_MAXIMIZED ||
      state == ui::SHOW_STATE_FULLSCREEN) {
    window_->SetIntProperty(aura::client::kShowStateKey, state);
  }
  window_->Show();
  if (can_activate_ && (state != ui::SHOW_STATE_INACTIVE ||
                        !GetWidget()->SetInitialFocus())) {
    Activate();
  }
}

bool NativeWidgetAura::IsVisible() const {
  return window_->IsVisible();
}

void NativeWidgetAura::Activate() {
  aura::client::GetActivationClient()->ActivateWindow(window_);
}

void NativeWidgetAura::Deactivate() {
  aura::client::GetActivationClient()->DeactivateWindow(window_);
}

bool NativeWidgetAura::IsActive() const {
  return aura::client::GetActivationClient()->GetActiveWindow() == window_;
}

void NativeWidgetAura::SetAlwaysOnTop(bool on_top) {
  window_->SetIntProperty(aura::client::kAlwaysOnTopKey, on_top);
}

void NativeWidgetAura::Maximize() {
  window_->SetIntProperty(aura::client::kShowStateKey,
                          ui::SHOW_STATE_MAXIMIZED);
}

void NativeWidgetAura::Minimize() {
  // No minimized window for aura. crbug.com/104571.
  NOTREACHED();
}

bool NativeWidgetAura::IsMaximized() const {
  return window_->GetIntProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MAXIMIZED;
}

bool NativeWidgetAura::IsMinimized() const {
  return window_->GetIntProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MINIMIZED;
}

void NativeWidgetAura::Restore() {
  window_->SetIntProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
}

void NativeWidgetAura::SetFullscreen(bool fullscreen) {
  window_->SetIntProperty(
      aura::client::kShowStateKey,
      fullscreen ? ui::SHOW_STATE_FULLSCREEN : ui::SHOW_STATE_NORMAL);
}

bool NativeWidgetAura::IsFullscreen() const {
  return window_->GetIntProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_FULLSCREEN;
}

void NativeWidgetAura::SetOpacity(unsigned char opacity) {
  window_->layer()->SetOpacity(opacity / 255.0);
}

void NativeWidgetAura::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

bool NativeWidgetAura::IsAccessibleWidget() const {
  // http://crbug.com/102570
  //NOTIMPLEMENTED();
  return false;
}

void NativeWidgetAura::RunShellDrag(View* view,
                                   const ui::OSExchangeData& data,
                                   int operation) {
  if (aura::client::GetDragDropClient())
    aura::client::GetDragDropClient()->StartDragAndDrop(data, operation);
}

void NativeWidgetAura::SchedulePaintInRect(const gfx::Rect& rect) {
  if (window_)
    window_->SchedulePaintInRect(rect);
}

void NativeWidgetAura::SetCursor(gfx::NativeCursor cursor) {
  cursor_ = cursor;
  aura::RootWindow::GetInstance()->SetCursor(cursor);
}

void NativeWidgetAura::ClearNativeFocus() {
  if (window_ && window_->GetFocusManager() &&
      window_->Contains(window_->GetFocusManager()->GetFocusedWindow())) {
    window_->GetFocusManager()->SetFocusedWindow(window_);
  }
}

void NativeWidgetAura::FocusNativeView(gfx::NativeView native_view) {
  window_->GetFocusManager()->SetFocusedWindow(native_view);
}

gfx::Rect NativeWidgetAura::GetWorkAreaBoundsInScreen() const {
  return gfx::Screen::GetMonitorWorkAreaNearestWindow(GetNativeView());
}

void NativeWidgetAura::SetInactiveRenderingDisabled(bool value) {
  if (!value)
    active_window_observer_.reset();
  else
    active_window_observer_.reset(new ActiveWindowObserver(this));
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, views::InputMethodDelegate implementation:

void NativeWidgetAura::DispatchKeyEventPostIME(const KeyEvent& key) {
  if (delegate_->OnKeyEvent(key) || !GetWidget()->GetFocusManager())
    return;

#if defined(USE_X11)
  // TODO(oshima): This is copied from native_widget_gtk for now.
  // RenderWidgetHostViewAura doesn't work and needs more work.
  // oshima thinks this should be moved to focus manager (see
  // crbug.com/106998), but beng believes that this should be done
  // in RootWindowHosLinux for aura/linux.
  const int key_code = key.key_code();

  // Always reset |should_handle_menu_key_release_| unless we are handling a
  // VKEY_MENU key release event. It ensures that VKEY_MENU accelerator can only
  // be activated when handling a VKEY_MENU key release event which is preceded
  // by an un-handled VKEY_MENU key press event.
  if (key_code != ui::VKEY_MENU || key.type() != ui::ET_KEY_RELEASED)
    should_handle_menu_key_release_ = false;

  if (key.type() == ui::ET_KEY_PRESSED) {
    // VKEY_MENU is triggered by key release event.
    // FocusManager::OnKeyEvent() returns false when the key has been consumed.
    if (key_code != ui::VKEY_MENU)
      GetWidget()->GetFocusManager()->OnKeyEvent(key);
    else
      should_handle_menu_key_release_ = true;
  } else if (key_code == ui::VKEY_MENU && should_handle_menu_key_release_ &&
             (key.flags() & ~ui::EF_ALT_DOWN) == 0) {
    // Trigger VKEY_MENU when only this key is pressed and released, and both
    // press and release events are not handled by others.
    ui::Accelerator accelerator(ui::VKEY_MENU, false, false, false);
    GetWidget()->GetFocusManager()->ProcessAccelerator(accelerator);
  }
#else
  if (key.type() == ui::ET_KEY_PRESSED)
    GetWidget()->GetFocusManager()->OnKeyEvent(key);
#endif
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, aura::WindowDelegate implementation:

gfx::Size NativeWidgetAura::GetMinimumSize() const {
  return delegate_->GetMinimumSize();
}

void NativeWidgetAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                       const gfx::Rect& new_bounds) {
  if (old_bounds.origin() != new_bounds.origin())
    GetWidget()->widget_delegate()->OnWidgetMove();
  if (old_bounds.size() != new_bounds.size())
    delegate_->OnNativeWidgetSizeChanged(new_bounds.size());
}

void NativeWidgetAura::OnFocus() {
#if defined(USE_X11)
  should_handle_menu_key_release_ = false;
#endif
  Widget* widget = GetWidget();
  if (widget->is_top_level()) {
    InputMethod* input_method = widget->GetInputMethod();
    input_method->OnFocus();
  }
  delegate_->OnNativeFocus(window_);
}

void NativeWidgetAura::OnBlur() {
  Widget* widget = GetWidget();
  if (widget->is_top_level()) {
    InputMethod* input_method = widget->GetInputMethod();
    input_method->OnBlur();
  }
  delegate_->OnNativeBlur(NULL);
}

bool NativeWidgetAura::OnKeyEvent(aura::KeyEvent* event) {
  if (event->is_char()) {
    // If a ui::InputMethod object is attched to the root window, character
    // events are handled inside the object and are not passed to this function.
    // If such object is not attached, character events might be sent (e.g. on
    // Windows). In this case, we just skip these.
    return false;
  }

  DCHECK(window_->IsVisible());
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
    return delegate_->OnMouseEvent(scroll_event);
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

bool NativeWidgetAura::ShouldActivate(aura::Event* event) {
  return can_activate_;
}

void NativeWidgetAura::OnActivated() {
  GetWidget()->GetFocusManager()->RestoreFocusedView();
  delegate_->OnNativeWidgetActivationChanged(true);
  if (IsVisible() && GetWidget()->non_client_view())
    GetWidget()->non_client_view()->SchedulePaint();
}

void NativeWidgetAura::OnLostActive() {
  GetWidget()->GetFocusManager()->StoreFocusedView();
  delegate_->OnNativeWidgetActivationChanged(false);
  if (IsVisible() && GetWidget()->non_client_view())
    GetWidget()->non_client_view()->SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, aura::WindowDragDropDelegate implementation:

void NativeWidgetAura::OnDragEntered(const aura::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != NULL);
  drop_helper_->OnDragOver(event.data(), event.location(),
      event.source_operations());
}

int NativeWidgetAura::OnDragUpdated(const aura::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != NULL);
  return drop_helper_->OnDragOver(event.data(), event.location(),
      event.source_operations());
}

void NativeWidgetAura::OnDragExited() {
  DCHECK(drop_helper_.get() != NULL);
  drop_helper_->OnDragExit();
}

int NativeWidgetAura::OnPerformDrop(const aura::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != NULL);
  return drop_helper_->OnDrop(event.data(), event.location(),
      event.source_operations());
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
void Widget::NotifyLocaleChanged() {
  ForEachNativeWidget(NotifyLocaleChangedCallback);
}

// static
void Widget::CloseAllSecondaryWidgets() {
  ForEachNativeWidget(CloseAllSecondaryWidgetsCallback);
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
  return aura::RootWindow::GetInstance()->IsMouseButtonDown();
}

}  // namespace internal
}  // namespace views
