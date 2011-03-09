// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_NATIVE_WINDOW_DELEGATE_H_
#define VIEWS_WIDGET_NATIVE_WINDOW_DELEGATE_H_
#pragma once

namespace views {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// NativeWindowDelegate interface
//
//  An interface implemented by an object that receives notifications from a
//  NativeWindow implementation.
//
class NativeWindowDelegate {
 public:
  virtual ~NativeWindowDelegate() {}

  // Returns true if the window is modal.
  virtual bool IsModal() const = 0;

  // Called just after the NativeWindow has been created.
  virtual void OnNativeWindowCreated(const gfx::Rect& bounds) = 0;

  // Called just before the native window is destroyed. This is the delegate's
  // last chance to do anything with the native window handle.
  virtual void OnWindowDestroying() = 0;

  // Called just after the native window is destroyed.
  virtual void OnWindowDestroyed() = 0;
};

}  // namespace internal
}  // namespace views

#endif  // VIEWS_WIDGET_NATIVE_WINDOW_DELEGATE_H_
