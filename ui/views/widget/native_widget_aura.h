// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_NATIVE_WIDGET_AURA_H_
#define UI_VIEWS_WIDGET_NATIVE_WIDGET_AURA_H_

#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/event_constants.h"
#include "ui/views/ime/input_method_delegate.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_delegate.h"
#include "ui/wm/public/drag_drop_delegate.h"

namespace aura {
class Window;
}
namespace gfx {
class FontList;
}

namespace views {

class DropHelper;
class TooltipManagerAura;
class WindowReorderer;

class VIEWS_EXPORT NativeWidgetAura
    : public internal::NativeWidgetPrivate,
      public internal::InputMethodDelegate,
      public aura::WindowDelegate,
      public aura::WindowObserver,
      public aura::client::ActivationDelegate,
      public aura::client::ActivationChangeObserver,
      public aura::client::FocusChangeObserver,
      public aura::client::DragDropDelegate {
 public:
  explicit NativeWidgetAura(internal::NativeWidgetDelegate* delegate);

  // Called internally by NativeWidgetAura and DesktopNativeWidgetAura to
  // associate |native_widget| with |window|.
  static void RegisterNativeWidgetForWindow(
      internal::NativeWidgetPrivate* native_widget,
      aura::Window* window);

  // Overridden from internal::NativeWidgetPrivate:
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
  virtual ui::Compositor* GetCompositor() override;
  virtual ui::Layer* GetLayer() override;
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
  virtual ui::NativeTheme* GetNativeTheme() const override;
  virtual void OnRootViewLayout() override;
  virtual bool IsTranslucentWindowOpacitySupported() const override;
  virtual void OnSizeConstraintsChanged() override;
  virtual void RepostNativeEvent(gfx::NativeEvent native_event) override;

  // Overridden from views::InputMethodDelegate:
  virtual void DispatchKeyEventPostIME(const ui::KeyEvent& key) override;

  // Overridden from aura::WindowDelegate:
  virtual gfx::Size GetMinimumSize() const override;
  virtual gfx::Size GetMaximumSize() const override;
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) override;
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) override;
  virtual int GetNonClientComponent(const gfx::Point& point) const override;
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override;
  virtual bool CanFocus() override;
  virtual void OnCaptureLost() override;
  virtual void OnPaint(gfx::Canvas* canvas) override;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  virtual void OnWindowDestroying(aura::Window* window) override;
  virtual void OnWindowDestroyed(aura::Window* window) override;
  virtual void OnWindowTargetVisibilityChanged(bool visible) override;
  virtual bool HasHitTestMask() const override;
  virtual void GetHitTestMask(gfx::Path* mask) const override;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) override;

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) override;
  virtual void OnMouseEvent(ui::MouseEvent* event) override;
  virtual void OnScrollEvent(ui::ScrollEvent* event) override;
  virtual void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from aura::client::ActivationDelegate:
  virtual bool ShouldActivate() const override;

  // Overridden from aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) override;

  // Overridden from aura::client::FocusChangeObserver:
  virtual void OnWindowFocused(aura::Window* gained_focus,
                               aura::Window* lost_focus) override;

  // Overridden from aura::client::DragDropDelegate:
  virtual void OnDragEntered(const ui::DropTargetEvent& event) override;
  virtual int OnDragUpdated(const ui::DropTargetEvent& event) override;
  virtual void OnDragExited() override;
  virtual int OnPerformDrop(const ui::DropTargetEvent& event) override;

 protected:
  virtual ~NativeWidgetAura();

  internal::NativeWidgetDelegate* delegate() { return delegate_; }

 private:
  class ActiveWindowObserver;

  void SetInitialFocus(ui::WindowShowState show_state);

  internal::NativeWidgetDelegate* delegate_;

  // WARNING: set to NULL when destroyed. As the Widget is not necessarily
  // destroyed along with |window_| all usage of |window_| should first verify
  // non-NULL.
  aura::Window* window_;

  // See class documentation for Widget in widget.h for a note about ownership.
  Widget::InitParams::Ownership ownership_;

  // Are we in the destructor?
  bool destroying_;

  gfx::NativeCursor cursor_;

  // The saved window state for exiting full screen state.
  ui::WindowShowState saved_window_state_;

  scoped_ptr<TooltipManagerAura> tooltip_manager_;

  // Reorders child windows of |window_| associated with a view based on the
  // order of the associated views in the widget's view hierarchy.
  scoped_ptr<WindowReorderer> window_reorderer_;

  scoped_ptr<DropHelper> drop_helper_;
  int last_drop_operation_;

  // The following factory is used for calls to close the NativeWidgetAura
  // instance.
  base::WeakPtrFactory<NativeWidgetAura> close_widget_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetAura);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_NATIVE_WIDGET_AURA_H_
