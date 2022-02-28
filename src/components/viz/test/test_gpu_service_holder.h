// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_GPU_SERVICE_HOLDER_H_
#define COMPONENTS_VIZ_TEST_TEST_GPU_SERVICE_HOLDER_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/vulkan/buildflags.h"

#if defined(USE_OZONE) && !defined(OS_FUCHSIA)
#include "mojo/public/cpp/bindings/binder_map.h"
#endif

namespace gpu {
class CommandBufferTaskExecutor;
class SingleTaskSequence;
#if BUILDFLAG(ENABLE_VULKAN)
class VulkanImplementation;
#endif
struct GpuPreferences;
}  // namespace gpu

namespace viz {
class GpuServiceImpl;

// Starts GPU Main and IO threads, and creates a GpuServiceImpl that can be used
// to create a SkiaOutputSurfaceImpl. This isn't a full GPU service
// implementation and should only be used in tests.
class TestGpuServiceHolder : public gpu::GpuInProcessThreadServiceDelegate {
 public:
  class ScopedResetter {
   public:
    ~ScopedResetter() { TestGpuServiceHolder::ResetInstance(); }
  };
  // Exposes a singleton to allow easy sharing of the GpuServiceImpl by
  // different clients (e.g. to share SharedImages via a common
  // SharedImageManager).
  //
  // The instance will parse GpuPreferences from the command line when it is
  // first created (e.g. to allow entire test suite with --use-vulkan).
  //
  // If specific feature flags or GpuPreferences are needed for a specific test,
  // a separate instance of this class can be created.
  //
  // By default the instance created by GetInstance() is destroyed after each
  // gtest completes -- it only applies to gtest because it uses gtest hooks. If
  // this isn't desired call DoNotResetOnTestExit() before first use.
  static TestGpuServiceHolder* GetInstance();

  // Resets the singleton instance, joining the GL thread. This is useful for
  // tests that individually initialize and tear down GL.
  static void ResetInstance();

  // Don't reset global instance on gtest exit. Must be called before
  // GetInstance().
  static void DoNotResetOnTestExit();

  explicit TestGpuServiceHolder(const gpu::GpuPreferences& preferences);

  TestGpuServiceHolder(const TestGpuServiceHolder&) = delete;
  TestGpuServiceHolder& operator=(const TestGpuServiceHolder&) = delete;

  ~TestGpuServiceHolder() override;

  scoped_refptr<base::SingleThreadTaskRunner> gpu_thread_task_runner() {
    return gpu_thread_.task_runner();
  }

  // Most of |gpu_service_| is not safe to use off of the GPU thread, be careful
  // when accessing this.
  GpuServiceImpl* gpu_service() { return gpu_service_.get(); }

  gpu::CommandBufferTaskExecutor* task_executor() {
    return task_executor_.get();
  }

  void ScheduleGpuTask(base::OnceClosure callback);

  bool is_vulkan_enabled() {
#if BUILDFLAG(ENABLE_VULKAN)
    return !!vulkan_implementation_;
#else
    return false;
#endif
  }

  // gpu::GpuInProcessThreadServiceDelegate implementation:
  scoped_refptr<gpu::SharedContextState> GetSharedContextState() override;
  scoped_refptr<gl::GLShareGroup> GetShareGroup() override;

 private:
  void InitializeOnGpuThread(const gpu::GpuPreferences& preferences,
                             base::WaitableEvent* completion);
  void DeleteOnGpuThread();

// TODO(crbug.com/1267788): Fuchsia crashes. See details in the crbug.
#if defined(USE_OZONE) && !defined(OS_FUCHSIA)
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe);
  void BindInterfaceOnGpuThread(const std::string& interface_name,
                                mojo::ScopedMessagePipeHandle interface_pipe);
#endif

#if !defined(OS_CHROMEOS)
  // TODO(crbug.com/1241161): This is equally applicable to Chrome OS there are
  // just a number of tests that already override the feature list after it's no
  // longer safe that need to be fixed first.
  base::FeatureList::ScopedDisallowOverrides disallow_feature_overrides{
      "FeatureList overrides must happen before the GPU service thread has "
      "been started."};
#endif

  base::Thread gpu_thread_;
  base::Thread io_thread_;

  // These should only be created and deleted on the gpu thread.
  std::unique_ptr<GpuServiceImpl> gpu_service_;
  std::unique_ptr<gpu::CommandBufferTaskExecutor> task_executor_;
  // This is used to schedule gpu tasks in sequence.
  std::unique_ptr<gpu::SingleTaskSequence> gpu_task_sequence_;
#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation_;
#endif

#if defined(USE_OZONE) && !defined(OS_FUCHSIA)
  // Bound interfaces.
  mojo::BinderMap binders_;
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_GPU_SERVICE_HOLDER_H_
