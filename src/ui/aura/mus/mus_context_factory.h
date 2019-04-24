// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_MUS_CONTEXT_FACTORY_H_
#define UI_AURA_MUS_MUS_CONTEXT_FACTORY_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/command_buffer/common/context_result.h"
#include "services/ws/public/cpp/gpu/shared_worker_context_provider_factory.h"
#include "services/ws/public/cpp/raster_thread_helper.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "ui/aura/aura_export.h"
#include "ui/compositor/compositor.h"

namespace gpu {
class GpuChannelHost;
class GpuChannelEstablishFactory;
}

namespace aura {

// ContextFactory implementation that can be used with Mus.
class AURA_EXPORT MusContextFactory : public ui::ContextFactory {
 public:
  explicit MusContextFactory(
      gpu::GpuChannelEstablishFactory* gpu_channel_establish_factory);
  ~MusContextFactory() override;

  // Drops the references to ContextProviders. This may be called to ensure a
  // particular shutdown ordering.
  void ResetContextProviders();

 private:
  // Validates the shared main thread context provider exists and hasn't been
  // lost. |main_context_provider_| will only be non-null when this function
  // returns kSuccess.
  gpu::ContextResult ValidateMainContextProvider(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel);

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

  gpu::GpuChannelEstablishFactory* const gpu_channel_establish_factory_;
  ws::RasterThreadHelper raster_thread_helper_;
  // Shared context provider for anything on the main thread.
  scoped_refptr<viz::ContextProvider> main_context_provider_;
  ws::SharedWorkerContextProviderFactory
      shared_worker_context_provider_factory_;

  base::WeakPtrFactory<MusContextFactory> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MusContextFactory);
};

}  // namespace aura

#endif  // UI_AURA_MUS_MUS_CONTEXT_FACTORY_H_
