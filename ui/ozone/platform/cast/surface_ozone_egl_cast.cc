// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/surface_ozone_egl_cast.h"

#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/platform/cast/surface_factory_cast.h"

namespace ui {

SurfaceOzoneEglCast::~SurfaceOzoneEglCast() {
  parent_->ChildDestroyed();
}

intptr_t SurfaceOzoneEglCast::GetNativeWindow() {
  return reinterpret_cast<intptr_t>(parent_->GetNativeWindow());
}

bool SurfaceOzoneEglCast::OnSwapBuffers() {
  parent_->OnSwapBuffers();
  return true;
}

void SurfaceOzoneEglCast::OnSwapBuffersAsync(
    const SwapCompletionCallback& callback) {
  parent_->OnSwapBuffers();
  callback.Run(gfx::SwapResult::SWAP_ACK);
}

bool SurfaceOzoneEglCast::ResizeNativeWindow(const gfx::Size& viewport_size) {
  return parent_->ResizeDisplay(viewport_size);
}

scoped_ptr<gfx::VSyncProvider> SurfaceOzoneEglCast::CreateVSyncProvider() {
  return nullptr;
}

}  // namespace ui
