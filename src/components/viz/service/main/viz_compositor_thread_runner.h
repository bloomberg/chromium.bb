// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_H_
#define COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/viz/service/viz_service_export.h"
#include "services/viz/privileged/interfaces/viz_main.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/java_handler_thread.h"
#endif

namespace base {
class SingleThreadTaskRunner;
class Thread;
}  // namespace base

namespace gpu {
class CommandBufferTaskExecutor;
class GpuChannelManager;
class ImageFactory;
}  // namespace gpu

namespace viz {
class DisplayProvider;
class FrameSinkManagerImpl;
class GpuServiceImpl;
class ServerSharedBitmapManager;

#if defined(OS_ANDROID)
using VizCompositorThreadType = base::android::JavaHandlerThread;
#else
using VizCompositorThreadType = base::Thread;
#endif

// Starts and runs the VizCompositorThread. The thread will be started when this
// object is constructed. Objects on the thread will be initialized after
// calling CreateFrameSinkManager(). Destructor will teardown objects on thread
// and then stop the thread.
class VIZ_SERVICE_EXPORT VizCompositorThreadRunner {
 public:
  VizCompositorThreadRunner();
  // Performs teardown on thread and then stops thread.
  ~VizCompositorThreadRunner();

  // Returns the TaskRunner for VizCompositorThread.
  base::SingleThreadTaskRunner* task_runner() { return task_runner_.get(); }

  // Creates FrameSinkManager from |params|. The version with |gpu_service| and
  // |task_executor| supports both GPU and software compositing, while the
  // version without supports only software compositing. Should be called from
  // the thread that owns |this| to initialize state on VizCompositorThread.
  void CreateFrameSinkManager(mojom::FrameSinkManagerParamsPtr params);
  void CreateFrameSinkManager(
      mojom::FrameSinkManagerParamsPtr params,
      scoped_refptr<gpu::CommandBufferTaskExecutor> task_executor,
      GpuServiceImpl* gpu_service);

  // Performs cleanup on VizCompositorThread needed before forcing thread to
  // shut down. Ensures VizCompositorThread teardown during the destructor
  // doesn't block on PostTasks back to the GPU thread. After cleanup has
  // finished |cleanup_finished_callback| will be run. Should be called from the
  // thread that owns |this|.
  //
  // This is intended to be used when the GPU thread wants to force restart. The
  // cleanup is normally handled by the browser process before GPU process
  // shutdown, except if the GPU thread is forcing restart.
  void CleanupForShutdown(base::OnceClosure cleanup_finished_callback);

 private:
  void CreateFrameSinkManagerOnCompositorThread(
      mojom::FrameSinkManagerParamsPtr params,
      scoped_refptr<gpu::CommandBufferTaskExecutor> task_executor,
      GpuServiceImpl* gpu_service,
      gpu::ImageFactory* image_factory,
      gpu::GpuChannelManager* gpu_channel_manager);
  void CleanupForShutdownOnCompositorThread();
  void TearDownOnCompositorThread();

  // Start variables to be accessed only on |task_runner_|.
  std::unique_ptr<ServerSharedBitmapManager> server_shared_bitmap_manager_;
  std::unique_ptr<DisplayProvider> display_provider_;
  std::unique_ptr<FrameSinkManagerImpl> frame_sink_manager_;
  // End variables to be accessed only on |task_runner_|.

  std::unique_ptr<VizCompositorThreadType> thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(VizCompositorThreadRunner);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_MAIN_VIZ_COMPOSITOR_THREAD_RUNNER_H_
