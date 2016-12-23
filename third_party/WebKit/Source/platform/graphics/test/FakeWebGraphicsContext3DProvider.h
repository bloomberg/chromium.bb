// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace blink {

class FakeWebGraphicsContext3DProvider : public WebGraphicsContext3DProvider {
 public:
  FakeWebGraphicsContext3DProvider(gpu::gles2::GLES2Interface* gl) : m_gl(gl) {
    sk_sp<const GrGLInterface> glInterface(GrGLCreateNullInterface());
    m_grContext.reset(GrContext::Create(
        kOpenGL_GrBackend,
        reinterpret_cast<GrBackendContext>(glInterface.get())));
  }

  GrContext* grContext() override { return m_grContext.get(); }

  gpu::Capabilities getCapabilities() override { return gpu::Capabilities(); }

  bool isSoftwareRendering() const override { return false; }

  gpu::gles2::GLES2Interface* contextGL() override { return m_gl; }

  bool bindToCurrentThread() override { return false; }
  void setLostContextCallback(const base::Closure&) override {}
  void setErrorMessageCallback(
      const base::Callback<void(const char*, int32_t id)>&) {}
  void signalQuery(uint32_t, const base::Closure&) override {}

 private:
  gpu::gles2::GLES2Interface* m_gl;
  sk_sp<GrContext> m_grContext;
};

}  // namespace blink
