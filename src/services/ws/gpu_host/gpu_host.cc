// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/gpu_host/gpu_host.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/shared_memory.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/switches.h"
#include "components/viz/host/gpu_client.h"
#include "components/viz/host/gpu_client_delegate.h"
#include "components/viz/host/host_gpu_memory_buffer_manager.h"
#include "components/viz/service/main/viz_main_impl.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/host/shader_disk_cache.h"
#include "gpu/ipc/service/gpu_init.h"
#include "media/gpu/buildflags.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/viz/privileged/interfaces/viz_main.mojom.h"
#include "services/viz/public/interfaces/constants.mojom.h"
#include "services/ws/gpu_host/gpu_host_delegate.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/buffer_format_util.h"

#if defined(OS_WIN)
#include "ui/gfx/win/rendering_window_manager.h"
#endif

#if defined(OS_CHROMEOS)
#include "services/ws/gpu_host/arc_gpu_client.h"
#endif

#if defined(OS_CHROMEOS) && BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

namespace ws {
namespace gpu_host {

namespace {

// The client Id 1 is reserved for the frame sink manager.
const int32_t kInternalGpuChannelClientId = 2;

class GpuClientDelegate : public viz::GpuClientDelegate {
 public:
  GpuClientDelegate(viz::GpuHostImpl* gpu_host_impl,
                    viz::HostGpuMemoryBufferManager* gpu_memory_buffer_manager);
  ~GpuClientDelegate() override;

  // viz::GpuClientDelegate:
  viz::GpuHostImpl* EnsureGpuHost() override;
  viz::HostGpuMemoryBufferManager* GetGpuMemoryBufferManager() override;

 private:
  viz::GpuHostImpl* gpu_host_impl_;
  viz::HostGpuMemoryBufferManager* gpu_memory_buffer_manager_;

  DISALLOW_COPY_AND_ASSIGN(GpuClientDelegate);
};

GpuClientDelegate::GpuClientDelegate(
    viz::GpuHostImpl* gpu_host_impl,
    viz::HostGpuMemoryBufferManager* gpu_memory_buffer_manager)
    : gpu_host_impl_(gpu_host_impl),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager) {}

GpuClientDelegate::~GpuClientDelegate() = default;

viz::GpuHostImpl* GpuClientDelegate::EnsureGpuHost() {
  return gpu_host_impl_;
}

viz::HostGpuMemoryBufferManager*
GpuClientDelegate::GetGpuMemoryBufferManager() {
  return gpu_memory_buffer_manager_;
}

}  // namespace

GpuHost::GpuHost(GpuHostDelegate* delegate,
                 service_manager::Connector* connector,
                 discardable_memory::DiscardableSharedMemoryManager*
                     discardable_shared_memory_manager)
    : delegate_(delegate),
      discardable_shared_memory_manager_(discardable_shared_memory_manager),
      next_client_id_(kInternalGpuChannelClientId + 1),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      gpu_thread_("GpuThread") {
  DCHECK(discardable_shared_memory_manager_);

  viz::GpuHostImpl::InitFontRenderParams(
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr));

  bool in_process = !connector || !features::IsMashOopVizEnabled();

  viz::mojom::VizMainPtr viz_main_ptr;
  if (in_process) {
    // TODO(crbug.com/912221): This goes away after the gpu process split in
    // mash.
    gpu_thread_.Start();
    gpu_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuHost::InitializeVizMain, base::Unretained(this),
                       base::Passed(MakeRequest(&viz_main_ptr))));
  } else {
    connector->BindInterface(viz::mojom::kVizServiceName,
                             MakeRequest(&viz_main_ptr));
  }

  viz::GpuHostImpl::InitParams params;
  params.restart_id = viz::BeginFrameSource::kNotRestartableId + 1;
  params.in_process = in_process;
  params.disable_gpu_shader_disk_cache =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache);
  params.deadline_to_synchronize_surfaces =
      switches::GetDeadlineToSynchronizeSurfaces();
  params.main_thread_task_runner = main_thread_task_runner_;
  gpu_host_impl_ = std::make_unique<viz::GpuHostImpl>(
      this, std::make_unique<viz::VizMainWrapper>(std::move(viz_main_ptr)),
      std::move(params));

#if defined(OS_WIN)
  // For OS_WIN the process id for GPU is needed. Using GetCurrentProcessId()
  // only works with in-process GPU, which is fine because GpuHost isn't used
  // outside of tests.
  gpu_host_impl_->OnProcessLaunched(::GetCurrentProcessId());
#endif

  gpu_memory_buffer_manager_ =
      std::make_unique<viz::HostGpuMemoryBufferManager>(
          base::BindRepeating(
              [](viz::mojom::GpuService* gpu_service,
                 base::OnceClosure connection_error_handler) {
                return gpu_service;
              },
              gpu_host_impl_->gpu_service()),
          next_client_id_++, std::make_unique<gpu::GpuMemoryBufferSupport>(),
          main_thread_task_runner_);

  shader_cache_factory_ = std::make_unique<gpu::ShaderCacheFactory>();
}

GpuHost::~GpuHost() {
  // TODO(crbug.com/912221): This goes away after the gpu process split in mash.
  if (gpu_thread_.IsRunning()) {
    // Stop() will return after |viz_main_impl_| has been destroyed.
    gpu_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuHost::DestroyVizMain, base::Unretained(this)));
    gpu_thread_.Stop();
  }

  viz::GpuHostImpl::ResetFontRenderParams();
}

