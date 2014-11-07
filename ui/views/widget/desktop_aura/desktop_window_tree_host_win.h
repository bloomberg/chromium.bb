// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_WIN_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_WIN_H_

#include "ui/aura/window_tree_host.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"
#include "ui/views/win/hwnd_message_handler_delegate.h"
#include "ui/wm/public/animation_host.h"

namespace aura {
namespace client {
class DragDropClient;
class FocusClient;
class ScopedTooltipDisabler;
}
}

namespace views {
class DesktopCursorClient;
class DesktopDragDropClientWin;
class HWNDMessageHandler;

namespace corewm {
class TooltipWin;
}

class VIEWS_EXPORT DesktopWindowTreeHostWin
    : public DesktopWindowTreeHost,
      public aura::client::AnimationHost,
      public aura::WindowTreeHost,
      public ui::EventSource,
      public HWNDMessageHandlerDelegate {
 public:
  DesktopWindowTreeHostWin(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);
  virtual ~DesktopWindowTreeHostWin();

  // A way of converting an HWND into a content window.
  static aura::Window* GetContentWindowForHWND(HWND hwnd);

 protected:
  // Overridden from DesktopWindowTreeHost:
  virtual void Init(aura::Window* content_window,
                    const Widget::InitParams& params) override;
  virtual void OnNativeWidgetCreated(const Widget::InitParams& params) override;
  virtual scoped_ptr<corewm::Tooltip> CreateTooltip() override;
  virtual scoped_ptr<aura::client::DragDropClient>
      CreateDragDropClient(DesktopNativeCursorManager* cursor_manager) override;
  virtual void Close() override;
  virtual void CloseNow() override;
  virtual aura::WindowTreeHost* AsWindowTreeHost() override;
  virtual void ShowWindowWithState(ui::WindowShowState show_state) override;
  virtual void ShowMaximizedWithBounds(
      const gfx::Rect& restored_bounds) override;
  virtual bool IsVisible() const override;
  virtual void SetSize(const gfx::Size& size) override;
  virtual void StackAtTop() override;
  virtual void CenterWindow(const gfx::Size& size) override;
  virtual void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const override;
  virtual gfx::Rect GetWindowBoundsInScreen() const override;
  virtual gfx::Rect GetClientAreaBoundsInScreen() const override;
  virtual gfx::Rect GetRestoredBounds() const override;
  virtual gfx::Rect GetWorkAreaBoundsInScreen() const override;
  virtual void SetShape(gfx::NativeRegion native_region) override;
  virtual void Activate() override;
  virtual void Deactivate() override;
  virtual bool IsActive() const override;
  virtual void Maximize() override;
  virtual void Minimize() override;
  virtual void Restore() override;
  virtual bool IsMaximized() const override;
  virtual bool IsMinimized() const override;
  virtual bool HasCapture() const override;
  virtual void SetAlwaysOnTop(bool always_on_top) override;
  virtual bool IsAlwaysOnTop() const override;
  virtual void SetVisibleOnAllWorkspaces(bool always_visible) override;
  virtual bool SetWindowTitle(const base::string16& title) override;
  virtual void ClearNativeFocus() override;
  virtual Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;
  virtual void EndMoveLoop() override;
  virtual void SetVisibilityChangedAnimationsEnabled(bool value) override;
  virtual bool ShouldUseNativeFrame() const override;
  virtual bool ShouldWindowContentsBeTransparent() const override;
  virtual void FrameTypeChanged() override;
  virtual void SetFullscreen(bool fullscreen) override;
  virtual bool IsFullscreen() const override;
  virtual void SetOpacity(unsigned char opacity) override;
  virtual void SetWindowIcons(const gfx::ImageSkia& window_icon,
                              const gfx::ImageSkia& app_icon) override;
  virtual void InitModalType(ui::ModalType modal_type) override;
  virtual void FlashFrame(bool flash_frame) override;
  virtual void OnRootViewLayout() override;
  virtual void OnNativeWidgetFocus() override;
  virtual void OnNativeWidgetBlur() override;
  virtual bool IsAnimatingClosed() const override;
  virtual bool IsTranslucentWindowOpacitySupported() const override;
  virtual void SizeConstraintsChanged() override;

  // Overridden from aura::WindowTreeHost:
  virtual ui::EventSource* GetEventSource() override;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual gfx::Rect GetBounds() const override;
  virtual void SetBounds(const gfx::Rect& bounds) override;
  virtual gfx::Point GetLocationOnNativeScreen() const override;
  virtual void SetCapture() override;
  virtual void ReleaseCapture() override;
  virtual void SetCursorNative(gfx::NativeCursor cursor) override;
  virtual void OnCursorVisibilityChangedNative(bool show) override;
  virtual void MoveCursorToNative(const gfx::Point& location) override;

  // Overridden frm ui::EventSource
  virtual ui::EventProcessor* GetEventProcessor() override;

  // Overridden from aura::client::AnimationHost
  virtual void SetHostTransitionOffsets(
      const gfx::Vector2d& top_left_delta,
      const gfx::Vector2d& bottom_right_delta) override;
  virtual void OnWindowHidingAnimationCompleted() override;

  // Overridden from HWNDMessageHandlerDelegate:
  virtual bool IsWidgetWindow() const override;
  virtual bool IsUsingCustomFrame() const override;
  virtual void SchedulePaint() override;
  virtual void EnableInactiveRendering() override;
  virtual bool IsInactiveRenderingDisabled() override;
  virtual bool CanResize() const override;
  virtual bool CanMaximize() const override;
  virtual bool CanMinimize() const override;
  virtual bool CanActivate() const override;
  virtual bool WidgetSizeIsClientSize() const override;
  virtual bool IsModal() const override;
  virtual int GetInitialShowState() const override;
  virtual bool WillProcessWorkAreaChange() const override;
  virtual int GetNonClientComponent(const gfx::Point& point) const override;
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* path) override;
  virtual bool GetClientAreaInsets(gfx::Insets* insets) const override;
  virtual void GetMinMaxSize(gfx::Size* min_size,
                             gfx::Size* max_size) const override;
  virtual gfx::Size GetRootViewSize() const override;
  virtual void ResetWindowControls() override;
  virtual void PaintLayeredWindow(gfx::Canvas* canvas) override;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() override;
  virtual bool ShouldHandleSystemCommands() const override;
  virtual InputMethod* GetInputMethod() override;
  virtual void HandleAppDeactivated() override;
  virtual void HandleActivationChanged(bool active) override;
  virtual bool HandleAppCommand(short command) override;
  virtual void HandleCancelMode() override;
  virtual void HandleCaptureLost() override;
  virtual void HandleClose() override;
  virtual bool HandleCommand(int command) override;
  virtual void HandleAccelerator(const ui::Accelerator& accelerator) override;
  virtual void HandleCreate() override;
  virtual void HandleDestroying() override;
  virtual void HandleDestroyed() override;
  virtual bool HandleInitialFocus(ui::WindowShowState show_state) override;
  virtual void HandleDisplayChange() override;
  virtual void HandleBeginWMSizeMove() override;
  virtual void HandleEndWMSizeMove() override;
  virtual void HandleMove() override;
  virtual void HandleWorkAreaChanged() override;
  virtual void HandleVisibilityChanging(bool visible) override;
  virtual void HandleVisibilityChanged(bool visible) override;
  virtual void HandleClientSizeChanged(const gfx::Size& new_size) override;
  virtual void HandleFrameChanged() override;
  virtual void HandleNativeFocus(HWND last_focused_window) override;
  virtual void HandleNativeBlur(HWND focused_window) override;
  virtual bool HandleMouseEvent(const ui::MouseEvent& event) override;
  virtual bool HandleKeyEvent(const ui::KeyEvent& event) override;
  virtual bool HandleUntranslatedKeyEvent(const ui::KeyEvent& event) override;
  virtual void HandleTouchEvent(const ui::TouchEvent& event) override;
  virtual bool HandleIMEMessage(UINT message,
                                WPARAM w_param,
                                LPARAM l_param,
                                LRESULT* result) override;
  virtual void HandleInputLanguageChange(DWORD character_set,
                                         HKL input_language_id) override;
  virtual bool HandlePaintAccelerated(const gfx::Rect& invalid_rect) override;
  virtual void HandlePaint(gfx::Canvas* canvas) override;
  virtual bool HandleTooltipNotify(int w_param,
                                   NMHDR* l_param,
                                   LRESULT* l_result) override;
  virtual void HandleMenuLoop(bool in_menu_loop) override;
  virtual bool PreHandleMSG(UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT* result) override;
  virtual void PostHandleMSG(UINT message,
                             WPARAM w_param,
                             LPARAM l_param) override;
  virtual bool HandleScrollEvent(const ui::ScrollEvent& event) override;
  virtual void HandleWindowSizeChanging() override;

