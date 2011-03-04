// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_NATIVE_WIDGET_DELEGATE_H_
#define VIEWS_WIDGET_NATIVE_WIDGET_DELEGATE_H_
#pragma once

namespace gfx {
class Size;
}

namespace views {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetDelegate
//
//  An interface implemented by the object that handles events sent by a
//  NativeWidget implementation.
//
class NativeWidgetDelegate {
 public:
  virtual ~NativeWidgetDelegate() {}

  // Called when native focus moves from one native view to another.
  virtual void OnNativeFocus(gfx::NativeView focused_view) = 0;
  virtual void OnNativeBlur(gfx::NativeView focused_view) = 0;

  // Called when the native widget is created.
  virtual void OnNativeWidgetCreated() = 0;

  // Called when the NativeWidget changed size to |new_size|.
  virtual void OnSizeChanged(const gfx::Size& new_size) = 0;

  // Returns true if the delegate has a FocusManager.
  virtual bool HasFocusManager() const = 0;
};

}  // namespace internal
}  // namespace views

#endif  // VIEWS_WIDGET_NATIVE_WIDGET_DELEGATE_H_
