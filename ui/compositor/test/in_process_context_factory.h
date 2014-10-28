// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_FACTORY_H_
#define UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_FACTORY_H_

#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "ui/compositor/compositor.h"

namespace base {
class Thread;
}

namespace webkit {
namespace gpu {
class ContextProviderInProcess;
}
}

namespace ui {

class InProcessContextFactory : public ContextFactory {
 public:
  InProcessContextFactory();
  ~InProcessContextFactory() override;

  // ContextFactory implementation
  void CreateOutputSurface(base::WeakPtr<Compositor> compositor,
                           bool software_fallback) override;

  scoped_refptr<Reflector> CreateReflector(Compositor* mirrored_compositor,
                                           Layer* mirroring_layer) override;
  void RemoveReflector(scoped_refptr<Reflector> reflector) override;

  scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider() override;
  void RemoveCompositor(Compositor* compositor) override;
  bool DoesCreateTestContexts() override;
  cc::SharedBitmapManager* GetSharedBitmapManager() override;
  cc::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  base::MessageLoopProxy* GetCompositorMessageLoop() override;
  scoped_ptr<cc::SurfaceIdAllocator> CreateSurfaceIdAllocator() override;

 private:
  scoped_ptr<base::Thread> compositor_thread_;
  scoped_refptr<webkit::gpu::ContextProviderInProcess>
      shared_main_thread_contexts_;
  cc::TestSharedBitmapManager shared_bitmap_manager_;
  cc::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  uint32_t next_surface_id_namespace_;

  DISALLOW_COPY_AND_ASSIGN(InProcessContextFactory);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_IN_PROCESS_CONTEXT_FACTORY_H_