  Widget* GetWidget();
  const Widget* GetWidget() const;
  HWND GetHWND() const;

 private:
  void SetWindowTransparency();

  // Returns true if a modal window is active in the current root window chain.
  bool IsModalWindowActive() const;

  scoped_ptr<HWNDMessageHandler> message_handler_;
  scoped_ptr<aura::client::FocusClient> focus_client_;

  // TODO(beng): Consider providing an interface to DesktopNativeWidgetAura
  //             instead of providing this route back to Widget.
  internal::NativeWidgetDelegate* native_widget_delegate_;

  DesktopNativeWidgetAura* desktop_native_widget_aura_;

  aura::Window* content_window_;

  // Owned by DesktopNativeWidgetAura.
  DesktopDragDropClientWin* drag_drop_client_;

  // When certain windows are being shown, we augment the window size
  // temporarily for animation. The following two members contain the top left
  // and bottom right offsets which are used to enlarge the window.
  gfx::Vector2d window_expansion_top_left_delta_;
  gfx::Vector2d window_expansion_bottom_right_delta_;

  // Windows are enlarged to be at least 64x64 pixels, so keep track of the
  // extra added here.
  gfx::Vector2d window_enlargement_;

  // Whether the window close should be converted to a hide, and then actually
  // closed on the completion of the hide animation. This is cached because
  // the property is set on the contained window which has a shorter lifetime.
  bool should_animate_window_close_;

  // When Close()d and animations are being applied to this window, the close
  // of the window needs to be deferred to when the close animation is
  // completed. This variable indicates that a Close was converted to a Hide,
  // so that when the Hide is completed the host window should be closed.
  bool pending_close_;

  // True if the widget is going to have a non_client_view. We cache this value
  // rather than asking the Widget for the non_client_view so that we know at
  // Init time, before the Widget has created the NonClientView.
  bool has_non_client_view_;

  // Owned by TooltipController, but we need to forward events to it so we keep
  // a reference.
  corewm::TooltipWin* tooltip_;

  // Visibility of the cursor. On Windows we can have multiple root windows and
  // the implementation of ::ShowCursor() is based on a counter, so making this
  // member static ensures that ::ShowCursor() is always called exactly once
  // whenever the cursor visibility state changes.
  static bool is_cursor_visible_;

  scoped_ptr<aura::client::ScopedTooltipDisabler> tooltip_disabler_;

  // This flag is set to true in cases where we need to force a synchronous
  // paint via the compositor. Cases include restoring/resizing/maximizing the
  // window. Defaults to false.
  bool need_synchronous_paint_;

  // Set to true if we are about to enter a sizing loop.
  bool in_sizing_loop_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostWin);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_WIN_H_
