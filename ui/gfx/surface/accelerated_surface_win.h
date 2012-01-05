// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SURFACE_ACCELERATED_SURFACE_WIN_H_
#define UI_GFX_SURFACE_ACCELERATED_SURFACE_WIN_H_
#pragma once

#include "base/callback_forward.h"
#include "base/memory/linked_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gfx/surface/surface_export.h"

class AcceleratedPresenter;

class SURFACE_EXPORT AcceleratedSurface {
 public:
  AcceleratedSurface();
  ~AcceleratedSurface();

  // Schedule a frame to be presented. The completion callback will be invoked
  // when it is safe to write to the surface on another thread. The lock for
  // this surface will be held while the completion callback runs.
  void AsyncPresentAndAcknowledge(HWND window,
                                  const gfx::Size& size,
                                  int64 surface_id,
                                  const base::Closure& completion_task);

  // Synchronously present a frame with no acknowledgement.
  bool Present(HWND window);

 private:
  linked_ptr<AcceleratedPresenter> presenter_;
  DISALLOW_COPY_AND_ASSIGN(AcceleratedSurface);
};

#endif  // UI_GFX_SURFACE_ACCELERATED_SURFACE_WIN_H_
