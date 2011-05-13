// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "ui/gfx/compositor/compositor.h"
#include "views/focus/view_storage.h"
#include "views/ime/input_method.h"
#include "views/views_delegate.h"
#include "views/widget/default_theme_provider.h"
#include "views/widget/root_view.h"
#include "views/widget/native_widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Widget, InitParams:

Widget::InitParams::InitParams()
    : type(TYPE_WINDOW),
      child(false),
      transient(false),
      transparent(false),
      accept_events(true),
      can_activate(true),
      keep_on_top(false),
      delete_on_destroy(true),
      mirror_origin_in_rtl(false),
      has_dropshadow(false),
      double_buffer(false),
      parent(NULL),
      parent_widget(NULL),
      native_widget(NULL) {
}

Widget::InitParams::InitParams(Type type)
    : type(type),
      child(type == TYPE_CONTROL),
      transient(type == TYPE_POPUP || type == TYPE_MENU),
      transparent(false),
      accept_events(true),
      can_activate(type != TYPE_POPUP && type != TYPE_MENU),
      keep_on_top(type == TYPE_MENU),
      delete_on_destroy(true),
      mirror_origin_in_rtl(false),
      has_dropshadow(false),
      double_buffer(false),
      parent(NULL),
      parent_widget(NULL),
      native_widget(NULL) {
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

// static
Widget::InitParams Widget::WindowInitParams() {
  return InitParams(InitParams::TYPE_WINDOW);
}

Widget::Widget()
    : is_mouse_button_pressed_(false),
      last_mouse_event_was_move_(false),
      native_widget_(NULL),
      widget_delegate_(NULL),
      dragged_view_(NULL),
      delete_on_destroy_(false),
      is_secondary_widget_(true) {
}

Widget::~Widget() {
  DestroyRootView();

  if (!delete_on_destroy_)
    delete native_widget_;
}

void Widget::Init(const InitParams& params) {
  delete_on_destroy_ = params.delete_on_destroy;
  native_widget_ =
      params.native_widget ? params.native_widget
                           : NativeWidget::CreateNativeWidget(this);
  GetRootView();
  default_theme_provider_.reset(new DefaultThemeProvider);
  if (params.type == InitParams::TYPE_MENU)
    is_mouse_button_pressed_ = native_widget_->IsMouseButtonDown();
  native_widget_->InitNativeWidget(params);
}

// Unconverted methods (see header) --------------------------------------------

gfx::NativeView Widget::GetNativeView() const {
   return native_widget_->GetNativeView();
}

gfx::NativeWindow Widget::GetNativeWindow() const {
  return native_widget_->GetNativeWindow();
}

bool Widget::GetAccelerator(int cmd_id, ui::Accelerator* accelerator) {
  return false;
}

Window* Widget::GetContainingWindow() {
  return native_widget_->GetContainingWindow();
}

const Window* Widget::GetContainingWindow() const {
  return native_widget_->GetContainingWindow();
}

void Widget::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (!is_add) {
    if (child == dragged_view_)
      dragged_view_ = NULL;

    FocusManager* focus_manager = GetFocusManager();
    if (focus_manager)
      focus_manager->ViewRemoved(child);
    ViewStorage::GetInstance()->ViewRemoved(child);
    native_widget_->ViewRemoved(child);
  }
}

void Widget::NotifyNativeViewHierarchyChanged(bool attached,
                                              gfx::NativeView native_view) {
  if (!attached) {
    FocusManager* focus_manager = GetFocusManager();
    // We are being removed from a window hierarchy.  Treat this as
    // the root_view_ being removed.
    if (focus_manager)
      focus_manager->ViewRemoved(root_view_.get());
  }
  root_view_->NotifyNativeViewHierarchyChanged(attached, native_view);
}

// Converted methods (see header) ----------------------------------------------

Widget* Widget::GetTopLevelWidget() {
  return const_cast<Widget*>(
      const_cast<const Widget*>(this)->GetTopLevelWidget());
}

const Widget* Widget::GetTopLevelWidget() const {
  NativeWidget* native_widget =
      NativeWidget::GetTopLevelNativeWidget(GetNativeView());
  return native_widget ? native_widget->GetWidget() : NULL;
}

void Widget::SetContentsView(View* view) {
  root_view_->SetContentsView(view);
}

gfx::Rect Widget::GetWindowScreenBounds() const {
  return native_widget_->GetWindowScreenBounds();
}

gfx::Rect Widget::GetClientAreaScreenBounds() const {
  return native_widget_->GetClientAreaScreenBounds();
}

void Widget::SetBounds(const gfx::Rect& bounds) {
  native_widget_->SetBounds(bounds);
}

void Widget::SetSize(const gfx::Size& size) {
  native_widget_->SetSize(size);
}

void Widget::MoveAboveWidget(Widget* widget) {
  native_widget_->MoveAbove(widget->GetNativeView());
}

