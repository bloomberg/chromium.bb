// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_NATIVE_MENU_HOST_DELEGATE_H_
#define VIEWS_CONTROLS_MENU_NATIVE_MENU_HOST_DELEGATE_H_

namespace views {
class MenuHost;
class RootView;
namespace internal {

class NativeMenuHostDelegate {
 public:
  virtual ~NativeMenuHostDelegate() {}

  // Called when the NativeMenuHost is being destroyed.
  virtual void OnNativeMenuHostDestroy() = 0;

  // Called when the NativeMenuHost is losing input capture.
  virtual void OnNativeMenuHostCancelCapture() = 0;

  // Pass-thrus for Widget overrides.
  // TODO(beng): Remove once MenuHost is-a Widget.
  virtual RootView* CreateRootView() = 0;
  virtual bool ShouldReleaseCaptureOnMouseRelease() const = 0;
};

}  // namespace internal
}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_NATIVE_MENU_HOST_DELEGATE_H_

