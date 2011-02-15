// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget_impl.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "views/focus/focus_manager.h"
#include "views/view.h"
#include "views/widget/native_widget.h"
#include "views/widget/root_view.h"

namespace views {

namespace {

// TODO(beng): move to platform file
int GetHorizontalDragThreshold() {
  static int threshold = -1;
#if defined(OS_WIN)
  if (threshold == -1)
    threshold = GetSystemMetrics(SM_CXDRAG) / 2;
#endif
  return threshold;
}

// TODO(beng): move to platform file
int GetVerticalDragThreshold() {
  static int threshold = -1;
#if defined(OS_WIN)
  if (threshold == -1)
    threshold = GetSystemMetrics(SM_CYDRAG) / 2;
#endif
  return threshold;
}

bool ExceededDragThreshold(int delta_x, int delta_y) {
  return (abs(delta_x) > GetHorizontalDragThreshold() ||
          abs(delta_y) > GetVerticalDragThreshold());
}

}

////////////////////////////////////////////////////////////////////////////////
// WidgetImpl, public:

WidgetImpl::WidgetImpl(View* contents_view)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          native_widget_(NativeWidget::CreateNativeWidget(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(root_view_(new RootView(this))),
      contents_view_(contents_view),
      is_mouse_button_pressed_(false),
      last_mouse_event_was_move_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)),
      delete_on_destroy_(true) {
}

WidgetImpl::~WidgetImpl() {
}

void WidgetImpl::InitWithNativeViewParent(gfx::NativeView parent,
                                          const gfx::Rect& bounds) {
  native_widget_->InitWithNativeViewParent(parent, bounds);
}

WidgetImpl* WidgetImpl::GetTopLevelWidgetImpl() const {
  NativeWidget* native_widget =
      NativeWidget::GetTopLevelNativeWidget(native_widget_->GetNativeView());
  return native_widget->GetWidgetImpl();
}

gfx::Rect WidgetImpl::GetWindowScreenBounds() const {
  return native_widget_->GetWindowScreenBounds();
}

gfx::Rect WidgetImpl::GetClientAreaScreenBounds() const {
  return native_widget_->GetClientAreaScreenBounds();
}

void WidgetImpl::SetBounds(const gfx::Rect& bounds) {
  native_widget_->SetBounds(bounds);
}

void WidgetImpl::SetShape(const gfx::Path& shape) {
  native_widget_->SetShape(shape);
}

void WidgetImpl::Show() {
  native_widget_->Show();
}

void WidgetImpl::Hide() {
  native_widget_->Hide();
}

void WidgetImpl::Close() {
  native_widget_->Hide();

  if (close_widget_factory_.empty()) {
    MessageLoop::current()->PostTask(FROM_HERE,
      close_widget_factory_.NewRunnableMethod(&WidgetImpl::CloseNow));
  }
}

void WidgetImpl::MoveAbove(WidgetImpl* other) {
  native_widget_->MoveAbove(other->native_widget());
}

void WidgetImpl::SetAlwaysOnTop(bool always_on_top) {
  NOTIMPLEMENTED();
}

void WidgetImpl::InvalidateRect(const gfx::Rect& invalid_rect) {
  native_widget_->InvalidateRect(invalid_rect);
}

ThemeProvider* WidgetImpl::GetThemeProvider() const {
  return NULL;
}

FocusManager* WidgetImpl::GetFocusManager() {
  return GetTopLevelWidgetImpl()->focus_manager_.get();
}

FocusTraversable* WidgetImpl::GetFocusTraversable() const {
  return root_view_.get();
}

////////////////////////////////////////////////////////////////////////////////
// WidgetImpl, NativeWidgetListener implementation:

void WidgetImpl::OnClose() {
  Close();
}

void WidgetImpl::OnDestroy() {
  if (delete_on_destroy_)
    delete this;
}

void WidgetImpl::OnDisplayChanged() {
  // TODO(beng):
}

bool WidgetImpl::OnKeyEvent(const KeyEvent& event) {
  // find root view.

  //return root_view_->OnKeyEvent(event);
  return true;
}

void WidgetImpl::OnMouseCaptureLost() {
  if (native_widget_->HasMouseCapture()) {
    if (is_mouse_button_pressed_) {
      // TODO(beng): Rename to OnMouseCaptureLost();
      root_view_->ProcessMouseDragCanceled();
    }
    is_mouse_button_pressed_ = false;
  }
}

bool WidgetImpl::OnMouseEvent(const MouseEvent& event) {
  last_mouse_event_was_move_ = false;
  switch (event.type()) {
    case ui::ET_MOUSE_PRESSED:
      if (root_view_->OnMousePressed(event)) {
        is_mouse_button_pressed_ = true;
        if (!native_widget_->HasMouseCapture())
          native_widget_->SetMouseCapture();
        return true;
      }
      return false;
    case ui::ET_MOUSE_RELEASED:
      // TODO(beng): NativeWidgetGtk should not call this function if drag data
      //             exists, see comment in this function in WidgetGtk.
      // Release the capture first, that way we don't get confused if
      // OnMouseReleased blocks.
      if (native_widget_->HasMouseCapture() &&
          native_widget_->ShouldReleaseCaptureOnMouseReleased()) {
        native_widget_->ReleaseMouseCapture();
      }
      is_mouse_button_pressed_ = false;
      root_view_->OnMouseReleased(event, false);
      return true;
    case ui::ET_MOUSE_MOVED:
      if (native_widget_->HasMouseCapture() && is_mouse_button_pressed_) {
        last_mouse_event_was_move_ = false;
        root_view_->OnMouseDragged(event);
      } else {
        gfx::Point screen_loc(event.location());
        View::ConvertPointToScreen(root_view_.get(), &screen_loc);
        if (last_mouse_event_was_move_ &&
            last_mouse_event_position_ == screen_loc) {
          // Don't generate a mouse event for the same location as the last.
          return true;
        }
        last_mouse_event_position_ = screen_loc;
        last_mouse_event_was_move_ = true;
        root_view_->OnMouseMoved(event);
      }
      break;
    case ui::ET_MOUSE_EXITED:
      // TODO(beng): rename to OnMouseExited(event);
      root_view_->ProcessOnMouseExited();
      return true;
    default:
      break;
  }
  return true;
}

bool WidgetImpl::OnMouseWheelEvent(const MouseWheelEvent& event) {
  // TODO(beng): rename to OnMouseWheel(event);
  return !root_view_->ProcessMouseWheelEvent(event);
}

void WidgetImpl::OnNativeWidgetCreated() {
  root_view_->SetContentsView(contents_view_);
  if (GetTopLevelWidgetImpl() == this)
    focus_manager_.reset(new FocusManager(this));
}

void WidgetImpl::OnPaint(gfx::Canvas* canvas) {
  // TODO(beng): replace with root_view_->Paint(canvas);
#if defined(OS_WIN)
  root_view_->OnPaint(native_widget_->GetNativeView());
#endif
}

void WidgetImpl::OnSizeChanged(const gfx::Size& size) {
  root_view_->SetSize(size);
}

void WidgetImpl::OnNativeFocus(gfx::NativeView focused_view) {
  GetFocusManager()->GetWidgetFocusManager()->OnWidgetFocusEvent(
      focused_view, native_widget_->GetNativeView());
}

void WidgetImpl::OnNativeBlur(gfx::NativeView focused_view) {
  GetFocusManager()->GetWidgetFocusManager()->OnWidgetFocusEvent(
      native_widget_->GetNativeView(), focused_view);
}

void WidgetImpl::OnWorkAreaChanged() {

}

WidgetImpl* WidgetImpl::GetWidgetImpl() {
  return const_cast<WidgetImpl*>(const_cast<const WidgetImpl*>(this));
}

const WidgetImpl* WidgetImpl::GetWidgetImpl() const {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// WidgetImpl, Widget implementation: (TEMPORARY, TEMPORARY, TEMPORARY!)

void WidgetImpl::Init(gfx::NativeView parent, const gfx::Rect& bounds) {
  InitWithNativeViewParent(parent, bounds);
}

void WidgetImpl::InitWithWidget(Widget* parent, const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

WidgetDelegate* WidgetImpl::GetWidgetDelegate() {
  NOTIMPLEMENTED();
  return NULL;
}

void WidgetImpl::SetWidgetDelegate(WidgetDelegate* delegate) {
  NOTIMPLEMENTED();
}

void WidgetImpl::SetContentsView(View* view) {
  NOTIMPLEMENTED();
}

void WidgetImpl::GetBounds(gfx::Rect* out, bool including_frame) const {
  NOTIMPLEMENTED();
}

void WidgetImpl::MoveAbove(Widget* widget) {
  NativeWidget* other =
      NativeWidget::GetNativeWidgetForNativeView(widget->GetNativeView());
  if (other)
    native_widget_->MoveAbove(other);
}

void WidgetImpl::SetShape(gfx::NativeRegion region) {
  NOTIMPLEMENTED();
}

gfx::NativeView WidgetImpl::GetNativeView() const {
  return native_widget_->GetNativeView();
}

void WidgetImpl::PaintNow(const gfx::Rect& update_rect) {
  NOTIMPLEMENTED();
}

void WidgetImpl::SetOpacity(unsigned char opacity) {
  NOTIMPLEMENTED();
}

RootView* WidgetImpl::GetRootView() {
  return root_view_.get();
}

Widget* WidgetImpl::GetRootWidget() const {
  NOTIMPLEMENTED();
  return NULL;
}

bool WidgetImpl::IsVisible() const {
  NOTIMPLEMENTED();
  return false;
}

bool WidgetImpl::IsActive() const {
  NOTIMPLEMENTED();
  return false;
}

bool WidgetImpl::IsAccessibleWidget() const {
  NOTIMPLEMENTED();
  return false;
}

TooltipManager* WidgetImpl::GetTooltipManager() {
  NOTIMPLEMENTED();
  return NULL;
}

void WidgetImpl::GenerateMousePressedForView(View* view,
                                             const gfx::Point& point) {
  NOTIMPLEMENTED();
}

bool WidgetImpl::GetAccelerator(int cmd_id, ui::Accelerator* accelerator) {
  return false;
}

Window* WidgetImpl::GetWindow() {
  return NULL;
}

const Window* WidgetImpl::GetWindow() const {
  return NULL;
}

void WidgetImpl::SetNativeWindowProperty(const char* name, void* value) {
  native_widget_->SetNativeWindowProperty(name, value);
}

void* WidgetImpl::GetNativeWindowProperty(const char* name) {
  return native_widget_->GetNativeWindowProperty(name);
}

ThemeProvider* WidgetImpl::GetDefaultThemeProvider() const {
  NOTIMPLEMENTED();
  return NULL;
}

void WidgetImpl::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  NOTIMPLEMENTED();
}

bool WidgetImpl::ContainsNativeView(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// WidgetImpl, private:

void WidgetImpl::CloseNow() {
  native_widget_->Close();
}

#if !defined(OS_WIN)

////////////////////////////////////////////////////////////////////////////////
// NativeWidget, public:

// static
NativeWidget* NativeWidget::CreateNativeWidget(
    internal::NativeWidgetListener* listener) {
  return NULL;
}

// static
NativeWidget* NativeWidget::GetNativeWidgetForNativeView(
    gfx::NativeView native_view) {
  return NULL;
}

// static
NativeWidget* NativeWidget::GetNativeWidgetForNativeWindow(
    gfx::NativeWindow native_window) {
  return NULL;
}

// static
NativeWidget* NativeWidget::GetTopLevelNativeWidget(
    gfx::NativeView native_view) {
  return NULL;
}

#endif  // !defined(OS_WIN)

}  // namespace views

