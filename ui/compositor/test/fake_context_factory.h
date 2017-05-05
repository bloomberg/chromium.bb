// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_FAKE_CONTEXT_FACTORY_H_
#define UI_COMPOSITOR_TEST_FAKE_CONTEXT_FACTORY_H_

#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_task_graph_runner.h"
#include "ui/compositor/compositor.h"

namespace cc {
class CompositorFrame;
class ContextProvider;
class FakeCompositorFrameSink;
class TestTaskGraphRunner;
class TestGpuMemoryBufferManager;
}

namespace ui {

class FakeContextFactory : public ui::ContextFactory {
 public:
  FakeContextFactory() = default;
  ~FakeContextFactory() override = default;

  const cc::CompositorFrame& GetLastCompositorFrame() const;

  // ui::ContextFactory:
  void CreateCompositorFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override;
  scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider() override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  double GetRefreshRate() const override;
  uint32_t GetImageTextureTarget(gfx::BufferFormat format,
                                 gfx::BufferUsage usage) override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  void AddObserver(ui::ContextFactoryObserver* observer) override {}
  void RemoveObserver(ui::ContextFactoryObserver* observer) override {}

 private:
  cc::FakeCompositorFrameSink* frame_sink_ = nullptr;
  cc::TestTaskGraphRunner task_graph_runner_;
  cc::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;

  DISALLOW_COPY_AND_ASSIGN(FakeContextFactory);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_FAKE_CONTEXT_FACTORY_H_
