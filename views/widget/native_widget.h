// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_NATIVE_WIDGET_H_
#define VIEWS_WIDGET_NATIVE_WIDGET_H_
#pragma once

#include <set>

#include "ui/gfx/native_widget_types.h"
#include "views/widget/widget.h"

namespace gfx {
class Rect;
}

namespace ui {
class OSExchangeData;
}

namespace views {

class InputMethod;
class TooltipManager;

////////////////////////////////////////////////////////////////////////////////
// NativeWidget interface
//
//  An interface implemented by an object that encapsulates rendering, event
//  handling and widget management provided by an underlying native toolkit.
//
class NativeWidget {
 public:
  typedef std::set<NativeWidget*> NativeWidgets;

  virtual ~NativeWidget() {}

  // Creates an appropriate default NativeWidget implementation for the current
  // OS/circumstance.
  static NativeWidget* CreateNativeWidget(
      internal::NativeWidgetDelegate* delegate);

  // Retrieves the NativeWidget implementation associated with the given
  // NativeView or Window, or NULL if the supplied handle has no associated
  // NativeView.
  static NativeWidget* GetNativeWidgetForNativeView(
      gfx::NativeView native_view);
  static NativeWidget* GetNativeWidgetForNativeWindow(
      gfx::NativeWindow native_window);

  // Retrieves the top NativeWidget in the hierarchy containing the given
  // NativeView, or NULL if there is no NativeWidget that contains it.
  static NativeWidget* GetTopLevelNativeWidget(gfx::NativeView native_view);

  // Returns all NativeWidgets in |native_view|'s hierarchy, including itself if
  // it is one.
  static void GetAllNativeWidgets(gfx::NativeView native_view,
                                  NativeWidgets* children);

  // Reparent a NativeView and notify all NativeWidgets in
  // |native_view|'s hierarchy of the change.
  static void ReparentNativeView(gfx::NativeView native_view,
                                 gfx::NativeView new_parent);

  // Initializes the NativeWidget.
  virtual void InitNativeWidget(const Widget::InitParams& params) = 0;

  // Returns the Widget associated with this NativeWidget. This function is
  // guaranteed to return non-NULL for the lifetime of the NativeWidget.
  virtual Widget* GetWidget() = 0;
  virtual const Widget* GetWidget() const = 0;

  // Returns the NativeView/Window associated with this NativeWidget.
  virtual gfx::NativeView GetNativeView() const = 0;
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Returns the enclosing Window, or NULL if there is no enclosing Window.
  virtual Window* GetContainingWindow() = 0;
  virtual const Window* GetContainingWindow() const = 0;

  // Notifies the NativeWidget that a view was removed from the Widget's view
  // hierarchy.
  virtual void ViewRemoved(View* view) = 0;

  // Sets/Gets a native window property on the underlying native window object.
  // Returns NULL if the property does not exist. Setting the property value to
  // NULL removes the property.
  virtual void SetNativeWindowProperty(const char* name, void* value) = 0;
  virtual void* GetNativeWindowProperty(const char* name) = 0;

  // Returns the native widget's tooltip manager. Called from the View hierarchy
  // to update tooltips.
  virtual TooltipManager* GetTooltipManager() const = 0;

  // Returns true if a system screen reader is active for the NativeWidget.
  virtual bool IsScreenReaderActive() const = 0;

  // Notify native Accessibility clients of an event.
  virtual void SendNativeAccessibilityEvent(
      View* view,
      ui::AccessibilityTypes::Event event_type) = 0;

  // Sets or releases event capturing for this native widget.
  virtual void SetMouseCapture() = 0;
  virtual void ReleaseMouseCapture() = 0;

  // Returns true if this native widget is capturing all events.
  virtual bool HasMouseCapture() const = 0;

  // Returns true if any mouse button is currently pressed.
  virtual bool IsMouseButtonDown() const = 0;

  // Returns the InputMethod for this native widget.
  // Note that all widgets in a widget hierarchy share the same input method.
  // TODO(suzhe): rename to GetInputMethod() when NativeWidget implementation
  // class doesn't inherit Widget anymore.
  virtual InputMethod* GetInputMethodNative() = 0;

  // Sets a different InputMethod instance to this native widget. The instance
  // must not be initialized, the ownership will be assumed by the native
  // widget. It's only for testing purpose.
  virtual void ReplaceInputMethod(InputMethod* input_method) = 0;

 protected:
  friend class Widget;

  // Returns a handle for the underlying native widget that can be used for
  // accelerated drawing.
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() = 0;

  // Widget pass-thrus, private to Views. --------------------------------------
  // See method documentation in Widget.
  virtual gfx::Rect GetWindowScreenBounds() const = 0;
  virtual gfx::Rect GetClientAreaScreenBounds() const = 0;
  virtual void SetBounds(const gfx::Rect& bounds) = 0;
  virtual void SetSize(const gfx::Size& size) = 0;
  virtual void MoveAbove(gfx::NativeView native_view) = 0;
  virtual void SetShape(gfx::NativeRegion shape) = 0;
  virtual void Close() = 0;
  virtual void CloseNow() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void SetOpacity(unsigned char opacity) = 0;
  virtual void SetAlwaysOnTop(bool on_top) = 0;
  virtual bool IsVisible() const = 0;
  virtual bool IsActive() const = 0;
  virtual bool IsAccessibleWidget() const = 0;
  virtual bool ContainsNativeView(gfx::NativeView native_view) const = 0;
  virtual void RunShellDrag(View* view,
                            const ui::OSExchangeData& data,
                            int operation) = 0;
  virtual void SchedulePaintInRect(const gfx::Rect& rect) = 0;
  virtual void SetCursor(gfx::NativeCursor cursor) = 0;
};

}  // namespace views

#endif  // VIEWS_WIDGET_NATIVE_WIDGET_H_
