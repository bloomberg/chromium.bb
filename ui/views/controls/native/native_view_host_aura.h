// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_
#define UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/views/controls/native/native_view_host_wrapper.h"
#include "ui/views/views_export.h"

namespace views {

class NativeViewHost;

// Aura implementation of NativeViewHostWrapper.
class VIEWS_EXPORT NativeViewHostAura : public NativeViewHostWrapper,
                                        public aura::WindowObserver {
 public:
  explicit NativeViewHostAura(NativeViewHost* host);
  virtual ~NativeViewHostAura();

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
  friend class NativeViewHostAuraTest;

  class ClippingWindowDelegate;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroying(aura::Window* window) override;
  virtual void OnWindowDestroyed(aura::Window* window) override;

  // Reparents the native view with the clipping window existing between it and
  // its old parent, so that the fast resize path works.
  void AddClippingWindow();

  // If the native view has been reparented via AddClippingWindow, this call
  // undoes it.
  void RemoveClippingWindow();

  // Our associated NativeViewHost.
  NativeViewHost* host_;

  scoped_ptr<ClippingWindowDelegate> clipping_window_delegate_;

  // Window that exists between the native view and the parent that allows for
  // clipping to occur. This is positioned in the coordinate space of
  // host_->GetWidget().
  aura::Window clipping_window_;
  scoped_ptr<gfx::Rect> clip_rect_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostAura);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_