void Widget::MoveAbove(gfx::NativeView native_view) {
  native_widget_->MoveAbove(native_view);
}

void Widget::SetShape(gfx::NativeRegion shape) {
  native_widget_->SetShape(shape);
}

void Widget::Close() {
  native_widget_->Close();
}

void Widget::CloseNow() {
  native_widget_->CloseNow();
}

void Widget::Show() {
  native_widget_->Show();
}

void Widget::Hide() {
  native_widget_->Hide();
}

void Widget::SetOpacity(unsigned char opacity) {
  native_widget_->SetOpacity(opacity);
}

void Widget::SetAlwaysOnTop(bool on_top) {
  native_widget_->SetAlwaysOnTop(on_top);
}

RootView* Widget::GetRootView() {
  if (!root_view_.get()) {
    // First time the root view is being asked for, create it now.
    root_view_.reset(CreateRootView());
  }
  return root_view_.get();
}

bool Widget::IsVisible() const {
  return native_widget_->IsVisible();
}

bool Widget::IsActive() const {
  return native_widget_->IsActive();
}

bool Widget::IsAccessibleWidget() const {
  return native_widget_->IsAccessibleWidget();
}

ThemeProvider* Widget::GetThemeProvider() const {
  const Widget* root_widget = GetTopLevelWidget();
  if (root_widget && root_widget != this) {
    // Attempt to get the theme provider, and fall back to the default theme
    // provider if not found.
    ThemeProvider* provider = root_widget->GetThemeProvider();
    if (provider)
      return provider;

    provider = root_widget->default_theme_provider_.get();
    if (provider)
      return provider;
  }
  return default_theme_provider_.get();
}

FocusManager* Widget::GetFocusManager() {
  Widget* toplevel_widget = GetTopLevelWidget();
  if (toplevel_widget && toplevel_widget != this)
    return toplevel_widget->focus_manager_.get();

  return focus_manager_.get();
}

InputMethod* Widget::GetInputMethod() {
  Widget* toplevel_widget = GetTopLevelWidget();
  return toplevel_widget ?
      toplevel_widget->native_widget()->GetInputMethodNative() : NULL;
}

bool Widget::ContainsNativeView(gfx::NativeView native_view) {
  if (native_widget_->ContainsNativeView(native_view))
    return true;

  // A views::NativeViewHost may contain the given native view, without it being
  // an ancestor of hwnd(), so traverse the views::View hierarchy looking for
  // such views.
  return GetRootView()->ContainsNativeView(native_view);
}

void Widget::RunShellDrag(View* view, const ui::OSExchangeData& data,
                          int operation) {
  dragged_view_ = view;
  native_widget_->RunShellDrag(view, data, operation);
  // If the view is removed during the drag operation, dragged_view_ is set to
  // NULL.
  if (view && dragged_view_ == view) {
    dragged_view_ = NULL;
    view->OnDragDone();
  }
}

void Widget::SchedulePaintInRect(const gfx::Rect& rect) {
  native_widget_->SchedulePaintInRect(rect);
}

void Widget::SetCursor(gfx::NativeCursor cursor) {
  native_widget_->SetCursor(cursor);
}

void Widget::ResetLastMouseMoveFlag() {
  last_mouse_event_was_move_ = false;
}

FocusTraversable* Widget::GetFocusTraversable() {
  return root_view_.get();
}

void Widget::ThemeChanged() {
  root_view_->ThemeChanged();
}

void Widget::LocaleChanged() {
  root_view_->LocaleChanged();
}

void Widget::SetFocusTraversableParent(FocusTraversable* parent) {
  root_view_->SetFocusTraversableParent(parent);
}

void Widget::SetFocusTraversableParentView(View* parent_view) {
  root_view_->SetFocusTraversableParentView(parent_view);
}

void Widget::NotifyAccessibilityEvent(
    View* view,
    ui::AccessibilityTypes::Event event_type,
    bool send_native_event) {
  // Send the notification to the delegate.
  if (ViewsDelegate::views_delegate)
    ViewsDelegate::views_delegate->NotifyAccessibilityEvent(view, event_type);

  if (send_native_event)
    native_widget_->SendNativeAccessibilityEvent(view, event_type);
}

////////////////////////////////////////////////////////////////////////////////
// Widget, NativeWidgetDelegate implementation:

void Widget::OnNativeFocus(gfx::NativeView focused_view) {
  GetFocusManager()->GetWidgetFocusManager()->OnWidgetFocusEvent(
      focused_view,
      GetNativeView());
}

void Widget::OnNativeBlur(gfx::NativeView focused_view) {
  GetFocusManager()->GetWidgetFocusManager()->OnWidgetFocusEvent(
      GetNativeView(),
      focused_view);
}

void Widget::OnNativeWidgetCreated() {
  if (GetTopLevelWidget() == this) {
    // Only the top level Widget in a native widget hierarchy has a focus
    // manager.
    focus_manager_.reset(new FocusManager(this));
  }
  EnsureCompositor();
}

