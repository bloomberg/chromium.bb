// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TOAST_SURFACE_H_
#define COMPONENTS_EXO_TOAST_SURFACE_H_

#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"

namespace exo {

class ToastSurfaceManager;

// Handles toast surface role of a given surface.
class ToastSurface : public ClientControlledShellSurface {
 public:
  ToastSurface(ToastSurfaceManager* manager,
               Surface* surface,
               bool default_scale_cancellation);

  ToastSurface(const ToastSurface&) = delete;
  ToastSurface& operator=(const ToastSurface&) = delete;

  ~ToastSurface() override;

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;

 private:
  ToastSurfaceManager* const manager_;
  bool added_to_manager_ = false;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_TOAST_SURFACE_H_
