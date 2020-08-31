// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_
#define UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_

#include "base/macros.h"
#include "ui/base/x/x11_window.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/platform_window/extensions/workspace_extension.h"
#include "ui/platform_window/extensions/x11_extension.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_handler/wm_move_loop_handler.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/x11/x11_window_export.h"

namespace ui {

class X11ExtensionDelegate;
class X11DesktopWindowMoveClient;
class LocatedEvent;
class WorkspaceExtensionDelegate;

// Delegate interface used to communicate the X11PlatformWindow API client about
// XEvents of interest.
class X11_WINDOW_EXPORT XEventDelegate {
 public:
  virtual ~XEventDelegate() {}

  // TODO(crbug.com/990756): We need to implement/reuse ozone interface for
  // these.
  virtual void OnXWindowSelectionEvent(XEvent* xev) = 0;
  virtual void OnXWindowDragDropEvent(XEvent* xev) = 0;
};

// PlatformWindow implementation for X11. PlatformEvents are XEvents.
class X11_WINDOW_EXPORT X11Window : public PlatformWindow,
                                    public WmMoveResizeHandler,
                                    public XWindow,
                                    public PlatformEventDispatcher,
                                    public XEventDispatcher,
                                    public WorkspaceExtension,
                                    public X11Extension,
                                    public WmMoveLoopHandler {
 public:
  explicit X11Window(PlatformWindowDelegate* platform_window_delegate);
  ~X11Window() override;

  virtual void Initialize(PlatformWindowInitProperties properties);

  void SetXEventDelegate(XEventDelegate* delegate);

  // X11WindowManager calls this.
  // XWindow override:
  void OnXWindowLostCapture() override;

  void OnMouseEnter();

  gfx::AcceleratedWidget GetWidget() const;

  // PlatformWindow:
  void Show(bool inactive) override;
  void Hide() override;
  void Close() override;
  bool IsVisible() const override;
  void PrepareForShutdown() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetTitle(const base::string16& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void Activate() override;
  void Deactivate() override;
  void SetUseNativeFrame(bool use_native_frame) override;
  bool ShouldUseNativeFrame() const override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  void SetRestoredBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoredBoundsInPixels() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void SetZOrderLevel(ZOrderLevel order) override;
  ZOrderLevel GetZOrderLevel() const override;
  void StackAbove(gfx::AcceleratedWidget widget) override;
  void StackAtTop() override;
  void FlashFrame(bool flash_frame) override;
  void SetShape(std::unique_ptr<ShapeRects> native_shape,
                const gfx::Transform& transform) override;
  void SetAspectRatio(const gfx::SizeF& aspect_ratio) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void SizeConstraintsChanged() override;
  bool IsTranslucentWindowOpacitySupported() const override;
  void SetOpacity(float opacity) override;

  // WorkspaceExtension:
  std::string GetWorkspace() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool IsVisibleOnAllWorkspaces() const override;
  void SetWorkspaceExtensionDelegate(
      WorkspaceExtensionDelegate* delegate) override;

  // X11Extension:
  bool IsSyncExtensionAvailable() const override;
  bool IsWmTiling() const override;
  void OnCompleteSwapAfterResize() override;
  gfx::Rect GetXRootWindowOuterBounds() const override;
  bool ContainsPointInXRegion(const gfx::Point& point) const override;
  void LowerXWindow() override;
  void SetOverrideRedirect(bool override_redirect) override;
  void SetX11ExtensionDelegate(X11ExtensionDelegate* delegate) override;

  // Overridden from ui::XEventDispatcher:
  void CheckCanDispatchNextPlatformEvent(XEvent* xev) override;
  void PlatformEventDispatchFinished() override;
  PlatformEventDispatcher* GetPlatformEventDispatcher() override;
  bool DispatchXEvent(XEvent* event) override;

 protected:
  PlatformWindowDelegate* platform_window_delegate() const {
    return platform_window_delegate_;
  }

  bool is_shutting_down() const { return is_shutting_down_; }

  // XWindow:
  void OnXWindowCreated() override;

  virtual bool DispatchDraggingUiEvent(ui::Event* event);

  // XWindow:
  void OnXWindowStateChanged() override;
  void OnXWindowDamageEvent(const gfx::Rect& damage_rect) override;
  void OnXWindowBoundsChanged(const gfx::Rect& size) override;
  void OnXWindowCloseRequested() override;
  void OnXWindowIsActiveChanged(bool active) override;
  void OnXWindowWorkspaceChanged() override;
  void OnXWindowLostPointerGrab() override;
  void OnXWindowSelectionEvent(XEvent* xev) override;
  void OnXWindowDragDropEvent(XEvent* xev) override;
  base::Optional<gfx::Size> GetMinimumSizeForXWindow() override;
  base::Optional<gfx::Size> GetMaximumSizeForXWindow() override;
  void GetWindowMaskForXWindow(const gfx::Size& size,
                               SkPath* window_mask) override;

 private:
  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  void DispatchUiEvent(ui::Event* event, XEvent* xev);

  // WmMoveResizeHandler
  void DispatchHostWindowDragMovement(
      int hittest,
      const gfx::Point& pointer_location_in_px) override;

  // WmMoveLoopHandler:
  bool RunMoveLoop(const gfx::Vector2d& drag_offset) override;
  void EndMoveLoop() override;

  // Handles |xevent| as a Atk Key Event
  bool HandleAsAtkEvent(XEvent* xevent);

  // Adjusts |requested_size_in_pixels| to avoid the WM "feature" where setting
  // the window size to the monitor size causes the WM to set the EWMH for
  // fullscreen.
  gfx::Size AdjustSizeForDisplay(const gfx::Size& requested_size_in_pixels);

  // Converts the location of the |located_event| from the
  // |current_window_bounds| to the |target_window_bounds|.
  void ConvertEventLocationToTargetLocation(
      const gfx::Rect& target_window_bounds,
      const gfx::Rect& current_window_bounds,
      ui::LocatedEvent* located_event);

  // Stores current state of this window.
  PlatformWindowState state_ = PlatformWindowState::kUnknown;

  PlatformWindowDelegate* const platform_window_delegate_;

  XEventDelegate* x_event_delegate_ = nullptr;

  WorkspaceExtensionDelegate* workspace_extension_delegate_ = nullptr;

  X11ExtensionDelegate* x11_extension_delegate_ = nullptr;

  // Tells if the window got a ::Close call.
  bool is_shutting_down_ = false;

  // The z-order level of the window; the window exhibits "always on top"
  // behavior if > 0.
  ui::ZOrderLevel z_order_ = ui::ZOrderLevel::kNormal;

  // The bounds of our window before the window was maximized.
  gfx::Rect restored_bounds_in_pixels_;

  // Tells if this dispatcher can process next translated event based on a
  // previous check in ::CheckCanDispatchNextPlatformEvent based on a XID
  // target.
  XEvent* current_xevent_ = nullptr;

  std::unique_ptr<X11DesktopWindowMoveClient> x11_window_move_client_;

  DISALLOW_COPY_AND_ASSIGN(X11Window);
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_
