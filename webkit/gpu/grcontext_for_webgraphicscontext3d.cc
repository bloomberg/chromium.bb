// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/gpu/grcontext_for_webgraphicscontext3d.h"

#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace webkit {
namespace gpu {

static void BindWebGraphicsContext3DGLContextCallback(
    const GrGLInterface* interface) {
  reinterpret_cast<WebKit::WebGraphicsContext3D*>(
      interface->fCallbackData)->makeContextCurrent();
}

GrContextForWebGraphicsContext3D::GrContextForWebGraphicsContext3D(
    WebKit::WebGraphicsContext3D* context3d) {
  if (!context3d)
    return;

  skia::RefPtr<GrGLInterface> interface = skia::AdoptRef(
      context3d->createGrGLInterface());
  if (!interface)
    return;

  interface->fCallback = BindWebGraphicsContext3DGLContextCallback;
  interface->fCallbackData =
      reinterpret_cast<GrGLInterfaceCallbackData>(context3d);

  gr_context_ = skia::AdoptRef(GrContext::Create(
      kOpenGL_GrBackend,
      reinterpret_cast<GrBackendContext>(interface.get())));
  if (!gr_context_)
    return;

  bool nonzero_allocation = true;
  SetMemoryLimit(nonzero_allocation);
}

GrContextForWebGraphicsContext3D::~GrContextForWebGraphicsContext3D() {
  if (gr_context_)
    gr_context_->contextDestroyed();
}

void GrContextForWebGraphicsContext3D::SetMemoryLimit(bool nonzero_allocation) {
  if (!gr_context_)
    return;

  if (nonzero_allocation) {
    // The limit of the number of textures we hold in the GrContext's
    // bitmap->texture cache.
    static const int kMaxGaneshTextureCacheCount = 2048;
    // The limit of the bytes allocated toward textures in the GrContext's
    // bitmap->texture cache.
    static const size_t kMaxGaneshTextureCacheBytes = 96 * 1024 * 1024;

    gr_context_->setTextureCacheLimits(
        kMaxGaneshTextureCacheCount, kMaxGaneshTextureCacheBytes);
  } else {
    gr_context_->freeGpuResources();
    gr_context_->setTextureCacheLimits(0, 0);
  }
}

}  // namespace gpu
}  // namespace webkit
