// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_FAKE_CONTEXT_FACTORY_H_
#define UI_COMPOSITOR_TEST_FAKE_CONTEXT_FACTORY_H_

#include "cc/output/renderer_settings.h"
#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "ui/compositor/compositor.h"

namespace cc {
class CompositorFrame;
class FakeLayerTreeFrameSink;
class ResourceSettings;
class TestTaskGraphRunner;
class TestGpuMemoryBufferManager;
}

namespace viz {
class ContextProvider;
}

namespace ui {

class FakeContextFactory : public ui::ContextFactory {
 public:
  FakeContextFactory();
  ~FakeContextFactory() override;

  const cc::CompositorFrame& GetLastCompositorFrame() const;

  // ui::ContextFactory:
  void CreateLayerTreeFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override;
  scoped_refptr<viz::ContextProvider> SharedMainThreadContextProvider()
      override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  double GetRefreshRate() const override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  const viz::ResourceSettings& GetResourceSettings() const override;
  void AddObserver(ui::ContextFactoryObserver* observer) override {}
  void RemoveObserver(ui::ContextFactoryObserver* observer) override {}

 protected:
  const cc::RendererSettings& renderer_settings() const {
    return renderer_settings_;
  }

 private:
  cc::FakeLayerTreeFrameSink* frame_sink_ = nullptr;
  cc::TestTaskGraphRunner task_graph_runner_;
  cc::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  cc::RendererSettings renderer_settings_;

  DISALLOW_COPY_AND_ASSIGN(FakeContextFactory);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_FAKE_CONTEXT_FACTORY_H_
