// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_native_widget_aura.h"

#include "ui/aura/root_window_host.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/desktop_root_window_host.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, public:

DesktopNativeWidgetAura::DesktopNativeWidgetAura(
    internal::NativeWidgetDelegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(window_(new aura::Window(this))),
      ownership_(Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET),
      native_widget_delegate_(delegate) {
}

DesktopNativeWidgetAura::~DesktopNativeWidgetAura() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, internal::NativeWidgetPrivate implementation:

void DesktopNativeWidgetAura::InitNativeWidget(
    const Widget::InitParams& params) {
  window_->Init(params.layer_type);
  window_->Show();

  desktop_root_window_host_.reset(
      DesktopRootWindowHost::Create(native_widget_delegate_, params.bounds));
  desktop_root_window_host_->Init(window_, params);
}

NonClientFrameView* DesktopNativeWidgetAura::CreateNonClientFrameView() {
  return NULL;
}

bool DesktopNativeWidgetAura::ShouldUseNativeFrame() const {
  return false;
}

void DesktopNativeWidgetAura::FrameTypeChanged() {
}

Widget* DesktopNativeWidgetAura::GetWidget() {
  return native_widget_delegate_->AsWidget();
}

const Widget* DesktopNativeWidgetAura::GetWidget() const {
  return native_widget_delegate_->AsWidget();
}

gfx::NativeView DesktopNativeWidgetAura::GetNativeView() const {
  return window_;
}

gfx::NativeWindow DesktopNativeWidgetAura::GetNativeWindow() const {
  return window_;
}

Widget* DesktopNativeWidgetAura::GetTopLevelWidget() {
  return GetWidget();
}

const ui::Compositor* DesktopNativeWidgetAura::GetCompositor() const {
  return window_->layer()->GetCompositor();
}

ui::Compositor* DesktopNativeWidgetAura::GetCompositor() {
  return window_->layer()->GetCompositor();
}

void DesktopNativeWidgetAura::CalculateOffsetToAncestorWithLayer(
      gfx::Point* offset,
      ui::Layer** layer_parent) {
}

void DesktopNativeWidgetAura::ViewRemoved(View* view) {
}

void DesktopNativeWidgetAura::SetNativeWindowProperty(const char* name,
                                                      void* value) {
}

void* DesktopNativeWidgetAura::GetNativeWindowProperty(const char* name) const {
  return NULL;
}

TooltipManager* DesktopNativeWidgetAura::GetTooltipManager() const {
  return NULL;
}

bool DesktopNativeWidgetAura::IsScreenReaderActive() const {
  return false;
}

void DesktopNativeWidgetAura::SendNativeAccessibilityEvent(
      View* view,
      ui::AccessibilityTypes::Event event_type) {
}

void DesktopNativeWidgetAura::SetCapture() {
}

void DesktopNativeWidgetAura::ReleaseCapture() {
}

bool DesktopNativeWidgetAura::HasCapture() const {
  return false;
}

InputMethod* DesktopNativeWidgetAura::CreateInputMethod() {
  return desktop_root_window_host_->CreateInputMethod();
}

internal::InputMethodDelegate*
    DesktopNativeWidgetAura::GetInputMethodDelegate() {
  return desktop_root_window_host_->GetInputMethodDelegate();
}

void DesktopNativeWidgetAura::CenterWindow(const gfx::Size& size) {
  desktop_root_window_host_->CenterWindow(size);
}

void DesktopNativeWidgetAura::GetWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* maximized) const {
  desktop_root_window_host_->GetWindowPlacement(bounds, maximized);
}

void DesktopNativeWidgetAura::SetWindowTitle(const string16& title) {
  desktop_root_window_host_->SetWindowTitle(title);
}

void DesktopNativeWidgetAura::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                             const gfx::ImageSkia& app_icon) {
}

void DesktopNativeWidgetAura::SetAccessibleName(const string16& name) {
}

void DesktopNativeWidgetAura::SetAccessibleRole(
    ui::AccessibilityTypes::Role role) {
}

void DesktopNativeWidgetAura::SetAccessibleState(
    ui::AccessibilityTypes::State state) {
}

void DesktopNativeWidgetAura::InitModalType(ui::ModalType modal_type) {
}

