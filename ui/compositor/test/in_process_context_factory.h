// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_FACTORY_H_
#define UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_FACTORY_H_

#include <stdint.h>
#include <memory>

#include "base/macros.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/frame_sink_id_allocator.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_image_factory.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/compositor/compositor.h"

namespace cc {
class ResourceSettings;
class SurfaceManager;
}

namespace viz {
class FrameSinkManagerHost;
}

namespace ui {
class InProcessContextProvider;

class InProcessContextFactory : public ContextFactory,
                                public ContextFactoryPrivate {
 public:
  // Both |frame_sink_manager_host| and |surface_manager| must outlive the
  // ContextFactory.
  // TODO(crbug.com/657959): |surface_manager| should go away and we should use
  // the LayerTreeFrameSink from the FrameSinkManagerHost.
  InProcessContextFactory(viz::FrameSinkManagerHost* frame_sink_manager_host,
                          cc::SurfaceManager* surface_manager);
  ~InProcessContextFactory() override;

  // If true (the default) an OutputSurface is created that does not display
  // anything. Set to false if you want to see results on the screen.
  void set_use_test_surface(bool use_test_surface) {
    use_test_surface_ = use_test_surface;
  }

  // This is used to call OnLostResources on all clients, to ensure they stop
  // using the SharedMainThreadContextProvider.
  void SendOnLostResources();

  // Set refresh rate to 200 to spend less time waiting for BeginFrame when
  // used for tests.
  void SetUseFastRefreshRateForTests();

  // ContextFactory implementation.
  void CreateLayerTreeFrameSink(base::WeakPtr<Compositor> compositor) override;

  std::unique_ptr<Reflector> CreateReflector(Compositor* mirrored_compositor,
                                             Layer* mirroring_layer) override;
  void RemoveReflector(Reflector* reflector) override;

  scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider() override;
  void RemoveCompositor(Compositor* compositor) override;
  double GetRefreshRate() const override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  cc::FrameSinkId AllocateFrameSinkId() override;
  cc::SurfaceManager* GetSurfaceManager() override;
  viz::FrameSinkManagerHost* GetFrameSinkManagerHost() override;
  void SetDisplayVisible(ui::Compositor* compositor, bool visible) override;
  void ResizeDisplay(ui::Compositor* compositor,
                     const gfx::Size& size) override;
  void SetDisplayColorSpace(
      ui::Compositor* compositor,
      const gfx::ColorSpace& blending_color_space,
      const gfx::ColorSpace& output_color_space) override {}
  void SetAuthoritativeVSyncInterval(ui::Compositor* compositor,
                                     base::TimeDelta interval) override {}
  void SetDisplayVSyncParameters(ui::Compositor* compositor,
                                 base::TimeTicks timebase,
                                 base::TimeDelta interval) override {}
  void SetOutputIsSecure(ui::Compositor* compositor, bool secure) override {}
  const cc::ResourceSettings& GetResourceSettings() const override;
  void AddObserver(ContextFactoryObserver* observer) override;
  void RemoveObserver(ContextFactoryObserver* observer) override;

 private:
  struct PerCompositorData;

  PerCompositorData* CreatePerCompositorData(ui::Compositor* compositor);

  scoped_refptr<InProcessContextProvider> shared_main_thread_contexts_;
  scoped_refptr<InProcessContextProvider> shared_worker_context_provider_;
  cc::TestSharedBitmapManager shared_bitmap_manager_;
  cc::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  cc::TestImageFactory image_factory_;
  cc::TestTaskGraphRunner task_graph_runner_;
  cc::FrameSinkIdAllocator frame_sink_id_allocator_;
  bool use_test_surface_;
  double refresh_rate_ = 60.0;
  viz::FrameSinkManagerHost* frame_sink_manager_;
  cc::SurfaceManager* surface_manager_;
  base::ObserverList<ContextFactoryObserver> observer_list_;

  cc::RendererSettings renderer_settings_;
  using PerCompositorDataMap =
      base::hash_map<ui::Compositor*, std::unique_ptr<PerCompositorData>>;
  PerCompositorDataMap per_compositor_data_;

  DISALLOW_COPY_AND_ASSIGN(InProcessContextFactory);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_FACTORY_H_
