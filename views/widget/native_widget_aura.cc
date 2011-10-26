// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_aura.h"

#include "base/bind.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_types.h"
#include "ui/base/view_prop.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/font.h"
#include "ui/gfx/screen.h"
#include "views/widget/native_widget_delegate.h"

#if defined(OS_WIN)
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#endif

#if defined(HAVE_IBUS)
#include "views/ime/input_method_ibus.h"
#else
#include "views/ime/mock_input_method.h"
#endif

using ui::ViewProp;

namespace views {

const char* const NativeWidgetAura::kWindowTypeKey = "WindowType";

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, public:

NativeWidgetAura::NativeWidgetAura(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(window_(new aura::Window(this))),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)),
      can_activate_(true),
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
  SetNativeWindowProperty(kWindowTypeKey, reinterpret_cast<void*>(window_type));
  window_->SetType(window_type == Widget::InitParams::TYPE_CONTROL ?
                   aura::kWindowType_Control : aura::kWindowType_None);
  window_->Init();
  // TODO(beng): respect |params| authoritah wrt transparency.
  window_->layer()->SetFillsBoundsOpaquely(false);
  delegate_->OnNativeWidgetCreated();
  window_->set_minimum_size(delegate_->GetMinimumSize());
  window_->SetBounds(params.bounds);
  if (params.type == Widget::InitParams::TYPE_CONTROL) {
    window_->SetParent(params.GetParent());
  } else {
    window_->SetParent(NULL);
    gfx::NativeView parent = params.GetParent();
    if (parent)
      parent->AddTransientChild(window_);
  }
  // TODO(beng): do this some other way.
  delegate_->OnNativeWidgetSizeChanged(params.bounds.size());
  can_activate_ = params.can_activate;
}

NonClientFrameView* NativeWidgetAura::CreateNonClientFrameView() {
  NOTIMPLEMENTED();
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
  return window_->layer()->compositor();
}

ui::Compositor* NativeWidgetAura::GetCompositor() {
  return window_->layer()->compositor();
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
//  NOTIMPLEMENTED();
}

void NativeWidgetAura::SetNativeWindowProperty(const char* name, void* value) {
  // TODO(sky): push this to Widget when we get rid of NativeWidgetGtk.
  if (!window_)
    return;

  // Remove the existing property (if any).
  for (ViewProps::iterator i = props_.begin(); i != props_.end(); ++i) {
    if ((*i)->Key() == name) {
      props_.erase(i);
      break;
    }
  }

  if (value)
    props_.push_back(new ViewProp(window_, name, value));
}

void* NativeWidgetAura::GetNativeWindowProperty(const char* name) const {
  return window_ ? ViewProp::GetValue(window_, name) : NULL;
}

TooltipManager* NativeWidgetAura::GetTooltipManager() const {
  //NOTIMPLEMENTED();
  return NULL;
}

bool NativeWidgetAura::IsScreenReaderActive() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetAura::SendNativeAccessibilityEvent(
    View* view,
    ui::AccessibilityTypes::Event event_type) {
  NOTIMPLEMENTED();
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
#if defined(HAVE_IBUS)
  InputMethod* input_method = new InputMethodIBus(this);
#else
  InputMethod* input_method = new MockInputMethod(this);
#endif
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
    ui::WindowShowState* maximized) const {
  *maximized = window_->show_state();
}

void NativeWidgetAura::SetWindowTitle(const string16& title) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::SetWindowIcons(const SkBitmap& window_icon,
                                     const SkBitmap& app_icon) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::SetAccessibleName(const string16& name) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::SetAccessibleRole(ui::AccessibilityTypes::Role role) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::SetAccessibleState(ui::AccessibilityTypes::State state) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::BecomeModal() {
  NOTIMPLEMENTED();
}

gfx::Rect NativeWidgetAura::GetWindowScreenBounds() const {
  return window_->GetScreenBounds();
}

gfx::Rect NativeWidgetAura::GetClientAreaScreenBounds() const {
  // TODO(beng):
  NOTIMPLEMENTED();
  return window_->GetScreenBounds();
}

gfx::Rect NativeWidgetAura::GetRestoredBounds() const {
  // TODO(beng):
  NOTIMPLEMENTED();
  return window_->bounds();
}

void NativeWidgetAura::SetBounds(const gfx::Rect& bounds) {
  window_->SetBounds(bounds);
}

void NativeWidgetAura::SetSize(const gfx::Size& size) {
  window_->SetBounds(gfx::Rect(window_->bounds().origin(), size));
}