gfx::Rect DesktopNativeWidgetAura::GetWindowBoundsInScreen() const {
  return desktop_root_window_host_->GetWindowBoundsInScreen();
}

gfx::Rect DesktopNativeWidgetAura::GetClientAreaBoundsInScreen() const {
  return desktop_root_window_host_->GetClientAreaBoundsInScreen();
}

gfx::Rect DesktopNativeWidgetAura::GetRestoredBounds() const {
  return desktop_root_window_host_->GetRestoredBounds();
}

void DesktopNativeWidgetAura::SetBounds(const gfx::Rect& bounds) {
  desktop_root_window_host_->AsRootWindowHost()->SetBounds(bounds);
}

void DesktopNativeWidgetAura::SetSize(const gfx::Size& size) {
  desktop_root_window_host_->SetSize(size);
}

void DesktopNativeWidgetAura::StackAbove(gfx::NativeView native_view) {
}

void DesktopNativeWidgetAura::StackAtTop() {
}

void DesktopNativeWidgetAura::StackBelow(gfx::NativeView native_view) {
}

void DesktopNativeWidgetAura::SetShape(gfx::NativeRegion shape) {
}

void DesktopNativeWidgetAura::Close() {
  desktop_root_window_host_->Close();
}

void DesktopNativeWidgetAura::CloseNow() {
  desktop_root_window_host_->CloseNow();
}

void DesktopNativeWidgetAura::Show() {
  desktop_root_window_host_->AsRootWindowHost()->Show();
}

void DesktopNativeWidgetAura::Hide() {
  desktop_root_window_host_->AsRootWindowHost()->Hide();
}

void DesktopNativeWidgetAura::ShowMaximizedWithBounds(
      const gfx::Rect& restored_bounds) {
  desktop_root_window_host_->ShowMaximizedWithBounds(restored_bounds);
}

void DesktopNativeWidgetAura::ShowWithWindowState(ui::WindowShowState state) {
  desktop_root_window_host_->ShowWindowWithState(state);
}

bool DesktopNativeWidgetAura::IsVisible() const {
  return desktop_root_window_host_->IsVisible();
}

void DesktopNativeWidgetAura::Activate() {
  desktop_root_window_host_->Activate();
}

void DesktopNativeWidgetAura::Deactivate() {
  desktop_root_window_host_->Deactivate();
}

bool DesktopNativeWidgetAura::IsActive() const {
  return desktop_root_window_host_->IsActive();
}

void DesktopNativeWidgetAura::SetAlwaysOnTop(bool always_on_top) {
  desktop_root_window_host_->SetAlwaysOnTop(always_on_top);
}

void DesktopNativeWidgetAura::Maximize() {
  desktop_root_window_host_->Maximize();
}

void DesktopNativeWidgetAura::Minimize() {
  desktop_root_window_host_->Minimize();
}

bool DesktopNativeWidgetAura::IsMaximized() const {
  return desktop_root_window_host_->IsMaximized();
}

bool DesktopNativeWidgetAura::IsMinimized() const {
  return desktop_root_window_host_->IsMinimized();
}

void DesktopNativeWidgetAura::Restore() {
  desktop_root_window_host_->Restore();
}

void DesktopNativeWidgetAura::SetFullscreen(bool fullscreen) {
}

bool DesktopNativeWidgetAura::IsFullscreen() const {
  return false;
}

void DesktopNativeWidgetAura::SetOpacity(unsigned char opacity) {
}

void DesktopNativeWidgetAura::SetUseDragFrame(bool use_drag_frame) {
}

void DesktopNativeWidgetAura::FlashFrame(bool flash_frame) {
}

bool DesktopNativeWidgetAura::IsAccessibleWidget() const {
  return false;
}

void DesktopNativeWidgetAura::RunShellDrag(View* view,
                            const ui::OSExchangeData& data,
                            const gfx::Point& location,
                            int operation) {
}

void DesktopNativeWidgetAura::SchedulePaintInRect(const gfx::Rect& rect) {
  if (window_)
    window_->SchedulePaintInRect(rect);
}

void DesktopNativeWidgetAura::SetCursor(gfx::NativeCursor cursor) {
}

void DesktopNativeWidgetAura::ClearNativeFocus() {
}

