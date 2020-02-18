// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_
#define CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "chrome/common/profiler/thread_profiler.h"
#include "content/public/gpu/content_gpu_client.h"

#if defined(OS_CHROMEOS)
namespace arc {
class ProtectedBufferManager;
}  // namespace arc
#endif

class ChromeContentGpuClient : public content::ContentGpuClient {
 public:
  ChromeContentGpuClient();
  ~ChromeContentGpuClient() override;

  // content::ContentGpuClient:
  void GpuServiceInitialized() override;
  void ExposeInterfacesToBrowser(const gpu::GpuPreferences& gpu_preferences,
                                 mojo::BinderMap* binders) override;
  void PostIOThreadCreated(
      base::SingleThreadTaskRunner* io_task_runner) override;
  void PostCompositorThreadCreated(
      base::SingleThreadTaskRunner* task_runner) override;

#if BUILDFLAG(ENABLE_CDM_PROXY)
  std::unique_ptr<media::CdmProxy> CreateCdmProxy(
      const base::Token& cdm_guid) override;
#endif

#if defined(OS_CHROMEOS)
  scoped_refptr<arc::ProtectedBufferManager> GetProtectedBufferManager();
#endif

 private:
  // Used to profile main thread startup.
  std::unique_ptr<ThreadProfiler> main_thread_profiler_;

#if defined(OS_CHROMEOS)
  scoped_refptr<arc::ProtectedBufferManager> protected_buffer_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeContentGpuClient);
};

#endif  // CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_
