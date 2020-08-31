// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_GPU_GPU_VIDEO_ACCELERATOR_FACTORIES_IMPL_H_
#define CONTENT_RENDERER_MEDIA_GPU_GPU_VIDEO_ACCELERATOR_FACTORIES_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/unguessable_token.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/video/supported_video_decoder_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
class GpuChannelHost;
class GpuMemoryBufferManager;
}  // namespace gpu

namespace viz {
class ContextProviderCommandBuffer;
}  // namespace viz

namespace content {

// Glue code to expose functionality needed by media::GpuVideoAccelerator to
// RenderViewImpl.  This class is entirely an implementation detail of
// RenderViewImpl and only has its own header to allow extraction of its
// implementation from render_view_impl.cc which is already far too large.
//
// The GpuVideoAcceleratorFactoriesImpl can be constructed on any thread,
// but subsequent calls to all public methods of the class must be called from
// the |task_runner_|, as provided during construction.
// |context_provider| should not support locking and will be bound to
// |task_runner_| where all the operations on the context should also happen.
class CONTENT_EXPORT GpuVideoAcceleratorFactoriesImpl
    : public media::GpuVideoAcceleratorFactories,
      public viz::ContextLostObserver {
 public:
  // Takes a ref on |gpu_channel_host| and tests |context| for loss before each
  // use.  Safe to call from any thread.
  static std::unique_ptr<GpuVideoAcceleratorFactoriesImpl> Create(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      const scoped_refptr<base::SingleThreadTaskRunner>&
          main_thread_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const scoped_refptr<viz::ContextProviderCommandBuffer>& context_provider,
      bool enable_video_gpu_memory_buffers,
      bool enable_media_stream_gpu_memory_buffers,
      bool enable_video_accelerator,
      mojo::PendingRemote<media::mojom::InterfaceFactory>
          interface_factory_remote,
      mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
          vea_provider_remote);

  // media::GpuVideoAcceleratorFactories implementation.
  bool IsGpuVideoAcceleratorEnabled() override;
  base::UnguessableToken GetChannelToken() override;
  int32_t GetCommandBufferRouteId() override;
  std::unique_ptr<media::VideoDecoder> CreateVideoDecoder(
      media::MediaLog* media_log,
      media::VideoDecoderImplementation implementation,
      media::RequestOverlayInfoCB request_overlay_info_cb) override;
  Supported IsDecoderConfigSupported(
      media::VideoDecoderImplementation implementation,
      const media::VideoDecoderConfig& config) override;
  std::unique_ptr<media::VideoEncodeAccelerator> CreateVideoEncodeAccelerator()
      override;

  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;

  bool ShouldUseGpuMemoryBuffersForVideoFrames(
      bool for_media_stream) const override;
  unsigned ImageTextureTarget(gfx::BufferFormat format) override;
  OutputFormat VideoFrameOutputFormat(
      media::VideoPixelFormat pixel_format) override;

  // Called on the media thread. Returns the SharedImageInterface unless the
  // ContextProvider has been lost, in which case it returns null.
  gpu::SharedImageInterface* SharedImageInterface() override;
  gpu::GpuMemoryBufferManager* GpuMemoryBufferManager() override;
  // Called on the media thread. Verifies if the ContextProvider is lost and
  // notifies the main thread of loss if it has occured, which can be seen later
  // from CheckContextProviderLost().
  bool CheckContextLost();
  // Called on the media thread. Destroys the ContextProvider held in this
  // class. Should only be called if the ContextProvider was previously lost,
  // and this class will no longer be used, as it assumes a ContextProvider is
  // present otherwise.
  void DestroyContext();
  base::UnsafeSharedMemoryRegion CreateSharedMemoryRegion(size_t size) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override;

  base::Optional<media::VideoEncodeAccelerator::SupportedProfiles>
  GetVideoEncodeAcceleratorSupportedProfiles() override;

  viz::RasterContextProvider* GetMediaContextProvider() override;

  void SetRenderingColorSpace(const gfx::ColorSpace& color_space) override;

  // Called on the main thread. Returns whether the media thread has seen the
  // ContextProvider become lost, in which case this class should be replaced
  // with a new ContextProvider.
  bool CheckContextProviderLostOnMainThread();

  ~GpuVideoAcceleratorFactoriesImpl() override;

 private:
  GpuVideoAcceleratorFactoriesImpl(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      const scoped_refptr<base::SingleThreadTaskRunner>&
          main_thread_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const scoped_refptr<viz::ContextProviderCommandBuffer>& context_provider,
      bool enable_gpu_memory_buffer_video_frames_for_video,
      bool enable_gpu_memory_buffer_video_frames_for_media_stream,
      bool enable_video_accelerator,
      mojo::PendingRemote<media::mojom::InterfaceFactory>
          interface_factory_remote,
      mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
          vea_provider_remote);

  void BindOnTaskRunner(
      mojo::PendingRemote<media::mojom::InterfaceFactory>
          interface_factory_remote,
      mojo::PendingRemote<media::mojom::VideoEncodeAcceleratorProvider>
          vea_provider_remote);

  // viz::ContextLostObserver implementation.
  void OnContextLost() override;
  void SetContextProviderLostOnMainThread();

  void OnSupportedDecoderConfigs(
      const media::SupportedVideoDecoderConfigMap& supported_configs);
  void OnGetVideoEncodeAcceleratorSupportedProfiles(
      const media::VideoEncodeAccelerator::SupportedProfiles&
          supported_profiles);

  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;

  // Shared pointer to a shared context provider. It is initially set on main
  // thread, but all subsequent access and destruction should happen only on the
  // media thread.
  scoped_refptr<viz::ContextProviderCommandBuffer> context_provider_;
  // Signals if |context_provider_| is alive on the media thread. For use on the
  // main thread.
  bool context_provider_lost_ = false;
  // A shadow of |context_provider_lost_| for the media thread.
  bool context_provider_lost_on_media_thread_ = false;

  base::UnguessableToken channel_token_;

  // Whether gpu memory buffers should be used to hold video frames data.
  const bool enable_video_gpu_memory_buffers_;
  const bool enable_media_stream_gpu_memory_buffers_;
  // Whether video acceleration encoding/decoding should be enabled.
  const bool video_accelerator_enabled_;

  gfx::ColorSpace rendering_color_space_;

  gpu::GpuMemoryBufferManager* const gpu_memory_buffer_manager_;

  mojo::Remote<media::mojom::InterfaceFactory> interface_factory_;
  mojo::Remote<media::mojom::VideoEncodeAcceleratorProvider> vea_provider_;

  // SupportedDecoderConfigs state.
  mojo::Remote<media::mojom::VideoDecoder> video_decoder_;

  base::Lock supported_profiles_lock_;

  // If the Optional is empty, then we have not yet gotten the configs.  If the
  // Optional contains an empty vector, then we have gotten the result and there
  // are no supported configs.
  base::Optional<media::SupportedVideoDecoderConfigMap>
      supported_decoder_configs_ GUARDED_BY(supported_profiles_lock_);

  base::Optional<media::VideoEncodeAccelerator::SupportedProfiles>
      supported_vea_profiles_ GUARDED_BY(supported_profiles_lock_);
  // For sending requests to allocate shared memory in the Browser process.
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoAcceleratorFactoriesImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_GPU_GPU_VIDEO_ACCELERATOR_FACTORIES_IMPL_H_
