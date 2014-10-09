// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_MAC_H_
#define UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_MAC_H_

#include "base/macros.h"
#include "ui/views/controls/native/native_view_host_wrapper.h"
#include "ui/views/views_export.h"

namespace views {

class NativeViewHost;

// Mac implementation of NativeViewHostWrapper.
class VIEWS_EXPORT NativeViewHostMac : public NativeViewHostWrapper {
 public:
  explicit NativeViewHostMac(NativeViewHost* host);
  virtual ~NativeViewHostMac();

  // Overridden from NativeViewHostWrapper:
  virtual void AttachNativeView() override;
  virtual void NativeViewDetaching(bool destroyed) override;
  virtual void AddedToWidget() override;
  virtual void RemovedFromWidget() override;
  virtual void InstallClip(int x, int y, int w, int h) override;
  virtual bool HasInstalledClip() override;
  virtual void UninstallClip() override;
  virtual void ShowWidget(int x, int y, int w, int h) override;
  virtual void HideWidget() override;
  virtual void SetFocus() override;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() override;
  virtual gfx::NativeCursor GetCursor(int x, int y) override;

 private:
  // Our associated NativeViewHost. Owns this.
  NativeViewHost* host_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostMac);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_
