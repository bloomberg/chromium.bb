// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_H_

#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <lib/ui/scenic/cpp/resources.h>
#include <lib/ui/scenic/cpp/session.h>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/system/handle.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/public/platform_window_surface.h"

namespace ui {

class ScenicSurfaceFactory;

// Holder for Scenic resources backing rendering surface.
//
// This object creates some simple Scenic resources for containing a window's
// texture, and attaches them to the parent View (by sending an IPC to the
// browser process).
//
// The texture is updated through an image pipe.
class ScenicSurface : public ui::PlatformWindowSurface {
 public:
  ScenicSurface(
      ScenicSurfaceFactory* scenic_surface_factory,
      gfx::AcceleratedWidget window,
      scenic::SessionPtrAndListenerRequest sesion_and_listener_request);
  ~ScenicSurface() override;

  // Sets the texture of the surface to a new image pipe.
  void SetTextureToNewImagePipe(
      fidl::InterfaceRequest<fuchsia::images::ImagePipe> image_pipe_request);

  // Sets the texture of the surface to an image resource.
  void SetTextureToImage(const scenic::Image& image);

  // Creates token to links the surface to the window in the browser process.
  mojo::ScopedHandle CreateParentExportToken();

  void AssertBelongsToCurrentThread() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  scenic::Session* scenic_session() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return &scenic_session_;
  }

 private:
  scenic::Session scenic_session_;
  scenic::ImportNode parent_;
  scenic::ShapeNode shape_;
  scenic::Material material_;

  ScenicSurfaceFactory* const scenic_surface_factory_;
  const gfx::AcceleratedWidget window_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(ScenicSurface);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_SURFACE_H_
