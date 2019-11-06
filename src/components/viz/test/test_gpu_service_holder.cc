// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_gpu_service_holder.h"

#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

namespace viz {

namespace {

base::Lock& GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

// We expect |GetLock()| to be acquired before accessing this variable.
TestGpuServiceHolder* g_holder = nullptr;

class InstanceResetter : public testing::EmptyTestEventListener {
 public:
  InstanceResetter() = default;
  ~InstanceResetter() override = default;

  void OnTestEnd(const testing::TestInfo& test_info) override {
    base::AutoLock locked(GetLock());
    if (g_holder) {
      delete g_holder;
      g_holder = nullptr;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InstanceResetter);
};

}  // namespace

// static
TestGpuServiceHolder* TestGpuServiceHolder::GetInstance() {
  base::AutoLock locked(GetLock());

  // Make sure all TestGpuServiceHolders are deleted at process exit.
  static bool registered_cleanup = false;
  if (!registered_cleanup) {
    registered_cleanup = true;
    base::AtExitManager::RegisterTask(base::BindOnce([]() {
      if (g_holder)
        delete g_holder;
    }));
  }

  if (!g_holder) {
    g_holder = new TestGpuServiceHolder(
        gpu::gles2::ParseGpuPreferences(base::CommandLine::ForCurrentProcess()),
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kUseGpuInTests));
  }
  return g_holder;
}

// static
void TestGpuServiceHolder::DestroyInstanceAfterEachTest() {
  static bool registered_listener = false;
  if (!registered_listener) {
    registered_listener = true;
    testing::TestEventListeners& listeners =
        testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new InstanceResetter);
  }
}

TestGpuServiceHolder::TestGpuServiceHolder(
    const gpu::GpuPreferences& gpu_preferences,
    bool use_swiftshader_for_vulkan)
    : gpu_thread_("GPUMainThread"), io_thread_("GPUIOThread") {
  CHECK(gpu_thread_.Start());
  CHECK(io_thread_.Start());

  base::WaitableEvent completion;
  gpu_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestGpuServiceHolder::InitializeOnGpuThread,
                                base::Unretained(this), gpu_preferences,
                                use_swiftshader_for_vulkan, &completion));
  completion.Wait();
}

TestGpuServiceHolder::~TestGpuServiceHolder() {
  // Ensure members created on GPU thread are destroyed there too.
  gpu_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestGpuServiceHolder::DeleteOnGpuThread,
                                base::Unretained(this)));
  gpu_thread_.Stop();
  io_thread_.Stop();
}

void TestGpuServiceHolder::InitializeOnGpuThread(
    const gpu::GpuPreferences& gpu_preferences,
    bool use_swiftshader_for_vulkan,
    base::WaitableEvent* completion) {
  DCHECK(gpu_thread_.task_runner()->BelongsToCurrentThread());

  if (gpu_preferences.enable_vulkan) {
#if BUILDFLAG(ENABLE_VULKAN)
#ifndef USE_X11
    // TODO(samans): Support Swiftshader on more platforms.
    // https://crbug.com/963988
    LOG_IF(ERROR, use_swiftshader_for_vulkan)
        << "Unable to use Vulkan Swiftshader on this platform. Falling back to "
           "GPU.";
    use_swiftshader_for_vulkan = false;
#endif
    vulkan_implementation_ =
        gpu::CreateVulkanImplementation(use_swiftshader_for_vulkan);
    if (!vulkan_implementation_ ||
        !vulkan_implementation_->InitializeVulkanInstance(
            !gpu_preferences.disable_vulkan_surface)) {
      LOG(FATAL) << "Failed to create and initialize Vulkan implementation.";
    }
#else
    NOTREACHED();
#endif
  }

  // Always enable gpu and oop raster, regardless of platform and blacklist.
  // The latter instructs GpuChannelManager::GetSharedContextState to create a
  // GrContext, which is required by SkiaRenderer as well as OOP-R.
  gpu::GpuFeatureInfo gpu_feature_info;
  gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION] =
      gpu::kGpuFeatureStatusEnabled;
  gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_OOP_RASTERIZATION] =
      gpu::kGpuFeatureStatusEnabled;

  // TODO(sgilhuly): Investigate why creating a GPUInfo and GpuFeatureInfo from
  // the command line causes the test SkiaOutputSurfaceImplTest.SubmitPaint to
  // fail on Android.
  gpu_service_ = std::make_unique<GpuServiceImpl>(
      gpu::GPUInfo(), /*watchdog_thread=*/nullptr, io_thread_.task_runner(),
      gpu_feature_info, gpu_preferences,
      /*gpu_info_for_hardware_gpu=*/gpu::GPUInfo(),
      /*gpu_feature_info_for_hardware_gpu=*/gpu::GpuFeatureInfo(),
#if BUILDFLAG(ENABLE_VULKAN)
      vulkan_implementation_.get(),
#else
      /*vulkan_implementation=*/nullptr,
#endif
      /*exit_callback=*/base::DoNothing());

  // Use a disconnected mojo pointer, we don't need to receive any messages.
  mojom::GpuHostPtr gpu_host_proxy;
  mojo::MakeRequest(&gpu_host_proxy);
  gpu_service_->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessActivityFlags(),
      gl::init::CreateOffscreenGLSurface(gfx::Size()),
      /*sync_point_manager=*/nullptr, /*shared_image_manager=*/nullptr,
      /*shutdown_event=*/nullptr);

  task_executor_ = std::make_unique<gpu::GpuInProcessThreadService>(
      gpu_thread_.task_runner(), gpu_service_->scheduler(),
      gpu_service_->sync_point_manager(), gpu_service_->mailbox_manager(),
      gpu_service_->share_group(),
      gpu_service_->gpu_channel_manager()
          ->default_offscreen_surface()
          ->GetFormat(),
      gpu_service_->gpu_feature_info(),
      gpu_service_->gpu_channel_manager()->gpu_preferences(),
      gpu_service_->shared_image_manager(),
      gpu_service_->gpu_channel_manager()->program_cache(),
      gpu_service_->GetContextState());

  completion->Signal();
}

void TestGpuServiceHolder::DeleteOnGpuThread() {
  task_executor_.reset();
  gpu_service_.reset();
}

}  // namespace viz
