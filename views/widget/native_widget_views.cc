// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_views.h"

#include "views/view.h"
#include "views/widget/native_widget_view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetViews, public:

NativeWidgetViews::NativeWidgetViews(View* host,
                                     internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate),
      view_(NULL),
      host_view_(host),
      active_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_widget_factory_(this)) {
}

NativeWidgetViews::~NativeWidgetViews() {
}

View* NativeWidgetViews::GetView() {
  return view_;
}

const View* NativeWidgetViews::GetView() const {
  return view_;
}

void NativeWidgetViews::OnActivate(bool active) {
  active_ = active;
  view_->SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetViews, NativeWidget implementation:

void NativeWidgetViews::InitNativeWidget(const Widget::InitParams& params) {
  view_ = new internal::NativeWidgetView(this);
  host_view_->AddChildView(view_);

  // TODO(beng): handle parenting.
  // TODO(beng): SetInitParams().
}

Widget* NativeWidgetViews::GetWidget() {
  return delegate_->AsWidget();
}

const Widget* NativeWidgetViews::GetWidget() const {
  return delegate_->AsWidget();
}

gfx::NativeView NativeWidgetViews::GetNativeView() const {
  return GetParentNativeWidget()->GetNativeView();
}

gfx::NativeWindow NativeWidgetViews::GetNativeWindow() const {
  return GetParentNativeWidget()->GetNativeWindow();
}

Window* NativeWidgetViews::GetContainingWindow() {
  return view_->GetWindow();
}

const Window* NativeWidgetViews::GetContainingWindow() const {
  return view_->GetWindow();
}

void NativeWidgetViews::ViewRemoved(View* view) {
  return GetParentNativeWidget()->ViewRemoved(view);
}

void NativeWidgetViews::SetNativeWindowProperty(const char* name, void* value) {
  NOTIMPLEMENTED();
}

void* NativeWidgetViews::GetNativeWindowProperty(const char* name) {
  NOTIMPLEMENTED();
  return NULL;
}

TooltipManager* NativeWidgetViews::GetTooltipManager() const {
  return GetParentNativeWidget()->GetTooltipManager();
}

bool NativeWidgetViews::IsScreenReaderActive() const {
  return GetParentNativeWidget()->IsScreenReaderActive();
}

void NativeWidgetViews::SendNativeAccessibilityEvent(
    View* view,
    ui::AccessibilityTypes::Event event_type) {
  return GetParentNativeWidget()->SendNativeAccessibilityEvent(view,
                                                               event_type);
}

void NativeWidgetViews::SetMouseCapture() {
  GetParentNativeWidget()->SetMouseCapture();
}

void NativeWidgetViews::ReleaseMouseCapture() {
  GetParentNativeWidget()->ReleaseMouseCapture();
}

bool NativeWidgetViews::HasMouseCapture() const {
  return GetParentNativeWidget()->HasMouseCapture();
}

bool NativeWidgetViews::IsMouseButtonDown() const {
  return GetParentNativeWidget()->IsMouseButtonDown();
}

InputMethod* NativeWidgetViews::GetInputMethodNative() {
  return GetParentNativeWidget()->GetInputMethodNative();
}

void NativeWidgetViews::ReplaceInputMethod(InputMethod* input_method) {
  GetParentNativeWidget()->ReplaceInputMethod(input_method);
}

gfx::AcceleratedWidget NativeWidgetViews::GetAcceleratedWidget() {
  // TODO(sky):
  return gfx::kNullAcceleratedWidget;
}

gfx::Rect NativeWidgetViews::GetWindowScreenBounds() const {
  gfx::Point origin = view_->bounds().origin();
  View::ConvertPointToScreen(view_->parent(), &origin);
  return gfx::Rect(origin.x(), origin.y(), view_->width(), view_->height());
}

gfx::Rect NativeWidgetViews::GetClientAreaScreenBounds() const {
  return GetWindowScreenBounds();
}

void NativeWidgetViews::SetBounds(const gfx::Rect& bounds) {
  // |bounds| are supplied in the coordinates of the parent.
  view_->SetBoundsRect(bounds);
}

void NativeWidgetViews::SetSize(const gfx::Size& size) {
  view_->SetSize(size);
}

void NativeWidgetViews::MoveAbove(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetViews::SetShape(gfx::NativeRegion region) {
  NOTIMPLEMENTED();
}

void NativeWidgetViews::Close() {
  Hide();
  if (close_widget_factory_.empty()) {
    MessageLoop::current()->PostTask(FROM_HERE,
        close_widget_factory_.NewRunnableMethod(&NativeWidgetViews::CloseNow));
  }
}

void NativeWidgetViews::CloseNow() {
  view_->parent()->RemoveChildView(view_);
  delete view_;
}

void NativeWidgetViews::Show() {
  view_->SetVisible(true);
}

void NativeWidgetViews::Hide() {
  view_->SetVisible(false);
}

void NativeWidgetViews::SetOpacity(unsigned char opacity) {
  NOTIMPLEMENTED();
}

void NativeWidgetViews::SetAlwaysOnTop(bool on_top) {
  NOTIMPLEMENTED();
}

bool NativeWidgetViews::IsVisible() const {
  return view_->IsVisible();
}

bool NativeWidgetViews::IsActive() const {
  return active_;
}

bool NativeWidgetViews::IsAccessibleWidget() const {
  NOTIMPLEMENTED();
  return false;
}

bool NativeWidgetViews::ContainsNativeView(gfx::NativeView native_view) const {
  NOTIMPLEMENTED();
  return GetParentNativeWidget()->ContainsNativeView(native_view);
}

void NativeWidgetViews::RunShellDrag(View* view,
                                     const ui::OSExchangeData& data,
                                     int operation) {
  GetParentNativeWidget()->RunShellDrag(view, data, operation);
}

void NativeWidgetViews::SchedulePaintInRect(const gfx::Rect& rect) {
  view_->SchedulePaintInRect(rect);
}

void NativeWidgetViews::SetCursor(gfx::NativeCursor cursor) {
  GetParentNativeWidget()->SetCursor(cursor);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetViews, private:

NativeWidget* NativeWidgetViews::GetParentNativeWidget() {
  return view_->GetWidget()->native_widget();
}

const NativeWidget* NativeWidgetViews::GetParentNativeWidget() const {
  return view_->GetWidget()->native_widget();
}

}  // namespace views

