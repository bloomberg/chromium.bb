// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_MUS_CONTEXT_FACTORY_H_
#define UI_AURA_MUS_MUS_CONTEXT_FACTORY_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/gpu/context_provider.h"
#include "services/ws/public/cpp/raster_thread_helper.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "ui/aura/aura_export.h"
#include "ui/compositor/compositor.h"

namespace gpu {
class GpuChannelHost;
}

namespace ws {
class Gpu;
class SharedWorkerContextProviderFactory;
}

namespace aura {

// ContextFactory implementation that can be used with Mus.
class AURA_EXPORT MusContextFactory : public ui::ContextFactory {
 public:
  explicit MusContextFactory(ws::Gpu* gpu);
  ~MusContextFactory() override;

  // Drops the references to the RasterContextProvider. This may be called to
  // ensure a particular shutdown ordering.
  void ResetSharedWorkerContextProvider();

 private:
  // Callback function for Gpu::EstablishGpuChannel().
  void OnEstablishedGpuChannel(base::WeakPtr<ui::Compositor> compositor,
                               scoped_refptr<gpu::GpuChannelHost> gpu_channel);

  // ContextFactory:
  void CreateLayerTreeFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override;
  scoped_refptr<viz::ContextProvider> SharedMainThreadContextProvider()
      override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  void AddObserver(ui::ContextFactoryObserver* observer) override {}
  void RemoveObserver(ui::ContextFactoryObserver* observer) override {}
  bool SyncTokensRequiredForDisplayCompositor() override;

  ws::RasterThreadHelper raster_thread_helper_;
  ws::Gpu* gpu_;
  scoped_refptr<viz::ContextProvider> shared_main_thread_context_provider_;
  std::unique_ptr<ws::SharedWorkerContextProviderFactory>
      shared_worker_context_provider_factory_;

  base::WeakPtrFactory<MusContextFactory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MusContextFactory);
};

}  // namespace aura

#endif  // UI_AURA_MUS_MUS_CONTEXT_FACTORY_H_
