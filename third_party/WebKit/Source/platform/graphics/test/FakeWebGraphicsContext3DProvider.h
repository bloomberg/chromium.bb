// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeWebGraphicsContext3DProvider_h
#define FakeWebGraphicsContext3DProvider_h

#include "cc/tiles/image_decode_cache.h"
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
  FakeWebGraphicsContext3DProvider(gpu::gles2::GLES2Interface* gl,
                                   cc::ImageDecodeCache* cache = nullptr)
      : gl_(gl), image_decode_cache_(cache) {
    sk_sp<const GrGLInterface> gl_interface(GrGLCreateNullInterface());
    gr_context_ = GrContext::MakeGL(std::move(gl_interface));
  }
  ~FakeWebGraphicsContext3DProvider() override = default;

  GrContext* GetGrContext() override { return gr_context_.get(); }
  void InvalidateGrContext(uint32_t state) override {
    gr_context_->resetContext(state);
  }

  const gpu::Capabilities& GetCapabilities() const override {
    return capabilities_;
  }

  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override {
    return gpu_feature_info_;
  }

  viz::GLHelper* GetGLHelper() override { return nullptr; }

  bool IsSoftwareRendering() const override { return false; }

  gpu::gles2::GLES2Interface* ContextGL() override { return gl_; }

  bool BindToCurrentThread() override { return false; }
  void SetLostContextCallback(base::Closure) override {}
  void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t id)>) {}
  void SignalQuery(uint32_t, base::OnceClosure) override {}
  cc::ImageDecodeCache* ImageDecodeCache() override {
    return image_decode_cache_;
  }

 private:
  gpu::gles2::GLES2Interface* gl_;
  sk_sp<GrContext> gr_context_;
  gpu::Capabilities capabilities_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  cc::ImageDecodeCache* image_decode_cache_;
};

}  // namespace blink

#endif  // FakeWebGraphicsContext3DProvider_h
