// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_NATIVE_WIDGET_H_
#define UI_VIEWS_WIDGET_NATIVE_WIDGET_H_

#include "ui/views/native_types.h"

namespace gfx{
class Path;
class Rect;
}

namespace ui {
namespace internal {
class NativeWidgetListener;
}
class View;
class Widget;

////////////////////////////////////////////////////////////////////////////////
// NativeWidget interface
//
//  An interface implemented by an object that encapsulates rendering, event
//  handling and widget management provided by an underlying native toolkit.
//
class NativeWidget {
 public:
  virtual ~NativeWidget() {}

  static NativeWidget* CreateNativeWidget(
      internal::NativeWidgetListener* listener);

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

  // See Widget for documentation and notes.
  virtual void InitWithNativeViewParent(gfx::NativeView parent,
                                        const gfx::Rect& bounds) = 0;
  virtual void InitWithWidgetParent(Widget* parent,
                                    const gfx::Rect& bounds) = 0;
  virtual void InitWithViewParent(View* parent, const gfx::Rect& bounds) = 0;
  virtual void SetNativeWindowProperty(const char* name, void* value) = 0;
  virtual void* GetNativeWindowProperty(const char* name) const = 0;
  virtual gfx::Rect GetWindowScreenBounds() const = 0;
  virtual gfx::Rect GetClientAreaScreenBounds() const = 0;
  virtual void SetBounds(const gfx::Rect& bounds) = 0;
  virtual void SetShape(const gfx::Path& shape) = 0;
  virtual gfx::NativeView GetNativeView() const = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void Close() = 0;
  virtual void MoveAbove(NativeWidget* other) = 0;
  virtual void SetAlwaysOnTop(bool always_on_top) = 0;
  virtual bool IsVisible() const = 0;
  virtual bool IsActive() const = 0;

  virtual void SetMouseCapture() = 0;
  virtual void ReleaseMouseCapture() = 0;
  virtual bool HasMouseCapture() const = 0;
  virtual bool ShouldReleaseCaptureOnMouseReleased() const = 0;

  virtual void Invalidate() = 0;
  virtual void InvalidateRect(const gfx::Rect& invalid_rect) = 0;
  virtual void Paint() = 0;
};

}  // namespace ui

#endif  // UI_VIEWS_WIDGET_NATIVE_WIDGET_H_

