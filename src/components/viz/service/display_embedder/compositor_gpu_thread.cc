// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/compositor_gpu_thread.h"

#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/switches.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

namespace viz {

// static
std::unique_ptr<CompositorGpuThread> CompositorGpuThread::Create(
    gpu::GpuChannelManager* gpu_channel_manager,
    gpu::VulkanImplementation* vulkan_implementation,
    gpu::VulkanDeviceQueue* device_queue,
    bool enable_watchdog) {
  if (!features::IsDrDcEnabled())
    return nullptr;

#if defined(OS_ANDROID)
  // When using angle via enabling passthrough command decoder on android, angle
  // context virtualization group extension should be enabled. Also since angle
  // currently always enables this extension, we are adding DCHECK() to ensure
  // that instead of enabling/disabling DrDc based on the extension.
  if (gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE)
    DCHECK(gl::GLSurfaceEGL::IsANGLEContextVirtualizationSupported());
#endif

  scoped_refptr<VulkanContextProvider> vulkan_context_provider;
#if BUILDFLAG(ENABLE_VULKAN)
  // Create a VulkanContextProvider.
  if (vulkan_implementation && device_queue) {
    auto compositor_thread_device_queue =
        std::make_unique<gpu::VulkanDeviceQueue>(
            device_queue->GetVulkanInstance());
    compositor_thread_device_queue->InitializeForCompositorGpuThread(
        device_queue->GetVulkanPhysicalDevice(),
        device_queue->GetVulkanDevice(), device_queue->GetVulkanQueue(),
        device_queue->GetVulkanQueueIndex(),
        device_queue->enabled_extensions());
    vulkan_context_provider =
        VulkanInProcessContextProvider::CreateForCompositorGpuThread(
            vulkan_implementation, std::move(compositor_thread_device_queue),
            gpu_channel_manager->gpu_preferences()
                .vulkan_sync_cpu_memory_limit);
  }
#endif

  auto compositor_gpu_thread = base::WrapUnique(new CompositorGpuThread(
      gpu_channel_manager, std::move(vulkan_context_provider),
      enable_watchdog));

  if (!compositor_gpu_thread->Initialize())
    return nullptr;
  return compositor_gpu_thread;
}

CompositorGpuThread::CompositorGpuThread(
    gpu::GpuChannelManager* gpu_channel_manager,
    scoped_refptr<VulkanContextProvider> vulkan_context_provider,
    bool enable_watchdog)
    : base::Thread("CompositorGpuThread"),
      gpu_channel_manager_(gpu_channel_manager),
      enable_watchdog_(enable_watchdog),
      vulkan_context_provider_(std::move(vulkan_context_provider)),
      weak_ptr_factory_(this) {}

CompositorGpuThread::~CompositorGpuThread() {
  base::Thread::Stop();
}

bool CompositorGpuThread::Initialize() {
  // Setup thread options.
  base::Thread::Options thread_options(base::MessagePumpType::DEFAULT, 0);
  if (base::FeatureList::IsEnabled(features::kGpuUseDisplayThreadPriority))
    thread_options.priority = base::ThreadPriority::DISPLAY;
  StartWithOptions(std::move(thread_options));

  // Wait until threas is started and Init() is executed in order to return
  // updated |init_succeded_|.
  WaitUntilThreadStarted();
  return init_succeded_;
}

void CompositorGpuThread::Init() {
  const auto& gpu_preferences = gpu_channel_manager_->gpu_preferences();
  if (enable_watchdog_) {
    watchdog_thread_ = gpu::GpuWatchdogThread::Create(
        gpu_preferences.watchdog_starts_backgrounded, "GpuWatchdog_Compositor");
  }

  // Create a new share group. Note that this share group is different from the
  // share group which gpu main thread uses.
  auto share_group = base::MakeRefCounted<gl::GLShareGroup>();
  auto surface = gl::init::CreateOffscreenGLSurface(gfx::Size());

  const bool use_passthrough_decoder =
      gpu::gles2::PassthroughCommandDecoderSupported() &&
      gpu_preferences.use_passthrough_cmd_decoder;
  gl::GLContextAttribs attribs = gpu::gles2::GenerateGLContextAttribs(
      gpu::ContextCreationAttribs(), use_passthrough_decoder);
  attribs.angle_context_virtualization_group_number =
      gl::AngleContextVirtualizationGroup::kDrDc;

  // Create a new gl context. Note that this gl context is not part of same
  // share group which gpu main thread uses. Hence this context does not share
  // GL resources with the contexts created on gpu main thread.
  auto context =
      gl::init::CreateGLContext(share_group.get(), surface.get(), attribs);
  if (!context)
    return;

  const auto& gpu_feature_info = gpu_channel_manager_->gpu_feature_info();
  gpu_feature_info.ApplyToGLContext(context.get());

  if (!context->MakeCurrent(surface.get()))
    return;

  // Create a SharedContextState.
  shared_context_state_ = base::MakeRefCounted<gpu::SharedContextState>(
      std::move(share_group), std::move(surface), std::move(context),
      /*use_virtualized_gl_contexts=*/false,
      gpu_channel_manager_->GetContextLostCallback(),
      gpu_preferences.gr_context_type,
#if BUILDFLAG(ENABLE_VULKAN)
      vulkan_context_provider_.get(),
#else
      /*vulkan_context_provider=*/nullptr,
#endif
      /*metal_context_provider=*/nullptr,
      /*dawn_context_provider=*/nullptr,
      /*peak_memory_monitor=*/weak_ptr_factory_.GetWeakPtr());

  const auto& workarounds = gpu_channel_manager_->gpu_driver_bug_workarounds();
  auto gles2_feature_info = base::MakeRefCounted<gpu::gles2::FeatureInfo>(
      workarounds, gpu_feature_info);

  // Initialize GL.
  if (!shared_context_state_->InitializeGL(gpu_preferences,
                                           std::move(gles2_feature_info))) {
    return;
  }

  // Initialize GrContext.
  if (!shared_context_state_->InitializeGrContext(
          gpu_preferences, workarounds, gpu_channel_manager_->gr_shader_cache(),
          /*activity_flags=*/nullptr, /*progress_reporter=*/nullptr)) {
    return;
  }
  if (watchdog_thread_)
    watchdog_thread_->OnInitComplete();
  init_succeded_ = true;
}

void CompositorGpuThread::CleanUp() {
  if (watchdog_thread_)
    watchdog_thread_->OnGpuProcessTearDown();

  weak_ptr_factory_.InvalidateWeakPtrs();
  if (shared_context_state_) {
    shared_context_state_->MakeCurrent(nullptr);
    shared_context_state_ = nullptr;
  }

  // WatchDogThread destruction should happen on the CompositorGpuThread.
  watchdog_thread_.reset();
}

void CompositorGpuThread::OnMemoryAllocatedChange(
    gpu::CommandBufferId id,
    uint64_t old_size,
    uint64_t new_size,
    gpu::GpuPeakMemoryAllocationSource source) {
  gpu_channel_manager_->GetOnMemoryAllocatedChangeCallback().Run(
      id, old_size, new_size, source);
}

void CompositorGpuThread::OnBackgrounded() {
  if (watchdog_thread_)
    watchdog_thread_->OnBackgrounded();
}

void CompositorGpuThread::OnForegrounded() {
  if (watchdog_thread_)
    watchdog_thread_->OnForegrounded();
}

}  // namespace viz
