// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_VIEWS_H_
#define VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_VIEWS_H_
#pragma once

#include "base/basictypes.h"
#include "views/controls/native/native_view_host_wrapper.h"

namespace views {

class NativeViewHost;

// A Views implementation of NativeViewHostWrapper
class NativeViewHostViews : public NativeViewHostWrapper {
 public:
  explicit NativeViewHostViews(NativeViewHost* host);
  virtual ~NativeViewHostViews();

  // Overridden from NativeViewHostWrapper:
  virtual void NativeViewAttached();
  virtual void NativeViewDetaching(bool destroyed);
  virtual void AddedToWidget();
  virtual void RemovedFromWidget();
  virtual void InstallClip(int x, int y, int w, int h);
  virtual bool HasInstalledClip();
  virtual void UninstallClip();
  virtual void ShowWidget(int x, int y, int w, int h);
  virtual void HideWidget();
  virtual void SetFocus();
  virtual gfx::NativeViewAccessible GetNativeViewAccessible();

 private:
  // Our associated NativeViewHost.
  NativeViewHost* host_;

  // Have we installed a clip region?
  bool installed_clip_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewHostViews);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_NATIVE_NATIVE_VIEW_HOST_VIEWS_H_
