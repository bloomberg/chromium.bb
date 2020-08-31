// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_GPU_HOST_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_GPU_HOST_H_

#include <inttypes.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/mojom/scenic_gpu_host.mojom.h"
#include "ui/ozone/public/mojom/scenic_gpu_service.mojom.h"

namespace ui {
class ScenicWindowManager;

// Browser process object which supports a GPU process host.
//
// Once a GPU process starts, this objects binds to it over mojo and
// enables exchange of Scenic resources with it. In particular, we must
// exchange export tokens for each view surface, so that the surface can
// present to Scenic views managed by the browser.
class ScenicGpuHost : public mojom::ScenicGpuHost,
                      public GpuPlatformSupportHost {
 public:
  ScenicGpuHost(ScenicWindowManager* scenic_window_manager);
  ~ScenicGpuHost() override;

  // Binds the receiver for the main process surface factory.
  void Initialize(mojo::PendingReceiver<mojom::ScenicGpuHost> pending_receiver);

  // Shuts down mojo service. After calling shutdown, it's safe to call
  // Initialize() again.
  void Shutdown();

  // mojom::ScenicGpuHost:
  void AttachSurfaceToWindow(
      int32_t window_id,
      mojo::PlatformHandle surface_view_holder_token_mojo) override;

  // GpuPlatformSupportHost:
  void OnChannelDestroyed(int host_id) override;
  void OnGpuServiceLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      GpuHostBindInterfaceCallback binder,
      GpuHostTerminateCallback terminate_callback) override;

 private:
  void OnGpuServiceLaunchedOnUI(
      mojo::PendingRemote<mojom::ScenicGpuService> gpu_service);
  void UpdateReceiver(uint32_t service_launch_count,
                      mojo::PendingReceiver<mojom::ScenicGpuHost> receiver);

  ScenicWindowManager* const scenic_window_manager_;
  mojo::Receiver<mojom::ScenicGpuHost> host_receiver_{this};
  mojo::Receiver<mojom::ScenicGpuHost> gpu_receiver_{this};

  mojo::Remote<mojom::ScenicGpuService> gpu_service_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_runner_;

  THREAD_CHECKER(ui_thread_checker_);
  THREAD_CHECKER(io_thread_checker_);

  base::WeakPtrFactory<ScenicGpuHost> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ScenicGpuHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_GPU_HOST_H_
