// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_NATIVE_WIDGET_MUS_H_
#define UI_VIEWS_MUS_NATIVE_WIDGET_MUS_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/views/mus/mus_export.h"
#include "ui/views/widget/native_widget_private.h"

namespace aura {
namespace client {
class DefaultCaptureClient;
class WindowTreeClient;
}
class Window;
}

namespace mojo {
class Shell;
}

namespace mus {
class Window;
class WindowTreeConnection;

namespace mojom {
class WindowManager;
}
}

namespace wm {
class FocusController;
}

namespace views {
class SurfaceContextFactory;
class WidgetDelegate;
struct WindowManagerClientAreaInsets;
class WindowTreeHostMus;

// An implementation of NativeWidget that binds to a mus::Window. Because Aura
// is used extensively within Views code, this code uses aura and binds to the
// mus::Window via a Mus-specific aura::WindowTreeHost impl. Because the root
// aura::Window in a hierarchy is created without a delegate by the
// aura::WindowTreeHost, we must create a child aura::Window in this class
// (content_) and attach it to the root.
class VIEWS_MUS_EXPORT NativeWidgetMus : public internal::NativeWidgetPrivate,
                                         public aura::WindowDelegate,
                                         public aura::WindowTreeHostObserver {
 public:
  NativeWidgetMus(internal::NativeWidgetDelegate* delegate,
                  mojo::Shell* shell,
                  mus::Window* window,
                  mus::mojom::SurfaceType surface_type);
  ~NativeWidgetMus() override;

  // Configures the set of properties supplied to the window manager when
  // creating a new Window for a Widget.
  static void ConfigurePropertiesForNewWindow(
      const Widget::InitParams& init_params,
      std::map<std::string, std::vector<uint8_t>>* properties);

  // Notifies all widgets the frame constants changed in some way.
  static void NotifyFrameChanged(mus::WindowTreeConnection* connection);

  mus::Window* window() { return window_; }

  void OnPlatformWindowClosed();
  void OnActivationChanged(bool active);

 protected:
  // Updates the client area in the mus::Window.
  virtual void UpdateClientArea();

  // internal::NativeWidgetPrivate:
  NonClientFrameView* CreateNonClientFrameView() override;
  void InitNativeWidget(const Widget::InitParams& params) override;
  void OnWidgetInitDone() override;
  bool ShouldUseNativeFrame() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void FrameTypeChanged() override;
  Widget* GetWidget() override;
  const Widget* GetWidget() const override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  Widget* GetTopLevelWidget() override;
  const ui::Compositor* GetCompositor() const override;
  const ui::Layer* GetLayer() const override;
  void ReorderNativeViews() override;
  void ViewRemoved(View* view) override;
  void SetNativeWindowProperty(const char* name, void* value) override;
  void* GetNativeWindowProperty(const char* name) const override;
  TooltipManager* GetTooltipManager() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  ui::InputMethod* GetInputMethod() override;
  void CenterWindow(const gfx::Size& size) override;
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* maximized) const override;
  bool SetWindowTitle(const base::string16& title) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void InitModalType(ui::ModalType modal_type) override;
  gfx::Rect GetWindowBoundsInScreen() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  gfx::Rect GetRestoredBounds() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void SetSize(const gfx::Size& size) override;
  void StackAbove(gfx::NativeView native_view) override;
  void StackAtTop() override;
  void StackBelow(gfx::NativeView native_view) override;
  void SetShape(SkRegion* shape) override;
  void Close() override;
  void CloseNow() override;
  void Show() override;
  void Hide() override;
  void ShowMaximizedWithBounds(const gfx::Rect& restored_bounds) override;
  void ShowWithWindowState(ui::WindowShowState state) override;
  bool IsVisible() const override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override;
  void SetAlwaysOnTop(bool always_on_top) override;
  bool IsAlwaysOnTop() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  void Maximize() override;
  void Minimize() override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  void Restore() override;
  void SetFullscreen(bool fullscreen) override;
  bool IsFullscreen() const override;
  void SetOpacity(unsigned char opacity) override;
  void SetUseDragFrame(bool use_drag_frame) override;
  void FlashFrame(bool flash_frame) override;
  void RunShellDrag(View* view,
                    const ui::OSExchangeData& data,
                    const gfx::Point& location,
                    int operation,
                    ui::DragDropTypes::DragEventSource source) override;
  void SchedulePaintInRect(const gfx::Rect& rect) override;
  void SetCursor(gfx::NativeCursor cursor) override;
  bool IsMouseEventsEnabled() const override;
  void ClearNativeFocus() override;
  gfx::Rect GetWorkAreaBoundsInScreen() const override;
  Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;
  void EndMoveLoop() override;
  void SetVisibilityChangedAnimationsEnabled(bool value) override;
  void SetVisibilityAnimationDuration(const base::TimeDelta& duration) override;
  void SetVisibilityAnimationTransition(
      Widget::VisibilityTransition transition) override;
  ui::NativeTheme* GetNativeTheme() const override;
  void OnRootViewLayout() override;
  bool IsTranslucentWindowOpacitySupported() const override;
  void OnSizeConstraintsChanged() override;
  void RepostNativeEvent(gfx::NativeEvent native_event) override;

  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override;
  gfx::NativeCursor GetCursor(const gfx::Point& point) override;
  int GetNonClientComponent(const gfx::Point& point) const override;
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override;
  bool CanFocus() override;
  void OnCaptureLost() override;
  void OnPaint(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowTargetVisibilityChanged(bool visible) override;
  bool HasHitTestMask() const override;
  void GetHitTestMask(gfx::Path* mask) const override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from aura::WindowTreeHostObserver:
  void OnHostCloseRequested(const aura::WindowTreeHost* host) override;

 private:
  mus::Window* window_;

  mojo::Shell* shell_;

  internal::NativeWidgetDelegate* native_widget_delegate_;

  const mus::mojom::SurfaceType surface_type_;
  ui::PlatformWindowState show_state_before_fullscreen_;

  // See class documentation for Widget in widget.h for a note about ownership.
  Widget::InitParams::Ownership ownership_;

  // Aura configuration.
  scoped_ptr<SurfaceContextFactory> context_factory_;
  scoped_ptr<WindowTreeHostMus> window_tree_host_;
  aura::Window* content_;
  scoped_ptr<wm::FocusController> focus_client_;
  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;
  scoped_ptr<aura::client::WindowTreeClient> window_tree_client_;
  base::WeakPtrFactory<NativeWidgetMus> close_widget_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMus);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_NATIVE_WIDGET_MUS_H_
