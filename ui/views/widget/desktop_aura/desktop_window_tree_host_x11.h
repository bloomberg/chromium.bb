// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_X11_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_X11_H_

#include <X11/extensions/shape.h>
#include <X11/Xlib.h>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor_loader_x11.h"
#include "ui/events/event_source.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"

namespace gfx {
class ImageSkia;
class ImageSkiaRep;
}

namespace ui {
class EventHandler;
}

namespace views {
class DesktopDragDropClientAuraX11;
class DesktopDispatcherClient;
class DesktopWindowTreeHostObserverX11;
class X11DesktopWindowMoveClient;
class X11WindowEventFilter;

class VIEWS_EXPORT DesktopWindowTreeHostX11
    : public DesktopWindowTreeHost,
      public aura::WindowTreeHost,
      public ui::EventSource,
      public ui::PlatformEventDispatcher {
 public:
  DesktopWindowTreeHostX11(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);
  virtual ~DesktopWindowTreeHostX11();

  // A way of converting an X11 |xid| host window into a |content_window_|.
  static aura::Window* GetContentWindowForXID(XID xid);

  // A way of converting an X11 |xid| host window into this object.
  static DesktopWindowTreeHostX11* GetHostForXID(XID xid);

  // Get all open top-level windows. This includes windows that may not be
  // visible. This list is sorted in their stacking order, i.e. the first window
  // is the topmost window.
  static std::vector<aura::Window*> GetAllOpenWindows();

  // Returns the current bounds in terms of the X11 Root Window.
  gfx::Rect GetX11RootWindowBounds() const;

  // Returns the current bounds in terms of the X11 Root Window including the
  // borders provided by the window manager (if any).
  gfx::Rect GetX11RootWindowOuterBounds() const;

  // Returns the window shape if the window is not rectangular. Returns NULL
  // otherwise.
  ::Region GetWindowShape() const;

  // Called by X11DesktopHandler to notify us that the native windowing system
  // has changed our activation.
  void HandleNativeWidgetActivationChanged(bool active);

  void AddObserver(views::DesktopWindowTreeHostObserverX11* observer);
  void RemoveObserver(views::DesktopWindowTreeHostObserverX11* observer);

  // Swaps the current handler for events in the non client view with |handler|.
  void SwapNonClientEventHandler(scoped_ptr<ui::EventHandler> handler);

  // Deallocates the internal list of open windows.
  static void CleanUpWindowList();

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
  virtual void SetSize(const gfx::Size& requested_size) override;
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
  virtual void SetBounds(const gfx::Rect& requested_bounds) override;
  virtual gfx::Point GetLocationOnNativeScreen() const override;
  virtual void SetCapture() override;
  virtual void ReleaseCapture() override;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) override;
  virtual void SetCursorNative(gfx::NativeCursor cursor) override;
  virtual void MoveCursorToNative(const gfx::Point& location) override;
  virtual void OnCursorVisibilityChangedNative(bool show) override;

  // Overridden frm ui::EventSource
  virtual ui::EventProcessor* GetEventProcessor() override;

 private:
  // Initializes our X11 surface to draw on. This method performs all
  // initialization related to talking to the X11 server.
  void InitX11Window(const Widget::InitParams& params);

  // Creates an aura::WindowEventDispatcher to contain the |content_window|,
  // along with all aura client objects that direct behavior.
  aura::WindowEventDispatcher* InitDispatcher(const Widget::InitParams& params);

  // Adjusts |requested_size| to avoid the WM "feature" where setting the
  // window size to the monitor size causes the WM to set the EWMH for
  // fullscreen.
  gfx::Size AdjustSize(const gfx::Size& requested_size);

  // Called when |xwindow_|'s _NET_WM_STATE property is updated.
  void OnWMStateUpdated();

  // Called when |xwindow_|'s _NET_FRAME_EXTENTS property is updated.
  void OnFrameExtentsUpdated();

  // Updates |xwindow_|'s minimum and maximum size.
  void UpdateMinAndMaxSize();

  // Updates |xwindow_|'s _NET_WM_USER_TIME if |xwindow_| is active.
  void UpdateWMUserTime(const ui::PlatformEvent& event);

  // Sends a message to the x11 window manager, enabling or disabling the
  // states |state1| and |state2|.
  void SetWMSpecState(bool enabled, ::Atom state1, ::Atom state2);

  // Checks if the window manager has set a specific state.
  bool HasWMSpecProperty(const char* property) const;

  // Sets whether the window's borders are provided by the window manager.
  void SetUseNativeFrame(bool use_native_frame);

