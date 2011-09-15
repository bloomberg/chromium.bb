// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_NATIVE_WIDGET_WAYLAND_H_
#define VIEWS_WIDGET_NATIVE_WIDGET_WAYLAND_H_
#pragma once

#include <wayland-client.h>

#include "base/memory/scoped_vector.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/size.h"
#include "ui/wayland/wayland_widget.h"
#include "views/widget/native_widget_private.h"
#include "views/widget/widget.h"

typedef struct _cairo_device cairo_device_t;
typedef struct _cairo_surface cairo_surface_t;

namespace gfx {
class Rect;
}

namespace ui {
class ViewProp;
class WaylandDisplay;
class WaylandInputDevice;
class WaylandWindow;
}

namespace views {

namespace internal {
class NativeWidgetDelegate;
}

// Widget implementation for Wayland
class NativeWidgetWayland : public internal::NativeWidgetPrivate,
                            public ui::CompositorDelegate,
                            public ui::WaylandWidget {
 public:
  explicit NativeWidgetWayland(internal::NativeWidgetDelegate* delegate);
  virtual ~NativeWidgetWayland();

  // Overridden from NativeWidget:
  virtual void InitNativeWidget(const Widget::InitParams& params) OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView() OVERRIDE;
  virtual void UpdateFrameAfterFrameChange() OVERRIDE;
  virtual bool ShouldUseNativeFrame() const OVERRIDE;
  virtual void FrameTypeChanged() OVERRIDE;
  virtual Widget* GetWidget() OVERRIDE;
  virtual const Widget* GetWidget() const OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual Widget* GetTopLevelWidget() OVERRIDE;
  virtual const ui::Compositor* GetCompositor() const OVERRIDE;
  virtual ui::Compositor* GetCompositor() OVERRIDE;
  virtual void CalculateOffsetToAncestorWithLayer(
      gfx::Point* offset,
      ui::Layer** layer_parent) OVERRIDE;
  virtual void ViewRemoved(View* view) OVERRIDE;
  virtual void SetNativeWindowProperty(const char* name, void* value) OVERRIDE;
  virtual void* GetNativeWindowProperty(const char* name) const OVERRIDE;
  virtual TooltipManager* GetTooltipManager() const OVERRIDE;
  virtual bool IsScreenReaderActive() const OVERRIDE;
  virtual void SendNativeAccessibilityEvent(
      View* view,
      ui::AccessibilityTypes::Event event_type) OVERRIDE;
  virtual void SetMouseCapture() OVERRIDE;
  virtual void ReleaseMouseCapture() OVERRIDE;
  virtual bool HasMouseCapture() const OVERRIDE;
  virtual InputMethod* CreateInputMethod() OVERRIDE;
  virtual void CenterWindow(const gfx::Size& size) OVERRIDE;
  virtual void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE;
  virtual void SetWindowTitle(const std::wstring& title) OVERRIDE;
  virtual void SetWindowIcons(const SkBitmap& window_icon,
                              const SkBitmap& app_icon) OVERRIDE;
  virtual void SetAccessibleName(const std::wstring& name) OVERRIDE;
  virtual void SetAccessibleRole(ui::AccessibilityTypes::Role role) OVERRIDE;
  virtual void SetAccessibleState(ui::AccessibilityTypes::State state) OVERRIDE;
  virtual void BecomeModal() OVERRIDE;
  virtual gfx::Rect GetWindowScreenBounds() const OVERRIDE;
  virtual gfx::Rect GetClientAreaScreenBounds() const OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBoundsConstrained(const gfx::Rect& bounds,
                                    Widget* other_widget) OVERRIDE;
  virtual void MoveAbove(gfx::NativeView native_view) OVERRIDE;
  virtual void MoveToTop() OVERRIDE;
  virtual void SetShape(gfx::NativeRegion shape) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void CloseNow() OVERRIDE;
  virtual void EnableClose(bool enable) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ShowMaximizedWithBounds(
      const gfx::Rect& restored_bounds) OVERRIDE;
  virtual void ShowWithWindowState(ui::WindowShowState window_state) OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual void SetAlwaysOnTop(bool always_on_top) OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual void SetFullscreen(bool fullscreen) OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual void SetOpacity(unsigned char opacity) OVERRIDE;
  virtual void SetUseDragFrame(bool use_drag_frame) OVERRIDE;
  virtual bool IsAccessibleWidget() const OVERRIDE;
  virtual void RunShellDrag(View* view,
                            const ui::OSExchangeData& data,
                            int operation) OVERRIDE;
  virtual void SchedulePaintInRect(const gfx::Rect& rect) OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual void ClearNativeFocus() OVERRIDE;
  virtual void FocusNativeView(gfx::NativeView native_view) OVERRIDE;
  virtual bool ConvertPointFromAncestor(
      const Widget* ancestor, gfx::Point* point) const OVERRIDE;

  virtual void OnMotionNotify(ui::WaylandEvent event) OVERRIDE;
  virtual void OnButtonNotify(ui::WaylandEvent event) OVERRIDE;
  virtual void OnKeyNotify(ui::WaylandEvent event) OVERRIDE;
  virtual void OnPointerFocus(ui::WaylandEvent event) OVERRIDE;
  virtual void OnKeyboardFocus(ui::WaylandEvent event) OVERRIDE;

  virtual void OnGeometryChange(ui::WaylandEvent event) OVERRIDE;

 private:
  typedef ScopedVector<ui::ViewProp> ViewProps;

  // Overridden from ui::CompositorDelegate
  virtual void ScheduleCompositorPaint();

  // Overridden from NativeWidget
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;

  // Overridden from internal::InputMethodDelegate
  virtual void DispatchKeyEventPostIME(const KeyEvent& key) OVERRIDE;

  void OnPaint(gfx::Rect damage_area);

  static gboolean IdleRedraw(void* ptr);

  // A delegate implementation that handles events received here.
  // See class documentation for Widget in widget.h for a note about ownership.
  internal::NativeWidgetDelegate* delegate_;

  scoped_ptr<TooltipManager> tooltip_manager_;

  // The following factory is used to delay destruction.
  ScopedRunnableMethodFactory<NativeWidgetWayland> close_widget_factory_;

  // See class documentation for Widget in widget.h for a note about ownership.
  Widget::InitParams::Ownership ownership_;

  // Keeps track of mause capture for this widget.
  bool has_mouse_capture_;

  // Current window allocation
  gfx::Rect allocation_;
  // Previous allocation. Used to restore the size and location.
  gfx::Rect saved_allocation_;

  // The compositor for accelerated drawing.
  scoped_refptr<ui::Compositor> compositor_;

  ViewProps props_;

  // Pointer to the Wayland display. This object doesn't own the pointer.
  ui::WaylandDisplay* wayland_display_;

  // Wayland window associated with this widget.
  ui::WaylandWindow* wayland_window_;

  // The accelerated surface associated with a Wayland window.
  struct wl_egl_window* egl_window_;

  cairo_device_t* device_;

  // Cairo surface associated with the Wayland accelerated surface. This is
  // used when we're not using the accelerated painting path.
  cairo_surface_t* cairo_surface_;
  const cairo_user_data_key_t surface_data_key_;

  // Used to accumulate damaged area between repaints.
  // Necessary since Wayland seems to expect at most one paint per frame.
  gfx::Rect damage_area_;

  // The GL surface and context used to render when we're using unaccelerated
  // rendering. If we're using accelerated rendering, we'll have a compositor
  // and the compositor will have these, so we don't need to worry about them.
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_refptr<gfx::GLContext> context_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetWayland);
};

}  // namespace views

#endif  // VIEWS_WIDGET_NATIVE_WIDGET_WAYLAND_H_
