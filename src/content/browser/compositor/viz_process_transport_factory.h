// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_VIZ_PROCESS_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_VIZ_PROCESS_TRANSPORT_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/service/main/viz_compositor_thread_runner.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "gpu/command_buffer/common/context_result.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/privileged/interfaces/compositing/frame_sink_manager.mojom.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/host/host_context_factory_private.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class SingleThreadTaskGraphRunner;
}

namespace gpu {
class GpuChannelEstablishFactory;
}

namespace viz {
class CompositingModeReporterImpl;
class RasterContextProvider;
}

namespace ws {
class ContextProviderCommandBuffer;
}

namespace content {

// A replacement for GpuProcessTransportFactory to be used when running viz. In
// this configuration the display compositor is located in the viz process
// instead of in the browser process. Any interaction with the display
// compositor must happen over IPC.
class VizProcessTransportFactory : public ui::ContextFactory,
                                   public ui::HostContextFactoryPrivate,
                                   public ImageTransportFactory,
                                   public viz::ContextLostObserver {
 public:
  VizProcessTransportFactory(
      gpu::GpuChannelEstablishFactory* gpu_channel_establish_factory,
      scoped_refptr<base::SingleThreadTaskRunner> resize_task_runner,
      viz::CompositingModeReporterImpl* compositing_mode_reporter);
  ~VizProcessTransportFactory() override;

  // Connects HostFrameSinkManager to FrameSinkManagerImpl in viz process.
  void ConnectHostFrameSinkManager();

  // ui::ContextFactory implementation.
  void CreateLayerTreeFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override;
  scoped_refptr<viz::ContextProvider> SharedMainThreadContextProvider()
      override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  double GetRefreshRate() const override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  void AddObserver(ui::ContextFactoryObserver* observer) override;
  void RemoveObserver(ui::ContextFactoryObserver* observer) override;
  bool SyncTokensRequiredForDisplayCompositor() override;

  // ImageTransportFactory implementation.
  void DisableGpuCompositing() override;
  bool IsGpuCompositingDisabled() override;
  ui::ContextFactory* GetContextFactory() override;
  ui::ContextFactoryPrivate* GetContextFactoryPrivate() override;

  // viz::ContextLostObserver implementation.
  void OnContextLost() override;

 private:
  // Disables GPU compositing. This notifies UI and renderer compositors to drop
  // LayerTreeFrameSinks and request new ones. If fallback happens while
  // creating a new LayerTreeFrameSink for UI compositor it should be passed in
  // as |guilty_compositor| to avoid extra work and reentrancy problems.
  void DisableGpuCompositing(ui::Compositor* guilty_compositor);

  // Provided as a callback when the GPU process has crashed.
  void OnGpuProcessLost();

  // Finishes creation of LayerTreeFrameSink after GPU channel has been
  // established.
  void OnEstablishedGpuChannel(
      base::WeakPtr<ui::Compositor> compositor_weak_ptr,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel);

  // Tries to create the raster and main thread ContextProviders. If the
  // ContextProviders already exist and haven't been lost then this will do
  // nothing. Also verifies |gpu_channel_host| and checks if GPU compositing is
  // blacklisted.
  //
  // Returns kSuccess if caller can use GPU compositing, kTransientFailure if
  // caller should try again or kFatalFailure/kSurfaceFailure if caller should
  // fallback to software compositing.
  gpu::ContextResult TryCreateContextsForGpuCompositing(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host);

  void OnLostMainThreadSharedContext();

  gpu::GpuChannelEstablishFactory* const gpu_channel_establish_factory_;

  // Controls the compositing mode based on what mode the display compositors
  // are using.
  viz::CompositingModeReporterImpl* const compositing_mode_reporter_;

  base::ObserverList<ui::ContextFactoryObserver>::Unchecked observer_list_;

  // ContextProvider used on worker threads for rasterization.
  scoped_refptr<viz::RasterContextProvider> worker_context_provider_;

  // ContextProvider used on the main thread. Shared by ui::Compositors and also
  // returned from GetSharedMainThreadContextProvider().
  scoped_refptr<ws::ContextProviderCommandBuffer> main_context_provider_;

  std::unique_ptr<cc::SingleThreadTaskGraphRunner> task_graph_runner_;

  // Will start and run the VizCompositorThread for using an in-process display
  // compositor.
  std::unique_ptr<viz::VizCompositorThreadRunner> viz_compositor_thread_;

  base::WeakPtrFactory<VizProcessTransportFactory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VizProcessTransportFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_VIZ_PROCESS_TRANSPORT_FACTORY_H_
