// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_NATIVE_WIDGET_H_
#define VIEWS_WIDGET_NATIVE_WIDGET_H_
#pragma once

#include <set>

namespace gfx {
class Rect;
}

namespace views {

class TooltipManager;
class Widget;

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

  // Returns the Widget associated with this NativeWidget. This function is
  // guaranteed to return non-NULL for the lifetime of the NativeWidget.
  virtual Widget* GetWidget() = 0;

  // Sets/Gets a native window property on the underlying native window object.
  // Returns NULL if the property does not exist. Setting the property value to
  // NULL removes the property.
  virtual void SetNativeWindowProperty(const char* name, void* value) = 0;
  virtual void* GetNativeWindowProperty(const char* name) = 0;

  // Returns the native widget's tooltip manager. Called from the View hierarchy
  // to update tooltips.
  virtual TooltipManager* GetTooltipManager() const = 0;

  // Widget pass-thrus ---------------------------------------------------------
  // TODO(beng): Investigate if we can move these to a NativeWidgetPrivate
  //             interface.

  // See method documentation in Widget:
  virtual gfx::Rect GetWindowScreenBounds() const = 0;
  virtual gfx::Rect GetClientAreaScreenBounds() const = 0;
  virtual void SetBounds(const gfx::Rect& bounds) = 0;
  virtual void MoveAbove(Widget* widget) = 0;
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