void NativeWidgetAura::SetBoundsConstrained(const gfx::Rect& bounds,
                                            Widget* other_widget) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::MoveAbove(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::MoveToTop() {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::SetShape(gfx::NativeRegion region) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::Close() {
  Hide();

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

void NativeWidgetAura::EnableClose(bool enable) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::Show() {
  window_->Show();
}

void NativeWidgetAura::Hide() {
  window_->Hide();
}

void NativeWidgetAura::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  window_->SetBounds(restored_bounds);
  window_->Maximize();
  window_->Show();
}

void NativeWidgetAura::ShowWithWindowState(ui::WindowShowState state) {
  switch(state) {
    case ui::SHOW_STATE_MAXIMIZED:
      window_->Maximize();
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      window_->Fullscreen();
      break;
    default:
      break;
  }
  window_->Show();
}

bool NativeWidgetAura::IsVisible() const {
  return window_->IsVisible();
}

void NativeWidgetAura::Activate() {
  window_->Activate();
}

void NativeWidgetAura::Deactivate() {
  window_->Deactivate();
}

bool NativeWidgetAura::IsActive() const {
  return aura::Desktop::GetInstance()->active_window() == window_;
}

void NativeWidgetAura::SetAlwaysOnTop(bool on_top) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::Maximize() {
  window_->Maximize();
}

void NativeWidgetAura::Minimize() {
  NOTIMPLEMENTED();
}

bool NativeWidgetAura::IsMaximized() const {
  return window_->show_state() == ui::SHOW_STATE_MAXIMIZED;
}

bool NativeWidgetAura::IsMinimized() const {
  return window_->show_state() == ui::SHOW_STATE_MINIMIZED;
}

void NativeWidgetAura::Restore() {
  window_->Restore();
}

void NativeWidgetAura::SetFullscreen(bool fullscreen) {
  fullscreen ? window_->Fullscreen() : window_->Restore();
}

bool NativeWidgetAura::IsFullscreen() const {
  return window_->show_state() == ui::SHOW_STATE_FULLSCREEN;
}

void NativeWidgetAura::SetOpacity(unsigned char opacity) {
  window_->layer()->SetOpacity(opacity / 255.0);
}

void NativeWidgetAura::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

bool NativeWidgetAura::IsAccessibleWidget() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetAura::RunShellDrag(View* view,
                                   const ui::OSExchangeData& data,
                                   int operation) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::SchedulePaintInRect(const gfx::Rect& rect) {
  if (window_)
    window_->SchedulePaintInRect(rect);
}

void NativeWidgetAura::SetCursor(gfx::NativeCursor cursor) {
  cursor_ = cursor;
  aura::Desktop::GetInstance()->SetCursor(cursor);
}

void NativeWidgetAura::ClearNativeFocus() {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::FocusNativeView(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

bool NativeWidgetAura::ConvertPointFromAncestor(const Widget* ancestor,
                                                gfx::Point* point) const {
  NOTIMPLEMENTED();
  return false;
}

gfx::Rect NativeWidgetAura::GetWorkAreaBoundsInScreen() const {
  return gfx::Screen::GetMonitorWorkAreaNearestWindow(GetNativeView());
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, views::InputMethodDelegate implementation:

void NativeWidgetAura::DispatchKeyEventPostIME(const KeyEvent& key) {
  delegate_->OnKeyEvent(key);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, aura::WindowDelegate implementation:

void NativeWidgetAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                       const gfx::Rect& new_bounds) {
  if (old_bounds.size() != new_bounds.size())
    delegate_->OnNativeWidgetSizeChanged(new_bounds.size());
}

void NativeWidgetAura::OnFocus() {
  Widget* widget = GetWidget();
  if (widget->is_top_level()) {
    InputMethod* input_method = widget->GetInputMethod();
    input_method->OnFocus();
    // See description of got_initial_focus_in_ for details on this.
    widget->GetFocusManager()->RestoreFocusedView();
  }
  delegate_->OnNativeFocus(window_);
}

void NativeWidgetAura::OnBlur() {
  Widget* widget = GetWidget();
  if (widget->is_top_level()) {
    InputMethod* input_method = widget->GetInputMethod();
    input_method->OnBlur();
    widget->GetFocusManager()->StoreFocusedView();
  }
  delegate_->OnNativeBlur(NULL);
}

bool NativeWidgetAura::OnKeyEvent(aura::KeyEvent* event) {
  // TODO(beng): Need an InputMethodAura to properly handle character events.
  //             Right now, we just skip these.
  if (event->is_char())
    return false;

  DCHECK(window_->IsVisible());
  InputMethod* input_method = GetWidget()->GetInputMethod();
  DCHECK(input_method);
  // TODO(oshima): DispatchKeyEvent should return bool?
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
  MouseEvent mouse_event(event);
  return delegate_->OnMouseEvent(mouse_event);
}

ui::TouchStatus NativeWidgetAura::OnTouchEvent(aura::TouchEvent* event) {
  DCHECK(window_->IsVisible());
  TouchEvent touch_event(event);
  return delegate_->OnTouchEvent(touch_event);
}

bool NativeWidgetAura::ShouldActivate(aura::Event* event) {
  return can_activate_;
}

void NativeWidgetAura::OnActivated() {
  delegate_->OnNativeWidgetActivationChanged(true);
  if (IsVisible() && GetWidget()->non_client_view())
    GetWidget()->non_client_view()->SchedulePaint();
}

void NativeWidgetAura::OnLostActive() {
  delegate_->OnNativeWidgetActivationChanged(false);
  if (IsVisible() && GetWidget()->non_client_view())
    GetWidget()->non_client_view()->SchedulePaint();
}

void NativeWidgetAura::OnCaptureLost() {
  delegate_->OnMouseCaptureLost();
}

void NativeWidgetAura::OnPaint(gfx::Canvas* canvas) {
  delegate_->OnNativeWidgetPaint(canvas);
}

void NativeWidgetAura::OnWindowDestroying() {
  delegate_->OnNativeWidgetDestroying();
}

void NativeWidgetAura::OnWindowDestroyed() {
  props_.reset();
  window_ = NULL;
  delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

void NativeWidgetAura::OnWindowVisibilityChanged(bool visible) {
  delegate_->OnNativeWidgetVisibilityChanged(visible);
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
void Widget::NotifyLocaleChanged() {
  NOTIMPLEMENTED();
}

// static
void Widget::CloseAllSecondaryWidgets() {
  NOTIMPLEMENTED();
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
  if (!native_view)
    return NULL;
  aura::Window* toplevel = native_view;
  aura::Window* parent = native_view->parent();
  while (parent) {
    if (parent->AsToplevelWindowContainer())
      return GetNativeWidgetForNativeView(toplevel);
    toplevel = parent;
    parent = parent->parent();
  }
  return NULL;
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
  NOTIMPLEMENTED();
}

// static
bool NativeWidgetPrivate::IsMouseButtonDown() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace internal
}  // namespace views
