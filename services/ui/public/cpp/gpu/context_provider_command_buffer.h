// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_GPU_CONTEXT_PROVIDER_COMMAND_BUFFER_H_
#define SERVICES_UI_PUBLIC_CPP_GPU_CONTEXT_PROVIDER_COMMAND_BUFFER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/output/context_provider.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/ipc/common/surface_handle.h"
#include "services/ui/public/cpp/gpu/command_buffer_metrics.h"
#include "ui/gl/gpu_preference.h"
#include "url/gurl.h"

namespace gpu {
class CommandBufferProxyImpl;
class GpuChannelHost;
class TransferBuffer;
namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
class GLES2TraceImplementation;
}
}

namespace skia_bindings {
class GrContextForGLES2Interface;
}

namespace ui {

// Implementation of cc::ContextProvider that provides a GL implementation over
// command buffer to the GPU process.
class ContextProviderCommandBuffer
    : public cc::ContextProvider,
      public base::trace_event::MemoryDumpProvider {
 public:
  ContextProviderCommandBuffer(
      scoped_refptr<gpu::GpuChannelHost> channel,
      int32_t stream_id,
      gpu::SchedulingPriority stream_priority,
      gpu::SurfaceHandle surface_handle,
      const GURL& active_url,
      bool automatic_flushes,
      bool support_locking,
      const gpu::SharedMemoryLimits& memory_limits,
      const gpu::gles2::ContextCreationAttribHelper& attributes,
      ContextProviderCommandBuffer* shared_context_provider,
      command_buffer_metrics::ContextType type);

  gpu::CommandBufferProxyImpl* GetCommandBufferProxy();
  // Gives the GL internal format that should be used for calling CopyTexImage2D
  // on the default framebuffer.
  uint32_t GetCopyTextureInternalFormat();

  // cc::ContextProvider implementation.
  bool BindToCurrentThread() override;
  void DetachFromThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  cc::ContextCacheController* CacheController() override;
  void InvalidateGrContext(uint32_t state) override;
  base::Lock* GetLock() override;
  gpu::Capabilities ContextCapabilities() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Set the default task runner for command buffers to use for handling IPCs.
  // If not specified, this will be the ThreadTaskRunner for the thread on
  // which BindToThread is called.
  void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> default_task_runner);

 protected:
  ~ContextProviderCommandBuffer() override;

  void OnLostContext();

 private:
  struct SharedProviders : public base::RefCountedThreadSafe<SharedProviders> {
    base::Lock lock;
    std::vector<ContextProviderCommandBuffer*> list;

    SharedProviders();

   private:
    friend class base::RefCountedThreadSafe<SharedProviders>;
    ~SharedProviders();
  };

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  bool bind_succeeded_ = false;
  bool bind_failed_ = false;

  const int32_t stream_id_;
  const gpu::SchedulingPriority stream_priority_;
  const gpu::SurfaceHandle surface_handle_;
  const GURL active_url_;
  const bool automatic_flushes_;
  const bool support_locking_;
  const gpu::SharedMemoryLimits memory_limits_;
  const gpu::gles2::ContextCreationAttribHelper attributes_;
  const command_buffer_metrics::ContextType context_type_;

  scoped_refptr<SharedProviders> shared_providers_;
  scoped_refptr<gpu::GpuChannelHost> channel_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;

  base::Lock context_lock_;  // Referenced by command_buffer_.
  std::unique_ptr<gpu::CommandBufferProxyImpl> command_buffer_;
  std::unique_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  std::unique_ptr<gpu::TransferBuffer> transfer_buffer_;
  std::unique_ptr<gpu::gles2::GLES2Implementation> gles2_impl_;
  std::unique_ptr<gpu::gles2::GLES2TraceImplementation> trace_impl_;
  std::unique_ptr<skia_bindings::GrContextForGLES2Interface> gr_context_;
  std::unique_ptr<cc::ContextCacheController> cache_controller_;

  LostContextCallback lost_context_callback_;
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_GPU_CONTEXT_PROVIDER_COMMAND_BUFFER_H_
