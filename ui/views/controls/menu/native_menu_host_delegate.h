// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_NATIVE_MENU_HOST_DELEGATE_H_
#define UI_VIEWS_CONTROLS_MENU_NATIVE_MENU_HOST_DELEGATE_H_
#pragma once
namespace views {

class MenuHost;

namespace internal {

class NativeWidgetDelegate;

class NativeMenuHostDelegate {
 public:
  virtual ~NativeMenuHostDelegate() {}

  // Called when the NativeMenuHost is being destroyed.
  virtual void OnNativeMenuHostDestroy() = 0;

  // Called when the NativeMenuHost is losing input capture.
  virtual void OnNativeMenuHostCancelCapture() = 0;

  virtual NativeWidgetDelegate* AsNativeWidgetDelegate() = 0;
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_NATIVE_MENU_HOST_DELEGATE_H_
