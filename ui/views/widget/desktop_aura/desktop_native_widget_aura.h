// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_WIDGET_AURA_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_WIDGET_AURA_H_

#include "base/memory/weak_ptr.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/client/activation_delegate.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/cursor/cursor.h"
#include "ui/views/ime/input_method_delegate.h"
#include "ui/views/widget/native_widget_private.h"

namespace aura {
class RootWindow;
namespace client {
class StackingClient;
}
}

namespace views {

namespace corewm {
class CompoundEventFilter;
class InputMethodEventFilter;
class ScopedCaptureClient;
class ShadowController;
class TooltipController;
class VisibilityController;
class WindowModalityController;
}

class DesktopRootWindowHost;
class DropHelper;
class NativeWidgetAuraWindowObserver;
class TooltipManagerAura;
class WindowReorderer;

class VIEWS_EXPORT DesktopNativeWidgetAura
    : public internal::NativeWidgetPrivate,
      public aura::WindowDelegate,
      public aura::client::ActivationDelegate,
      public aura::client::ActivationChangeObserver,
      public aura::client::FocusChangeObserver,
      public views::internal::InputMethodDelegate,
      public aura::client::DragDropDelegate,
      public aura::RootWindowObserver {
 public:
  explicit DesktopNativeWidgetAura(internal::NativeWidgetDelegate* delegate);
  virtual ~DesktopNativeWidgetAura();

  // Maps from window to DesktopNativeWidgetAura.
  static DesktopNativeWidgetAura* ForWindow(aura::Window* window);

  // Called by our DesktopRootWindowHost after it has deleted native resources;
  // this is the signal that we should start our shutdown.
  virtual void OnHostClosed();

  // Installs the input method filter on |root|. This is intended to be invoked
  // by the DesktopRootWindowHost implementation during Init().
  void InstallInputMethodEventFilter(aura::RootWindow* root);
  corewm::InputMethodEventFilter* input_method_event_filter() {
    return input_method_event_filter_.get();
  }
  corewm::CompoundEventFilter* root_window_event_filter() {
    return root_window_event_filter_;
  }

  // Invoked from DesktopRootWindowHost creation to create the CaptureClient.
  void CreateCaptureClient(aura::RootWindow* root);

  // Overridden from NativeWidget:
  virtual ui::EventHandler* GetEventHandler() OVERRIDE;

  // Ensures that the correct window is activated/deactivated based on whether
  // we are being activated/deactivated.
  void HandleActivationChanged(bool active);

  // Installs the window modality controller event filter on the |root|. This
  // should be invoked by the DesktopRootWindowHost implementation immediately
  // after creation of the RootWindow.
  void InstallWindowModalityController(aura::RootWindow* root);

 protected:
  // Overridden from internal::NativeWidgetPrivate:
  virtual void InitNativeWidget(const Widget::InitParams& params) OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView() OVERRIDE;
  virtual bool ShouldUseNativeFrame() const OVERRIDE;
  virtual void FrameTypeChanged() OVERRIDE;
  virtual Widget* GetWidget() OVERRIDE;
  virtual const Widget* GetWidget() const OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual Widget* GetTopLevelWidget() OVERRIDE;
  virtual const ui::Compositor* GetCompositor() const OVERRIDE;
  virtual ui::Compositor* GetCompositor() OVERRIDE;
  virtual ui::Layer* GetLayer() OVERRIDE;
  virtual void ReorderNativeViews() OVERRIDE;
  virtual void ViewRemoved(View* view) OVERRIDE;
  virtual void SetNativeWindowProperty(const char* name, void* value) OVERRIDE;
  virtual void* GetNativeWindowProperty(const char* name) const OVERRIDE;
  virtual TooltipManager* GetTooltipManager() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual bool HasCapture() const OVERRIDE;
  virtual InputMethod* CreateInputMethod() OVERRIDE;
  virtual internal::InputMethodDelegate* GetInputMethodDelegate() OVERRIDE;
  virtual void CenterWindow(const gfx::Size& size) OVERRIDE;
  virtual void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* maximized) const OVERRIDE;
  virtual void SetWindowTitle(const string16& title) OVERRIDE;
  virtual void SetWindowIcons(const gfx::ImageSkia& window_icon,
                              const gfx::ImageSkia& app_icon) OVERRIDE;
  virtual void InitModalType(ui::ModalType modal_type) OVERRIDE;
  virtual gfx::Rect GetWindowBoundsInScreen() const OVERRIDE;
  virtual gfx::Rect GetClientAreaBoundsInScreen() const OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void StackAbove(gfx::NativeView native_view) OVERRIDE;
  virtual void StackAtTop() OVERRIDE;
  virtual void StackBelow(gfx::NativeView native_view) OVERRIDE;
  virtual void SetShape(gfx::NativeRegion shape) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void CloseNow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ShowMaximizedWithBounds(
      const gfx::Rect& restored_bounds) OVERRIDE;
  virtual void ShowWithWindowState(ui::WindowShowState state) OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual void SetAlwaysOnTop(bool always_on_top) OVERRIDE;
  virtual bool IsAlwaysOnTop() const OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual void SetFullscreen(bool fullscreen) OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual void SetOpacity(unsigned char opacity) OVERRIDE;
  virtual void SetUseDragFrame(bool use_drag_frame) OVERRIDE;
  virtual void FlashFrame(bool flash_frame) OVERRIDE;
  virtual void RunShellDrag(View* view,
                            const ui::OSExchangeData& data,
                            const gfx::Point& location,
                            int operation,
                            ui::DragDropTypes::DragEventSource source) OVERRIDE;
  virtual void SchedulePaintInRect(const gfx::Rect& rect) OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual bool IsMouseEventsEnabled() const OVERRIDE;
  virtual void ClearNativeFocus() OVERRIDE;
  virtual gfx::Rect GetWorkAreaBoundsInScreen() const OVERRIDE;
  virtual void SetInactiveRenderingDisabled(bool value) OVERRIDE;
  virtual Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) OVERRIDE;
  virtual void EndMoveLoop() OVERRIDE;
  virtual void SetVisibilityChangedAnimationsEnabled(bool value) OVERRIDE;
  virtual ui::NativeTheme* GetNativeTheme() const OVERRIDE;
  virtual void OnRootViewLayout() const OVERRIDE;

  // Overridden from aura::WindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual gfx::Size GetMaximumSize() const OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {}
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE;
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE;
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) OVERRIDE;
  virtual bool CanFocus() OVERRIDE;
  virtual void OnCaptureLost() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void OnWindowDestroying() OVERRIDE;
  virtual void OnWindowDestroyed() OVERRIDE;
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE;
  virtual bool HasHitTestMask() const OVERRIDE;
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE;
  virtual void DidRecreateLayer(ui::Layer* old_layer,
                                ui::Layer* new_layer) OVERRIDE;

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Overridden from aura::client::ActivationDelegate:
  virtual bool ShouldActivate() const OVERRIDE;

  // Overridden from aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

  // Overridden from aura::client::FocusChangeObserver:
  virtual void OnWindowFocused(aura::Window* gained_focus,
                               aura::Window* lost_focus) OVERRIDE;

  // Overridden from views::internal::InputMethodDelegate:
  virtual void DispatchKeyEventPostIME(const ui::KeyEvent& key) OVERRIDE;

  // Overridden from aura::client::DragDropDelegate:
  virtual void OnDragEntered(const ui::DropTargetEvent& event) OVERRIDE;
  virtual int OnDragUpdated(const ui::DropTargetEvent& event) OVERRIDE;
  virtual void OnDragExited() OVERRIDE;
  virtual int OnPerformDrop(const ui::DropTargetEvent& event) OVERRIDE;

  // Overridden from aura::RootWindowObserver:
  virtual void OnRootWindowHostCloseRequested(
      const aura::RootWindow* root) OVERRIDE;
  virtual void OnRootWindowHostResized(const aura::RootWindow* root) OVERRIDE;
  virtual void OnRootWindowHostMoved(const aura::RootWindow* root,
                                     const gfx::Point& new_origin) OVERRIDE;

 private:
  // To save a clear on platforms where the window is never transparent, the
  // window is only set as transparent when the glass frame is in use.
  void UpdateWindowTransparency();

  // See class documentation for Widget in widget.h for a note about ownership.
  Widget::InitParams::Ownership ownership_;

  scoped_ptr<corewm::ScopedCaptureClient> capture_client_;

  // The NativeWidget owns the RootWindow. Required because the RootWindow owns
  // its RootWindowHost, so DesktopRootWindowHost can't own it.
  scoped_ptr<aura::RootWindow> root_window_;

  // The following factory is used for calls to close the NativeWidgetAura
  // instance.
  base::WeakPtrFactory<DesktopNativeWidgetAura> close_widget_factory_;

  scoped_ptr<NativeWidgetAuraWindowObserver> active_window_observer_;

  // Can we be made active?
  bool can_activate_;

  // Ownership passed to RootWindow on Init.
  DesktopRootWindowHost* desktop_root_window_host_;

  // The content of |root_window_|. WARNING: this may be NULL if deleted out
  // from under us.
  aura::Window* window_;

  // Contains the content window defined above. Ensures that ZOrder changes
  // occurring in the content window hierarchy don't affect the other children
  // of the root window.
  aura::Window* content_window_container_;

  internal::NativeWidgetDelegate* native_widget_delegate_;

  scoped_ptr<aura::client::StackingClient> stacking_client_;

  // Toplevel event filter which dispatches to other event filters.
  corewm::CompoundEventFilter* root_window_event_filter_;

  scoped_ptr<corewm::InputMethodEventFilter> input_method_event_filter_;

  scoped_ptr<DropHelper> drop_helper_;
  int last_drop_operation_;

  scoped_ptr<corewm::TooltipController> tooltip_controller_;
  scoped_ptr<TooltipManagerAura> tooltip_manager_;

  scoped_ptr<views::corewm::VisibilityController> visibility_controller_;

  scoped_ptr<views::corewm::WindowModalityController>
      window_modality_controller_;

  // See comments in OnLostActive().
  bool restore_focus_on_activate_;

  gfx::NativeCursor cursor_;

  scoped_ptr<corewm::ShadowController> shadow_controller_;

  // Reorders child windows of |window_| associated with a view based on the
  // order of the associated views in the widget's view hierarchy.
  scoped_ptr<WindowReorderer> window_reorderer_;

  // See class documentation for Widget in widget.h for a note about type.
  Widget::InitParams::Type widget_type_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNativeWidgetAura);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_NATIVE_WIDGET_AURA_H_
