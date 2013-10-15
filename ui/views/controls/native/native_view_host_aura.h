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
  virtual void AttachNativeView() OVERRIDE;
  virtual void NativeViewDetaching(bool destroyed) OVERRIDE;
  virtual void AddedToWidget() OVERRIDE;
  virtual void RemovedFromWidget() OVERRIDE;
  virtual void InstallClip(int x, int y, int w, int h) OVERRIDE;
  virtual bool HasInstalledClip() OVERRIDE;
  virtual void UninstallClip() OVERRIDE;
  virtual void ShowWidget(int x, int y, int w, int h) OVERRIDE;
  virtual void HideWidget() OVERRIDE;
  virtual void SetFocus() OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;

 private:
  friend class NativeViewHostAuraTest;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

  // Calculates the origin of the native view for the fast resize path, so it
  // can be clipped correctly. This value is affected by the current gravity
  // being applied. The formula for this calulation is:
  //
  // x' = width_scaling_factor * (content_width - clip_width)
  // y' = height_scaling_factor * (content_heigth - clip_height)
  //
  // , where the scaling factors are either 0, 0.5, or 1.0 depending on the
  // current gravity.
  gfx::Point CalculateNativeViewOrigin(const gfx::Rect& input_rect,
                                       const gfx::Rect& native_rect) const;

  // Reparents the native view with the clipping window existing between it and
  // its old parent, so that the fast resize path works.
  void AddClippingWindow();

  // If the native view has been reparented via AddClippingWindow, this call
  // undoes it.
  void RemoveClippingWindow();

  // Update the positioning of the clipping window and the attached native
  // view. The native view is a child of the clipping window, so is positioned
  // relative to it.
  void UpdateClippingWindow();

  // Our associated NativeViewHost.
  NativeViewHost* host_;

  // Have we installed a clip region?
  bool installed_clip_;

  // Window that exists between the native view and the parent that allows for
  // clipping to occur.
  aura::Window clipping_window_;

  // The bounds of the content layer before any clipping actions occur. This is
  // restored when the clip is uninstalled. This value should be in the
  // coordinate space of host_->GetWidget().
  gfx::Rect orig_bounds_;

  // The bounds of the clipping window. When no clip is installed |orig_bounds_|
  // and |clip_rect_| should be equal. When clip is installed |clip_rect_| is
  // manipulated to position of the clipping window in
  // UpdateClippingWindow. This value should be in the coordinate space of
  // host_->GetWidget().
  gfx::Rect clip_rect_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostAura);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_AURA_H_
