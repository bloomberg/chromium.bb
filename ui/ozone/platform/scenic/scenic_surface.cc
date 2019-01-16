// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_surface.h"

#include <lib/ui/scenic/cpp/commands.h>
#include <lib/zx/eventpair.h>

#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/ozone/platform/scenic/scenic_gpu_host.h"
#include "ui/ozone/platform/scenic/scenic_surface_factory.h"

namespace ui {

ScenicSurface::ScenicSurface(ScenicSurfaceFactory* scenic_surface_factory,
                             fuchsia::ui::scenic::Scenic* scenic,
                             mojom::ScenicGpuHost* gpu_host,
                             gfx::AcceleratedWidget window)
    : scenic_session_(scenic),
      parent_(&scenic_session_),
      shape_(&scenic_session_),
      material_(&scenic_session_),
      scenic_surface_factory_(scenic_surface_factory),
      gpu_host_(gpu_host),
      window_(window) {
  shape_.SetShape(scenic::Rectangle(&scenic_session_, 1.f, 1.f));
  shape_.SetMaterial(material_);
  scenic_surface_factory->AddSurface(window, this);
}

ScenicSurface::~ScenicSurface() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scenic_surface_factory_->RemoveSurface(window_);
}

void ScenicSurface::SetTextureToNewImagePipe(
    fidl::InterfaceRequest<fuchsia::images::ImagePipe> image_pipe_request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  uint32_t image_pipe_id = scenic_session_.AllocResourceId();
  scenic_session_.Enqueue(scenic::NewCreateImagePipeCmd(
      image_pipe_id, std::move(image_pipe_request)));
  material_.SetTexture(image_pipe_id);
  scenic_session_.ReleaseResource(image_pipe_id);
}

void ScenicSurface::SetTextureToImage(const scenic::Image& image) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  material_.SetTexture(image);
}

void ScenicSurface::LinkToParent() {
  // Scenic does not care about order here; it's totally fine for imports to
  // cause exports, and that's what's done here.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  zx::eventpair export_token;
  parent_.BindAsRequest(&export_token);
  parent_.AddChild(shape_);
  gpu_host_->ExportParent(
      window_,
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(export_token))));
}

void ScenicSurface::Commit() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scenic_session_.Present(
      /*presentation_time=*/0, [](fuchsia::images::PresentationInfo info) {});
}

}  // namespace ui
