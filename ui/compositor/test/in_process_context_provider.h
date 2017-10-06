// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_
#define UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {
class GLInProcessContext;
class GpuMemoryBufferManager;
class ImageFactory;
}

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace ui {

class InProcessContextProvider : public viz::ContextProvider {
 public:
  static scoped_refptr<InProcessContextProvider> Create(
      const gpu::gles2::ContextCreationAttribHelper& attribs,
      InProcessContextProvider* shared_context,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gpu::ImageFactory* image_factory,
      gpu::SurfaceHandle window,
      const std::string& debug_name);

  // Uses default attributes for creating an offscreen context.
  static scoped_refptr<InProcessContextProvider> CreateOffscreen(
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gpu::ImageFactory* image_factory,
      InProcessContextProvider* shared_context);

  // cc::ContextProvider implementation.
  bool BindToCurrentThread() override;
  void DetachFromThread() override;
  const gpu::Capabilities& ContextCapabilities() const override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  viz::ContextCacheController* CacheController() override;
  void InvalidateGrContext(uint32_t state) override;
  base::Lock* GetLock() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;

  // Gives the GL internal format that should be used for calling CopyTexImage2D
  // on the default framebuffer.
  uint32_t GetCopyTextureInternalFormat();

 private:
  InProcessContextProvider(
      const gpu::gles2::ContextCreationAttribHelper& attribs,
      InProcessContextProvider* shared_context,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      gpu::ImageFactory* image_factory,
      gpu::SurfaceHandle window,
      const std::string& debug_name);
  ~InProcessContextProvider() override;

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  std::unique_ptr<gpu::GLInProcessContext> context_;
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;
  std::unique_ptr<viz::ContextCacheController> cache_controller_;

  gpu::gles2::ContextCreationAttribHelper attribs_;
  InProcessContextProvider* shared_context_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  gpu::ImageFactory* image_factory_;
  gpu::SurfaceHandle window_;
  std::string debug_name_;

  base::Lock context_lock_;

  DISALLOW_COPY_AND_ASSIGN(InProcessContextProvider);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_
