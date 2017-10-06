// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeWebGraphicsContext3DProvider_h
#define FakeWebGraphicsContext3DProvider_h

#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/config/gpu_feature_info.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace blink {

class FakeWebGraphicsContext3DProvider : public WebGraphicsContext3DProvider {
 public:
  FakeWebGraphicsContext3DProvider(gpu::gles2::GLES2Interface* gl) : gl_(gl) {
    sk_sp<const GrGLInterface> gl_interface(GrGLCreateNullInterface());
    gr_context_.reset(GrContext::Create(
        kOpenGL_GrBackend,
        reinterpret_cast<GrBackendContext>(gl_interface.get())));
  }

  GrContext* GetGrContext() override { return gr_context_.get(); }

  const gpu::Capabilities& GetCapabilities() const override {
    return capabilities_;
  }

  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override {
    return gpu_feature_info_;
  }

  bool IsSoftwareRendering() const override { return false; }

  gpu::gles2::GLES2Interface* ContextGL() override { return gl_; }

  bool BindToCurrentThread() override { return false; }
  void SetLostContextCallback(const base::Closure&) override {}
  void SetErrorMessageCallback(
      const base::Callback<void(const char*, int32_t id)>&) {}
  void SignalQuery(uint32_t, const base::Closure&) override {}

 private:
  gpu::gles2::GLES2Interface* gl_;
  sk_sp<GrContext> gr_context_;
  gpu::Capabilities capabilities_;
  gpu::GpuFeatureInfo gpu_feature_info_;
};

}  // namespace blink

#endif  // FakeWebGraphicsContext3DProvider_h
