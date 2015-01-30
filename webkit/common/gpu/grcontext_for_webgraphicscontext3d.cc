// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/gpu/grcontext_for_webgraphicscontext3d.h"

#include "base/lazy_instance.h"
#include "base/trace_event/trace_event.h"
#include "gpu/blink/webgraphicscontext3d_impl.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

using gpu_blink::WebGraphicsContext3DImpl;

namespace webkit {
namespace gpu {

namespace {

// Singleton used to initialize and terminate the gles2 library.
class GLES2Initializer {
 public:
  GLES2Initializer() { gles2::Initialize(); }

  ~GLES2Initializer() { gles2::Terminate(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLES2Initializer);
};

base::LazyInstance<GLES2Initializer> g_gles2_initializer =
    LAZY_INSTANCE_INITIALIZER;

void BindWebGraphicsContext3DGLContextCallback(const GrGLInterface* interface) {
  gles2::SetGLContext(reinterpret_cast<WebGraphicsContext3DImpl*>(
                          interface->fCallbackData)->GetGLInterface());
}

}  // namespace anonymous

GrContextForWebGraphicsContext3D::GrContextForWebGraphicsContext3D(
    WebGraphicsContext3DImpl* context3d) {
  if (!context3d)
    return;

  // Ensure the gles2 library is initialized first in a thread safe way.
  g_gles2_initializer.Get();
  gles2::SetGLContext(context3d->GetGLInterface());
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
  if (gr_context_) {
    // The limit of the number of GPU resources we hold in the GrContext's
    // GPU cache.
    static const int kMaxGaneshResourceCacheCount = 2048;
    // The limit of the bytes allocated toward GPU resources in the GrContext's
    // GPU cache.
    static const size_t kMaxGaneshResourceCacheBytes = 96 * 1024 * 1024;

    gr_context_->setResourceCacheLimits(kMaxGaneshResourceCacheCount,
                                        kMaxGaneshResourceCacheBytes);
  }
}

GrContextForWebGraphicsContext3D::~GrContextForWebGraphicsContext3D() {
}

void GrContextForWebGraphicsContext3D::OnLostContext() {
  if (gr_context_)
    gr_context_->abandonContext();
}

void GrContextForWebGraphicsContext3D::FreeGpuResources() {
  if (gr_context_) {
    TRACE_EVENT_INSTANT0("gpu", "GrContext::freeGpuResources", \
        TRACE_EVENT_SCOPE_THREAD);
    gr_context_->freeGpuResources();
  }
}

}  // namespace gpu
}  // namespace webkit
