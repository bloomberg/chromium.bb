// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ozone/surface_ozone_base.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/vsync_provider.h"

namespace gfx {

SurfaceOzoneBase::SurfaceOzoneBase() {}

SurfaceOzoneBase::~SurfaceOzoneBase() {}

bool SurfaceOzoneBase::InitializeEGL() { return false; }

intptr_t /* EGLNativeWindowType */ SurfaceOzoneBase::GetEGLNativeWindow() {
  NOTREACHED();
  return 0;
}

bool SurfaceOzoneBase::InitializeCanvas() { return false; }

skia::RefPtr<SkCanvas> SurfaceOzoneBase::GetCanvas() {
  NOTREACHED();
  return skia::RefPtr<SkCanvas>();
}

bool SurfaceOzoneBase::ResizeCanvas(const gfx::Size& viewport_size) {
  NOTIMPLEMENTED();
  return false;
}

bool SurfaceOzoneBase::PresentCanvas() {
  NOTIMPLEMENTED();
  return true;
}

scoped_ptr<gfx::VSyncProvider> SurfaceOzoneBase::CreateVSyncProvider() {
  NOTIMPLEMENTED();
  return scoped_ptr<gfx::VSyncProvider>();
}

}  // namespace gfx