void GpuHost::CreateFrameSinkManager(
    viz::mojom::FrameSinkManagerRequest request,
    viz::mojom::FrameSinkManagerClientPtrInfo client) {
  gpu_host_impl_->ConnectFrameSinkManager(std::move(request),
                                          std::move(client));
}

void GpuHost::Shutdown() {
  gpu_clients_.clear();
  gpu_host_impl_.reset();
}

void GpuHost::Add(mojom::GpuRequest request) {
  const int client_id = next_client_id_++;
  const uint64_t client_tracing_id = 0;
  auto client = std::make_unique<viz::GpuClient>(
      std::make_unique<GpuClientDelegate>(gpu_host_impl_.get(),
                                          gpu_memory_buffer_manager_.get()),
      client_id, client_tracing_id, main_thread_task_runner_);
  client->Add(std::move(request));
  gpu_clients_.push_back(std::move(client));
}

#if defined(OS_CHROMEOS)
void GpuHost::AddArcGpu(mojom::ArcGpuRequest request) {
  arc_gpu_bindings_.AddBinding(
      std::make_unique<ArcGpuClient>(gpu_host_impl_->gpu_service()),
      std::move(request));
}
#endif  // defined(OS_CHROMEOS)

#if defined(USE_OZONE)
void GpuHost::BindOzoneGpuInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // This is only used when viz is run in-process.
  DCHECK(gpu_thread_.IsRunning());

  // Interfaces should be bound on gpu thread.
  if (!gpu_thread_.task_runner()->BelongsToCurrentThread()) {
    gpu_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuHost::BindOzoneGpuInterface, base::Unretained(this),
                       interface_name, std::move(interface_pipe)));
    return;
  }
  DCHECK(viz_main_impl_);
  viz_main_impl_->BindInterface(interface_name, std::move(interface_pipe));
}
#endif  // defined(USE_OZONE)

void GpuHost::OnBadMessageFromGpu() {
  // TODO(sad): Received some unexpected message from the gpu process. We
  // should kill the process and restart it.
  NOTIMPLEMENTED();
}

void GpuHost::InitializeVizMain(viz::mojom::VizMainRequest request) {
  gpu::GpuPreferences gpu_preferences;
  gpu_preferences.gpu_program_cache_size =
      gpu::ShaderDiskCache::CacheSizeBytes();
  gpu_preferences.texture_target_exception_list =
      gpu::CreateBufferUsageAndFormatExceptionList();

#if defined(OS_CHROMEOS) && BUILDFLAG(USE_VAAPI)
  // Initialize media codec. The UI service is running in a privileged process.
  // We don't need care when to initialize media codec.
  media::VaapiWrapper::PreSandboxInitialization();
#endif

  auto gpu_init = std::make_unique<gpu::GpuInit>();
  gpu_init->InitializeInProcess(base::CommandLine::ForCurrentProcess(),
                                gpu_preferences);

  viz::VizMainImpl::ExternalDependencies deps;
  deps.create_display_compositor = true;
  viz_main_impl_ = std::make_unique<viz::VizMainImpl>(nullptr, std::move(deps),
                                                      std::move(gpu_init));
  viz_main_impl_->Bind(std::move(request));
}

void GpuHost::DestroyVizMain() {
  DCHECK(viz_main_impl_);
  viz_main_impl_.reset();
}

gpu::GPUInfo GpuHost::GetGPUInfo() const {
  return gpu_info_;
}

gpu::GpuFeatureInfo GpuHost::GetGpuFeatureInfo() const {
  return gpu_feature_info_;
}

void GpuHost::DidInitialize(
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
    const base::Optional<gpu::GpuFeatureInfo>&
        gpu_feature_info_for_hardware_gpu) {
  gpu_info_ = gpu_info;
  gpu_feature_info_ = gpu_feature_info;
  delegate_->OnGpuServiceInitialized();
}

void GpuHost::DidFailInitialize() {}

void GpuHost::DidCreateContextSuccessfully() {}

void GpuHost::BlockDomainFrom3DAPIs(const GURL& url, gpu::DomainGuilt guilt) {}

void GpuHost::DisableGpuCompositing() {}

bool GpuHost::GpuAccessAllowed() const {
  return true;
}

gpu::ShaderCacheFactory* GpuHost::GetShaderCacheFactory() {
  return shader_cache_factory_.get();
}

void GpuHost::RecordLogMessage(int32_t severity,
                               const std::string& header,
                               const std::string& message) {}

void GpuHost::BindDiscardableMemoryRequest(
    discardable_memory::mojom::DiscardableSharedMemoryManagerRequest request) {
  service_manager::BindSourceInfo source_info;
  discardable_shared_memory_manager_->Bind(std::move(request), source_info);
}

void GpuHost::BindInterface(const std::string& interface_name,
                            mojo::ScopedMessagePipeHandle interface_pipe) {
  NOTREACHED();
}

void GpuHost::RunService(
    const std::string& service_name,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver) {
  NOTREACHED();
}

#if defined(USE_OZONE)
void GpuHost::TerminateGpuProcess(const std::string& message) {}

void GpuHost::SendGpuProcessMessage(IPC::Message* message) {}
#endif

}  // namespace gpu_host
}  // namespace ws
