// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_aura.h"

#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/font.h"
#include "views/widget/native_widget_delegate.h"

#if defined(OS_WIN)
#include "base/win/scoped_gdi_object.h"
#include "base/win/win_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#endif

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, public:

NativeWidgetAura::NativeWidgetAura(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(window_(new aura::Window(this))),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)) {
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
  window_->Init();
  delegate_->OnNativeWidgetCreated();
  window_->SetBounds(params.bounds, 0);
  window_->SetParent(params.parent);
  // TODO(beng): do this some other way.
  delegate_->OnNativeWidgetSizeChanged(params.bounds.size());
  window_->SetVisibility(aura::Window::VISIBILITY_SHOWN);
}

NonClientFrameView* NativeWidgetAura::CreateNonClientFrameView() {
  NOTIMPLEMENTED();
  return NULL;
}

void NativeWidgetAura::UpdateFrameAfterFrameChange() {
  NOTIMPLEMENTED();
}

bool NativeWidgetAura::ShouldUseNativeFrame() const {
  // There is only one frame type for aura.
  return false;
}

void NativeWidgetAura::FrameTypeChanged() {
  NOTIMPLEMENTED();
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

void NativeWidgetAura::ViewRemoved(View* view) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::SetNativeWindowProperty(const char* name, void* value) {
  NOTIMPLEMENTED();
}

void* NativeWidgetAura::GetNativeWindowProperty(const char* name) const {
  NOTIMPLEMENTED();
  return NULL;
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
  NOTIMPLEMENTED();
}

void NativeWidgetAura::ReleaseMouseCapture() {
  NOTIMPLEMENTED();
}

bool NativeWidgetAura::HasMouseCapture() const {
  //NOTIMPLEMENTED();
  return false;
}

InputMethod* NativeWidgetAura::CreateInputMethod() {
  NOTIMPLEMENTED();
  return NULL;
}

void NativeWidgetAura::CenterWindow(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* maximized) const {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::SetWindowTitle(const std::wstring& title) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::SetWindowIcons(const SkBitmap& window_icon,
                                     const SkBitmap& app_icon) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::SetAccessibleName(const std::wstring& name) {
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
  // TODO(beng): ensure screen bounds
  return window_->bounds();
}

gfx::Rect NativeWidgetAura::GetClientAreaScreenBounds() const {
  // TODO(beng):
  return window_->bounds();
}

gfx::Rect NativeWidgetAura::GetRestoredBounds() const {
  // TODO(beng):
  return window_->bounds();
}

void NativeWidgetAura::SetBounds(const gfx::Rect& bounds) {
  window_->SetBounds(bounds, 0);
}

void NativeWidgetAura::SetSize(const gfx::Size& size) {
  window_->SetBounds(gfx::Rect(window_->bounds().origin(), size), 0);
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

  if (close_widget_factory_.empty()) {
    MessageLoop::current()->PostTask(FROM_HERE,
        close_widget_factory_.NewRunnableMethod(
            &NativeWidgetAura::CloseNow));
  }
}

void NativeWidgetAura::CloseNow() {
  delete window_;
}

void NativeWidgetAura::EnableClose(bool enable) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::Show() {
  window_->SetVisibility(aura::Window::VISIBILITY_SHOWN);
}

void NativeWidgetAura::Hide() {
  window_->SetVisibility(aura::Window::VISIBILITY_HIDDEN);
}

void NativeWidgetAura::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::ShowWithWindowState(ui::WindowShowState state) {
  window_->SetVisibility(aura::Window::VISIBILITY_SHOWN);
  NOTIMPLEMENTED();
}

bool NativeWidgetAura::IsVisible() const {
  return window_->visibility() != aura::Window::VISIBILITY_HIDDEN;
}

void NativeWidgetAura::Activate() {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::Deactivate() {
  NOTIMPLEMENTED();
}

bool NativeWidgetAura::IsActive() const {
  //NOTIMPLEMENTED();
  return false;
}

void NativeWidgetAura::SetAlwaysOnTop(bool on_top) {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::Maximize() {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::Minimize() {
  NOTIMPLEMENTED();
}

bool NativeWidgetAura::IsMaximized() const {
  //NOTIMPLEMENTED();
  return false;
}

bool NativeWidgetAura::IsMinimized() const {
  //NOTIMPLEMENTED();
  return false;
}

void NativeWidgetAura::Restore() {
  NOTIMPLEMENTED();
}

void NativeWidgetAura::SetFullscreen(bool fullscreen) {
  NOTIMPLEMENTED();
}

bool NativeWidgetAura::IsFullscreen() const {
  //NOTIMPLEMENTED();
  return false;
}

void NativeWidgetAura::SetOpacity(unsigned char opacity) {
  NOTIMPLEMENTED();
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
  //NOTIMPLEMENTED();
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

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, views::InputMethodDelegate implementation:

void NativeWidgetAura::DispatchKeyEventPostIME(const KeyEvent& key) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetAura, aura::WindowDelegate implementation:

void NativeWidgetAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                       const gfx::Rect& new_bounds) {
  if (old_bounds.size() != new_bounds.size())
    delegate_->OnNativeWidgetSizeChanged(new_bounds.size());
}

void NativeWidgetAura::OnFocus() {
  delegate_->OnNativeFocus(window_);
}

void NativeWidgetAura::OnBlur() {
  delegate_->OnNativeBlur(NULL);
}

bool NativeWidgetAura::OnKeyEvent(aura::KeyEvent* event) {
  return delegate_->OnKeyEvent(KeyEvent(event));
}

int NativeWidgetAura::GetNonClientComponent(const gfx::Point& point) const {
  return delegate_->GetNonClientComponent(point);
}

bool NativeWidgetAura::OnMouseEvent(aura::MouseEvent* event) {
  return delegate_->OnMouseEvent(MouseEvent(event));
}

void NativeWidgetAura::OnPaint(gfx::Canvas* canvas) {
  delegate_->OnNativeWidgetPaint(canvas);
}

void NativeWidgetAura::OnWindowDestroying() {
  delegate_->OnNativeWidgetDestroying();
}

void NativeWidgetAura::OnWindowDestroyed() {
  window_ = NULL;
  delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
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
    if (parent->IsTopLevelWindowContainer())
      return GetNativeWidgetForNativeView(toplevel);
    toplevel = parent;
    parent = parent->parent();
  }
  return NULL;
}

// static
void NativeWidgetPrivate::GetAllChildWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* children) {
  NOTIMPLEMENTED();
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
