// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_NATIVE_WIDGET_MAC_H_
#define UI_VIEWS_WIDGET_NATIVE_WIDGET_MAC_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/native_widget_private.h"

namespace views {
namespace test {
class MockNativeWidgetMac;
}

class BridgedNativeWidget;

class VIEWS_EXPORT NativeWidgetMac : public internal::NativeWidgetPrivate {
 public:
  NativeWidgetMac(internal::NativeWidgetDelegate* delegate);
  virtual ~NativeWidgetMac();

  // Deletes |bridge_| and informs |delegate_| that the native widget is
  // destroyed.
  void OnWindowWillClose();

  // internal::NativeWidgetPrivate:
  virtual void InitNativeWidget(const Widget::InitParams& params) override;
  virtual NonClientFrameView* CreateNonClientFrameView() override;
  virtual bool ShouldUseNativeFrame() const override;
  virtual bool ShouldWindowContentsBeTransparent() const override;
  virtual void FrameTypeChanged() override;
  virtual Widget* GetWidget() override;
  virtual const Widget* GetWidget() const override;
  virtual gfx::NativeView GetNativeView() const override;
  virtual gfx::NativeWindow GetNativeWindow() const override;
  virtual Widget* GetTopLevelWidget() override;
  virtual const ui::Compositor* GetCompositor() const override;
  virtual const ui::Layer* GetLayer() const override;
  virtual void ReorderNativeViews() override;
  virtual void ViewRemoved(View* view) override;
  virtual void SetNativeWindowProperty(const char* name, void* value) override;
  virtual void* GetNativeWindowProperty(const char* name) const override;
  virtual TooltipManager* GetTooltipManager() const override;
  virtual void SetCapture() override;
  virtual void ReleaseCapture() override;
  virtual bool HasCapture() const override;
  virtual InputMethod* CreateInputMethod() override;
  virtual internal::InputMethodDelegate* GetInputMethodDelegate() override;
  virtual ui::InputMethod* GetHostInputMethod() override;
  virtual void CenterWindow(const gfx::Size& size) override;
  virtual void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* maximized) const override;
  virtual bool SetWindowTitle(const base::string16& title) override;
  virtual void SetWindowIcons(const gfx::ImageSkia& window_icon,
                              const gfx::ImageSkia& app_icon) override;
  virtual void InitModalType(ui::ModalType modal_type) override;
  virtual gfx::Rect GetWindowBoundsInScreen() const override;
  virtual gfx::Rect GetClientAreaBoundsInScreen() const override;
  virtual gfx::Rect GetRestoredBounds() const override;
  virtual void SetBounds(const gfx::Rect& bounds) override;
  virtual void SetSize(const gfx::Size& size) override;
  virtual void StackAbove(gfx::NativeView native_view) override;
  virtual void StackAtTop() override;
  virtual void StackBelow(gfx::NativeView native_view) override;
  virtual void SetShape(gfx::NativeRegion shape) override;
  virtual void Close() override;
  virtual void CloseNow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual void ShowMaximizedWithBounds(
      const gfx::Rect& restored_bounds) override;
  virtual void ShowWithWindowState(ui::WindowShowState state) override;
  virtual bool IsVisible() const override;
  virtual void Activate() override;
  virtual void Deactivate() override;
  virtual bool IsActive() const override;
  virtual void SetAlwaysOnTop(bool always_on_top) override;
  virtual bool IsAlwaysOnTop() const override;
  virtual void SetVisibleOnAllWorkspaces(bool always_visible) override;
  virtual void Maximize() override;
  virtual void Minimize() override;
  virtual bool IsMaximized() const override;
  virtual bool IsMinimized() const override;
  virtual void Restore() override;
  virtual void SetFullscreen(bool fullscreen) override;
  virtual bool IsFullscreen() const override;
  virtual void SetOpacity(unsigned char opacity) override;
  virtual void SetUseDragFrame(bool use_drag_frame) override;
  virtual void FlashFrame(bool flash_frame) override;
  virtual void RunShellDrag(View* view,
                            const ui::OSExchangeData& data,
                            const gfx::Point& location,
                            int operation,
                            ui::DragDropTypes::DragEventSource source) override;
  virtual void SchedulePaintInRect(const gfx::Rect& rect) override;
  virtual void SetCursor(gfx::NativeCursor cursor) override;
  virtual bool IsMouseEventsEnabled() const override;
  virtual void ClearNativeFocus() override;
  virtual gfx::Rect GetWorkAreaBoundsInScreen() const override;
  virtual Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;
  virtual void EndMoveLoop() override;
  virtual void SetVisibilityChangedAnimationsEnabled(bool value) override;
  virtual void SetVisibilityAnimationDuration(
      const base::TimeDelta& duration) override;
  virtual void SetVisibilityAnimationTransition(
      Widget::VisibilityTransition transition) override;
  virtual ui::NativeTheme* GetNativeTheme() const override;
  virtual void OnRootViewLayout() override;
  virtual bool IsTranslucentWindowOpacitySupported() const override;
  virtual void OnSizeConstraintsChanged() override;
  virtual void RepostNativeEvent(gfx::NativeEvent native_event) override;

 protected:
  internal::NativeWidgetDelegate* delegate() { return delegate_; }

 private:
  friend class test::MockNativeWidgetMac;

  internal::NativeWidgetDelegate* delegate_;
  scoped_ptr<BridgedNativeWidget> bridge_;

  Widget::InitParams::Ownership ownership_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMac);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_NATIVE_WIDGET_MAC_H_