void Widget::OnSizeChanged(const gfx::Size& new_size) {
  root_view_->SetSize(new_size);
}

bool Widget::HasFocusManager() const {
  return !!focus_manager_.get();
}

bool Widget::OnNativeWidgetPaintAccelerated(const gfx::Rect& dirty_region) {
#if !defined(COMPOSITOR_2)
  return false;
#else
  if (!compositor_.get())
    return false;

  compositor_->NotifyStart();
  GetRootView()->PaintToTexture(dirty_region);
  GetRootView()->PaintComposite();
  compositor_->NotifyEnd();
  return true;
#endif
}

void Widget::OnNativeWidgetPaint(gfx::Canvas* canvas) {
  GetRootView()->Paint(canvas);
#if !defined(COMPOSITOR_2)
  if (compositor_.get()) {
    compositor_->NotifyStart();
    root_view_->PaintComposite(compositor_.get());
    compositor_->NotifyEnd();
  }
#endif
}

bool Widget::OnKeyEvent(const KeyEvent& event) {
  return GetRootView()->OnKeyEvent(event);
}

bool Widget::OnMouseEvent(const MouseEvent& event) {
  switch (event.type()) {
    case ui::ET_MOUSE_PRESSED:
      last_mouse_event_was_move_ = false;
      if (GetRootView()->OnMousePressed(event)) {
        is_mouse_button_pressed_ = true;
        if (!native_widget_->HasMouseCapture())
          native_widget_->SetMouseCapture();
        return true;
      }
      return false;
    case ui::ET_MOUSE_RELEASED:
      last_mouse_event_was_move_ = false;
      is_mouse_button_pressed_ = false;
      // Release capture first, to avoid confusion if OnMouseReleased blocks.
      if (native_widget_->HasMouseCapture() &&
          ShouldReleaseCaptureOnMouseReleased()) {
        native_widget_->ReleaseMouseCapture();
      }
      GetRootView()->OnMouseReleased(event);
      return (event.flags() & ui::EF_IS_NON_CLIENT) ? false : true;
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      if (native_widget_->HasMouseCapture() && is_mouse_button_pressed_) {
        last_mouse_event_was_move_ = false;
        GetRootView()->OnMouseDragged(event);
      } else if (!last_mouse_event_was_move_ ||
                 last_mouse_event_position_ != event.location()) {
        last_mouse_event_position_ = event.location();
        last_mouse_event_was_move_ = true;
        GetRootView()->OnMouseMoved(event);
      }
      return false;
    case ui::ET_MOUSE_EXITED:
      last_mouse_event_was_move_ = false;
      GetRootView()->OnMouseExited(event);
      return false;
    case ui::ET_MOUSEWHEEL:
      return GetRootView()->OnMouseWheel(
          reinterpret_cast<const MouseWheelEvent&>(event));
    default:
      return false;
  }
  return true;
}

void Widget::OnMouseCaptureLost() {
  if (is_mouse_button_pressed_)
    GetRootView()->OnMouseCaptureLost();
  is_mouse_button_pressed_ = false;
}

Widget* Widget::AsWidget() {
  return this;
}

const Widget* Widget::AsWidget() const {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// Widget, FocusTraversable implementation:

FocusSearch* Widget::GetFocusSearch() {
  return root_view_->GetFocusSearch();
}

FocusTraversable* Widget::GetFocusTraversableParent() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

View* Widget::GetFocusTraversableParentView() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Widget, protected:

RootView* Widget::CreateRootView() {
  return new RootView(this);
}

void Widget::DestroyRootView() {
  root_view_.reset();

  // Defer focus manager's destruction. This is for the case when the
  // focus manager is referenced by a child WidgetGtk (e.g. TabbedPane in a
  // dialog). When gtk_widget_destroy is called on the parent, the destroy
  // signal reaches parent first and then the child. Thus causing the parent
  // WidgetGtk's dtor executed before the child's. If child's view hierarchy
  // references this focus manager, it crashes. This will defer focus manager's
  // destruction after child WidgetGtk's dtor.
  FocusManager* focus_manager = focus_manager_.release();
  if (focus_manager)
    MessageLoop::current()->DeleteSoon(FROM_HERE, focus_manager);
}

void Widget::ReplaceFocusManager(FocusManager* focus_manager) {
  focus_manager_.reset(focus_manager);
}

////////////////////////////////////////////////////////////////////////////////
// Widget, private:

void Widget::EnsureCompositor() {
  DCHECK(!compositor_.get());

  // TODO(sad): If there is a parent Widget, then use the same compositor
  //            instead of creating a new one here.
  gfx::AcceleratedWidget widget = native_widget_->GetAcceleratedWidget();
  if (widget != gfx::kNullAcceleratedWidget)
    compositor_ = ui::Compositor::Create(widget);
}

bool Widget::ShouldReleaseCaptureOnMouseReleased() const {
  return true;
}

}  // namespace views