  // Dispatches a mouse event, taking mouse capture into account. If a
  // different host has capture, we translate the event to its coordinate space
  // and dispatch it to that host instead.
  void DispatchMouseEvent(ui::MouseEvent* event);

  // Dispatches a touch event, taking capture into account. If a different host
  // has capture, then touch-press events are translated to its coordinate space
  // and dispatched to that host instead.
  void DispatchTouchEvent(ui::TouchEvent* event);

  // Resets the window region for the current widget bounds if necessary.
  void ResetWindowRegion();

  // Serializes an image to the format used by _NET_WM_ICON.
  void SerializeImageRepresentation(const gfx::ImageSkiaRep& rep,
                                    std::vector<unsigned long>* data);

  // Returns an 8888 ARGB visual. Can return NULL if there is no matching
  // visual on this display.
  Visual* GetARGBVisual();

  // See comment for variable open_windows_.
  static std::list<XID>& open_windows();

  // Map the window (shows it) taking into account the given |show_state|.
  void MapWindow(ui::WindowShowState show_state);

  void SetWindowTransparency();

  // Relayout the widget's client and non-client views.
  void Relayout();

  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  void DelayedResize(const gfx::Size& size);

  // X11 things
  // The display and the native X window hosting the root window.
  XDisplay* xdisplay_;
  ::Window xwindow_;

  // The native root window.
  ::Window x_root_window_;

  ui::X11AtomCache atom_cache_;

  // Is the window mapped to the screen?
  bool window_mapped_;

  // The bounds of |xwindow_|.
  gfx::Rect bounds_;

  // Whenever the bounds are set, we keep the previous set of bounds around so
  // we can have a better chance of getting the real |restored_bounds_|. Window
  // managers tend to send a Configure message with the maximized bounds, and
  // then set the window maximized property. (We don't rely on this for when we
  // request that the window be maximized, only when we detect that some other
  // process has requested that we become the maximized window.)
  gfx::Rect previous_bounds_;

  // The bounds of our window before we were maximized.
  gfx::Rect restored_bounds_;

  // |xwindow_|'s minimum size.
  gfx::Size min_size_;

  // |xwindow_|'s maximum size.
  gfx::Size max_size_;

  // The window manager state bits.
  std::set< ::Atom> window_properties_;

  // Whether |xwindow_| was requested to be fullscreen via SetFullscreen().
  bool is_fullscreen_;

  // True if the window should stay on top of most other windows.
  bool is_always_on_top_;

  // True if the window has title-bar / borders provided by the window manager.
  bool use_native_frame_;

  // True if a Maximize() call should be done after mapping the window.
  bool should_maximize_after_map_;

  // Whether we used an ARGB visual for our window.
  bool use_argb_visual_;

  DesktopDragDropClientAuraX11* drag_drop_client_;

  scoped_ptr<ui::EventHandler> x11_non_client_event_filter_;
  scoped_ptr<X11DesktopWindowMoveClient> x11_window_move_client_;

  // TODO(beng): Consider providing an interface to DesktopNativeWidgetAura
  //             instead of providing this route back to Widget.
  internal::NativeWidgetDelegate* native_widget_delegate_;

  DesktopNativeWidgetAura* desktop_native_widget_aura_;

  aura::Window* content_window_;

  // We can optionally have a parent which can order us to close, or own
  // children who we're responsible for closing when we CloseNow().
  DesktopWindowTreeHostX11* window_parent_;
  std::set<DesktopWindowTreeHostX11*> window_children_;

  ObserverList<DesktopWindowTreeHostObserverX11> observer_list_;

  // The window shape if the window is non-rectangular.
  ::Region window_shape_;

  // Whether |window_shape_| was set via SetShape().
  bool custom_window_shape_;

  // The size of the window manager provided borders (if any).
  gfx::Insets native_window_frame_borders_;

  // The current DesktopWindowTreeHostX11 which has capture. Set synchronously
  // when capture is requested via SetCapture().
  static DesktopWindowTreeHostX11* g_current_capture;

  // A list of all (top-level) windows that have been created but not yet
  // destroyed.
  static std::list<XID>* open_windows_;

  base::string16 window_title_;

  // Whether we currently are flashing our frame. This feature is implemented
  // by setting the urgency hint with the window manager, which can draw
  // attention to the window or completely ignore the hint. We stop flashing
  // the frame when |xwindow_| gains focus or handles a mouse button event.
  bool urgency_hint_set_;

  base::CancelableCallback<void()> delayed_resize_task_;

  base::WeakPtrFactory<DesktopWindowTreeHostX11> close_widget_factory_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostX11);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_X11_H_