void DesktopNativeWidgetAura::FocusNativeView(gfx::NativeView native_view) {
}

gfx::Rect DesktopNativeWidgetAura::GetWorkAreaBoundsInScreen() const {
  return gfx::Rect(100, 100);
}

void DesktopNativeWidgetAura::SetInactiveRenderingDisabled(bool value) {
}

Widget::MoveLoopResult DesktopNativeWidgetAura::RunMoveLoop(
      const gfx::Point& drag_offset) {
  return Widget::MOVE_LOOP_CANCELED;
}

void DesktopNativeWidgetAura::EndMoveLoop() {
}

void DesktopNativeWidgetAura::SetVisibilityChangedAnimationsEnabled(
    bool value) {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::WindowDelegate implementation:

gfx::Size DesktopNativeWidgetAura::GetMinimumSize() const {
  return gfx::Size(100, 100);
}

void DesktopNativeWidgetAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
  if (old_bounds.origin() != new_bounds.origin())
    native_widget_delegate_->OnNativeWidgetMove();
  if (old_bounds.size() != new_bounds.size())
    native_widget_delegate_->OnNativeWidgetSizeChanged(new_bounds.size());
}

void DesktopNativeWidgetAura::OnFocus(aura::Window* old_focused_window) {
}

void DesktopNativeWidgetAura::OnBlur() {
}

gfx::NativeCursor DesktopNativeWidgetAura::GetCursor(const gfx::Point& point) {
  return gfx::kNullCursor;
}

int DesktopNativeWidgetAura::GetNonClientComponent(
    const gfx::Point& point) const {
  // TODO(beng): seems like this shouldn't be necessary here, right?
  return HTCLIENT;
}

bool DesktopNativeWidgetAura::ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) {
  return true;
}

bool DesktopNativeWidgetAura::CanFocus() {
  return true;
}

void DesktopNativeWidgetAura::OnCaptureLost() {
}

void DesktopNativeWidgetAura::OnPaint(gfx::Canvas* canvas) {
  native_widget_delegate_->OnNativeWidgetPaint(canvas);
}

void DesktopNativeWidgetAura::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
}

void DesktopNativeWidgetAura::OnWindowDestroying() {
}

void DesktopNativeWidgetAura::OnWindowDestroyed() {
}

void DesktopNativeWidgetAura::OnWindowTargetVisibilityChanged(bool visible) {
}

bool DesktopNativeWidgetAura::HasHitTestMask() const {
  return false;
}

void DesktopNativeWidgetAura::GetHitTestMask(gfx::Path* mask) const {
}

scoped_refptr<ui::Texture> DesktopNativeWidgetAura::CopyTexture() {
  // The layer we create doesn't have an external texture, so this should never
  // get invoked.
  NOTREACHED();
  return scoped_refptr<ui::Texture>();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, ui::EventHandler implementation:

ui::EventResult DesktopNativeWidgetAura::OnKeyEvent(ui::KeyEvent* event) {
  return native_widget_delegate_->OnKeyEvent(*event) ? ui::ER_HANDLED :
      ui::ER_UNHANDLED;
}

ui::EventResult DesktopNativeWidgetAura::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(window_->IsVisible());
  if (event->type() == ui::ET_MOUSEWHEEL) {
    return native_widget_delegate_->OnMouseEvent(*event) ?
        ui::ER_HANDLED : ui::ER_UNHANDLED;
  }

  if (event->type() == ui::ET_SCROLL) {
    if (native_widget_delegate_->OnMouseEvent(*event))
      return ui::ER_HANDLED;

    // Convert unprocessed scroll events into wheel events.
    ui::MouseWheelEvent mwe(*static_cast<ui::ScrollEvent*>(event));
    return native_widget_delegate_->OnMouseEvent(mwe) ?
        ui::ER_HANDLED : ui::ER_UNHANDLED;
  }
  return native_widget_delegate_->OnMouseEvent(*event) ?
      ui::ER_HANDLED : ui::ER_UNHANDLED;
}

ui::TouchStatus DesktopNativeWidgetAura::OnTouchEvent(ui::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult DesktopNativeWidgetAura::OnGestureEvent(
    ui::GestureEvent* event) {
  return ui::ER_UNHANDLED;
}

}  // namespace views
