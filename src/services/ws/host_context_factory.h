// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_HOST_CONTEXT_FACTORY_H_
#define SERVICES_WS_HOST_CONTEXT_FACTORY_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/gpu/context_provider.h"
#include "services/ws/public/cpp/raster_thread_helper.h"
#include "ui/compositor/compositor.h"

namespace gpu {
class GpuChannelHost;
}

namespace ui {
class HostContextFactoryPrivate;
}  // namespace ui

namespace ws {

class Gpu;

// ui::ContextFactory used when the WindowService is acting as the viz host.
// Internally this creates a ui::HostContextFactoryPrivate for the
// ui::ContextFactoryPrivate implementation.
class HostContextFactory : public ui::ContextFactory {
 public:
  HostContextFactory(Gpu* gpu,
                     viz::HostFrameSinkManager* host_frame_sink_manager);
  ~HostContextFactory() override;

  ui::ContextFactoryPrivate* GetContextFactoryPrivate();

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

  RasterThreadHelper raster_thread_helper_;
  Gpu* gpu_;
  scoped_refptr<viz::ContextProvider> shared_main_thread_context_provider_;

  std::unique_ptr<ui::HostContextFactoryPrivate> context_factory_private_;

  base::WeakPtrFactory<HostContextFactory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostContextFactory);
};

}  // namespace ws

#endif  // SERVICES_WS_HOST_CONTEXT_FACTORY_H_
