// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_SHARED_IMAGE_STUB_H_
#define GPU_IPC_SERVICE_SHARED_IMAGE_STUB_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/gpu_ipc_service_export.h"

namespace gpu {
class SharedContextState;
struct Mailbox;
class GpuChannel;
class SharedImageFactory;

class GPU_IPC_SERVICE_EXPORT SharedImageStub
    : public MemoryTracker,
      public base::trace_event::MemoryDumpProvider {
 public:
  ~SharedImageStub() override;

  using SharedImageDestructionCallback =
      base::OnceCallback<void(const gpu::SyncToken&)>;

  static std::unique_ptr<SharedImageStub> Create(GpuChannel* channel,
                                                 int32_t route_id);

  // Executes a DeferredRequest routed to this stub by a GpuChannel.
  void ExecuteDeferredRequest(mojom::DeferredSharedImageRequestPtr request);

  // MemoryTracker implementation:
  void TrackMemoryAllocatedChange(int64_t delta) override;
  uint64_t GetSize() const override;
  uint64_t ClientTracingId() const override;
  int ClientId() const override;
  uint64_t ContextGroupTracingId() const override;

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  SequenceId sequence() const { return sequence_; }
  SharedImageFactory* factory() const { return factory_.get(); }
  GpuChannel* channel() const { return channel_; }

  SharedImageDestructionCallback GetSharedImageDestructionCallback(
      const Mailbox& mailbox);

  bool CreateSharedImage(const Mailbox& mailbox,
                         int client_id,
                         gfx::GpuMemoryBufferHandle handle,
                         gfx::BufferFormat format,
                         gfx::BufferPlane plane,
                         SurfaceHandle surface_handle,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         uint32_t usage);

#if defined(OS_ANDROID)
  bool CreateSharedImageWithAHB(const Mailbox& out_mailbox,
                                const Mailbox& in_mailbox,
                                uint32_t usage);
#endif

  bool UpdateSharedImage(const Mailbox& mailbox,
                         gfx::GpuFenceHandle in_fence_handle);

#if defined(OS_FUCHSIA)
  void RegisterSysmemBufferCollection(gfx::SysmemBufferCollectionId id,
                                      zx::channel token,
                                      gfx::BufferFormat format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe);
  void ReleaseSysmemBufferCollection(gfx::SysmemBufferCollectionId id);
#endif  // OS_FUCHSIA

 private:
  SharedImageStub(GpuChannel* channel, int32_t route_id);

  void OnCreateSharedImage(mojom::CreateSharedImageParamsPtr params);
  void OnCreateSharedImageWithData(
      mojom::CreateSharedImageWithDataParamsPtr params);
  void OnCreateGMBSharedImage(mojom::CreateGMBSharedImageParamsPtr params);
  void OnUpdateSharedImage(const Mailbox& mailbox,
                           uint32_t release_id,
                           gfx::GpuFenceHandle in_fence_handle);
#if defined(OS_ANDROID)
  void OnCreateSharedImageWithAHB(const Mailbox& out_mailbox,
                                  const Mailbox& in_mailbox,
                                  uint32_t usage,
                                  uint32_t release_id);
#endif
  void OnDestroySharedImage(const Mailbox& mailbox);
  void OnRegisterSharedImageUploadBuffer(base::ReadOnlySharedMemoryRegion shm);
#if defined(OS_WIN)
  void OnCreateSharedImageVideoPlanes(
      mojom::CreateSharedImageVideoPlanesParamsPtr params);
  void OnCopyToGpuMemoryBuffer(const Mailbox& mailbox, uint32_t release_id);
  void OnCreateSwapChain(mojom::CreateSwapChainParamsPtr params);
  void OnPresentSwapChain(const Mailbox& mailbox, uint32_t release_id);
#endif  // OS_WIN

  bool MakeContextCurrent(bool needs_gl = false);
  ContextResult MakeContextCurrentAndCreateFactory();
  void OnError();

  // Wait on the sync token if any and destroy the shared image.
  void DestroySharedImage(const Mailbox& mailbox, const SyncToken& sync_token);

  raw_ptr<GpuChannel> channel_;

  // While this is not a CommandBuffer, this provides a unique identifier for
  // a SharedImageStub, comprised of identifiers which it was already using.
  // TODO(jonross): Look into a rename of CommandBufferId to reflect that it can
  // be a unique identifier for numerous gpu constructs.
  CommandBufferId command_buffer_id_;
  const SequenceId sequence_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<SharedImageFactory> factory_;
  uint64_t size_ = 0;
  // Holds shared memory used in initial data uploads.
  base::ReadOnlySharedMemoryRegion upload_memory_;
  base::ReadOnlySharedMemoryMapping upload_memory_mapping_;

  base::WeakPtrFactory<SharedImageStub> weak_factory_{this};
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_SHARED_IMAGE_STUB_H_
