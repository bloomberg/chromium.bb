// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/gpu_host_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/host/shader_disk_cache.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/font_render_params.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(OS_WIN)
#include "ui/gfx/win/rendering_window_manager.h"
#elif defined(OS_MAC)
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace viz {
namespace {

// A wrapper around gfx::FontRenderParams that checks it is set and accessed on
// the same thread.
class FontRenderParams {
 public:
  void Set(const gfx::FontRenderParams& params);
  void Reset();
  const absl::optional<gfx::FontRenderParams>& Get();

 private:
  friend class base::NoDestructor<FontRenderParams>;

  FontRenderParams();
  ~FontRenderParams();

  THREAD_CHECKER(thread_checker_);
  absl::optional<gfx::FontRenderParams> params_;

  DISALLOW_COPY_AND_ASSIGN(FontRenderParams);
};

void FontRenderParams::Set(const gfx::FontRenderParams& params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  params_ = params;
}

void FontRenderParams::Reset() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  params_ = absl::nullopt;
}

const absl::optional<gfx::FontRenderParams>& FontRenderParams::Get() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return params_;
}

FontRenderParams::FontRenderParams() = default;

FontRenderParams::~FontRenderParams() {
  NOTREACHED();
}

FontRenderParams& GetFontRenderParams() {
  static base::NoDestructor<FontRenderParams> instance;
  return *instance;
}

}  // namespace

GpuHostImpl::InitParams::InitParams() = default;

GpuHostImpl::InitParams::InitParams(InitParams&&) = default;

GpuHostImpl::InitParams::~InitParams() = default;

GpuHostImpl::GpuHostImpl(Delegate* delegate,
                         mojo::PendingRemote<mojom::VizMain> viz_main,
                         InitParams params)
    : delegate_(delegate),
      viz_main_(std::move(viz_main)),
      params_(std::move(params)),
      host_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  // Create a special GPU info collection service if the GPU process is used for
  // info collection only.
#if defined(OS_WIN)
  if (params.info_collection_gpu_process) {
    viz_main_->CreateInfoCollectionGpuService(
        info_collection_gpu_service_remote_.BindNewPipeAndPassReceiver());
    return;
  }
#endif

  DCHECK(delegate_);

  mojo::PendingRemote<discardable_memory::mojom::DiscardableSharedMemoryManager>
      discardable_manager_remote;
  delegate_->BindDiscardableMemoryReceiver(
      discardable_manager_remote.InitWithNewPipeAndPassReceiver());

  DCHECK(GetFontRenderParams().Get());
  scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr;
#if defined(OS_MAC)
  if (params_.main_thread_task_runner->BelongsToCurrentThread())
    task_runner = ui::WindowResizeHelperMac::Get()->task_runner();
#endif

  viz_main_->CreateGpuService(
      gpu_service_remote_.BindNewPipeAndPassReceiver(task_runner),
      gpu_host_receiver_.BindNewPipeAndPassRemote(task_runner),
      std::move(discardable_manager_remote), activity_flags_.CloneHandle(),
      GetFontRenderParams().Get()->subpixel_rendering);

#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform())
    InitOzone();
#endif  // defined(USE_OZONE)
}

GpuHostImpl::~GpuHostImpl() {
  SendOutstandingReplies();
}

// static
void GpuHostImpl::InitFontRenderParams(const gfx::FontRenderParams& params) {
  DCHECK(!GetFontRenderParams().Get());
  GetFontRenderParams().Set(params);
}

// static
void GpuHostImpl::ResetFontRenderParams() {
  DCHECK(GetFontRenderParams().Get());
  GetFontRenderParams().Reset();
}

void GpuHostImpl::SetProcessId(base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(base::kNullProcessId, pid_);
  DCHECK_NE(base::kNullProcessId, pid);
  pid_ = pid;
}

void GpuHostImpl::OnProcessCrashed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the GPU process crashed while compiling a shader, we may have invalid
  // cached binaries. Completely clear the shader cache to force shader binaries
  // to be re-created.
  if (activity_flags_.IsFlagSet(
          gpu::ActivityFlagsBase::FLAG_LOADING_PROGRAM_BINARY)) {
    auto* shader_cache_factory = delegate_->GetShaderCacheFactory();
    for (auto cache_key : client_id_to_shader_cache_) {
      // This call will temporarily extend the lifetime of the cache (kept
      // alive in the factory), and may drop loads of cached shader binaries if
      // it takes a while to complete. As we are intentionally dropping all
      // binaries, this behavior is fine.
      shader_cache_factory->ClearByClientId(
          cache_key.first, base::Time(), base::Time::Max(), base::DoNothing());
    }
  }
}

void GpuHostImpl::AddConnectionErrorHandler(base::OnceClosure handler) {
  connection_error_handlers_.push_back(std::move(handler));
}

void GpuHostImpl::BlockLiveOffscreenContexts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto iter = urls_with_live_offscreen_contexts_.begin();
       iter != urls_with_live_offscreen_contexts_.end(); ++iter) {
    delegate_->BlockDomainFrom3DAPIs(*iter, gpu::DomainGuilt::kUnknown);
  }
}

void GpuHostImpl::ConnectFrameSinkManager(
    mojo::PendingReceiver<mojom::FrameSinkManager> receiver,
    mojo::PendingRemote<mojom::FrameSinkManagerClient> client,
    const DebugRendererSettings& debug_renderer_settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::ConnectFrameSinkManager");

  mojom::FrameSinkManagerParamsPtr params =
      mojom::FrameSinkManagerParams::New();
  params->restart_id = params_.restart_id;
  params->use_activation_deadline =
      params_.deadline_to_synchronize_surfaces.has_value();
  params->activation_deadline_in_frames =
      params_.deadline_to_synchronize_surfaces.value_or(0u);
  params->frame_sink_manager = std::move(receiver);
  params->frame_sink_manager_client = std::move(client);
  params->debug_renderer_settings = debug_renderer_settings;
  viz_main_->CreateFrameSinkManager(std::move(params));
}

#if BUILDFLAG(USE_VIZ_DEVTOOLS)
void GpuHostImpl::ConnectVizDevTools(mojom::VizDevToolsParamsPtr params) {
  viz_main_->CreateVizDevTools(std::move(params));
}
#endif

void GpuHostImpl::EstablishGpuChannel(int client_id,
                                      uint64_t client_tracing_id,
                                      bool is_gpu_host,
                                      bool sync,
                                      EstablishChannelCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::EstablishGpuChannel");

  shutdown_timeout_.Stop();

  // If GPU features are already blocklisted, no need to establish the channel.
  if (!delegate_->GpuAccessAllowed()) {
    DVLOG(1) << "GPU access blocked, refusing to open a GPU channel.";
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            EstablishChannelStatus::kGpuAccessDenied);
    return;
  }

  if (gpu::IsReservedClientId(client_id)) {
    // The display-compositor/GrShaderCache in the gpu process uses these
    // special client ids.
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            EstablishChannelStatus::kGpuAccessDenied);
    return;
  }

  bool cache_shaders_on_disk =
      delegate_->GetShaderCacheFactory()->Get(client_id) != nullptr;

  channel_requests_[client_id] = std::move(callback);
  if (sync) {
    mojo::ScopedMessagePipeHandle channel_handle;
    gpu::GPUInfo gpu_info;
    gpu::GpuFeatureInfo gpu_feature_info;
    {
      mojo::SyncCallRestrictions::ScopedAllowSyncCall scoped_allow;
      gpu_service_remote_->EstablishGpuChannel(
          client_id, client_tracing_id, is_gpu_host, cache_shaders_on_disk,
          &channel_handle, &gpu_info, &gpu_feature_info);
    }
    OnChannelEstablished(client_id, true, std::move(channel_handle), gpu_info,
                         gpu_feature_info);
  } else {
    gpu_service_remote_->EstablishGpuChannel(
        client_id, client_tracing_id, is_gpu_host, cache_shaders_on_disk,
        base::BindOnce(&GpuHostImpl::OnChannelEstablished,
                       weak_ptr_factory_.GetWeakPtr(), client_id, false));
  }

  if (!params_.disable_gpu_shader_disk_cache)
    CreateChannelCache(client_id);
}

void GpuHostImpl::CloseChannel(int client_id) {
  gpu_service_remote_->CloseChannel(client_id);

  channel_requests_.erase(client_id);
}
#if BUILDFLAG(USE_VIZ_DEBUGGER)
void GpuHostImpl::FilterVisualDebugStream(base::Value json) {
  viz_main_->FilterDebugStream(std::move(json));
}

void GpuHostImpl::StartVisualDebugStream(
    base::RepeatingCallback<void(base::Value)> callback) {
  viz_debug_output_callback_ = std::move(callback);
  viz_main_->StartDebugStream(viz_debug_output_.BindNewPipeAndPassRemote());
}

void GpuHostImpl::StopVisualDebugStream() {
  viz_main_->StopDebugStream();
  viz_debug_output_.reset();
}
#endif

void GpuHostImpl::SendOutstandingReplies() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& handler : connection_error_handlers_)
    std::move(handler).Run();
  connection_error_handlers_.clear();

  // Send empty channel handles for all EstablishChannel requests.
  for (auto& entry : channel_requests_) {
    std::move(entry.second)
        .Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
             gpu::GpuFeatureInfo(), EstablishChannelStatus::kGpuHostInvalid);
  }
  channel_requests_.clear();
}

void GpuHostImpl::BindInterface(const std::string& interface_name,
                                mojo::ScopedMessagePipeHandle interface_pipe) {
  delegate_->BindInterface(interface_name, std::move(interface_pipe));
}

mojom::GpuService* GpuHostImpl::gpu_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(gpu_service_remote_.is_bound());
  return gpu_service_remote_.get();
}

#if defined(OS_WIN)
mojom::InfoCollectionGpuService* GpuHostImpl::info_collection_gpu_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(info_collection_gpu_service_remote_.is_bound());
  return info_collection_gpu_service_remote_.get();
}
#endif

#if defined(USE_OZONE)

void GpuHostImpl::InitOzone() {
  DCHECK(features::IsUsingOzonePlatform());
  // Ozone needs to send the primary DRM device to GPU service as early as
  // possible to ensure the latter always has a valid device.
  // https://crbug.com/608839
  //
  // The Ozone/Wayland requires mojo communication to be established to be
  // functional with a separate gpu process. Thus, using the PlatformProperties,
  // check if there is such a requirement.
  auto interface_binder = base::BindRepeating(&GpuHostImpl::BindInterface,
                                              weak_ptr_factory_.GetWeakPtr());
  auto terminate_callback = base::BindOnce(&GpuHostImpl::TerminateGpuProcess,
                                           weak_ptr_factory_.GetWeakPtr());

  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnGpuServiceLaunched(params_.restart_id,
                             params_.main_thread_task_runner,
                             host_thread_task_runner_, interface_binder,
                             std::move(terminate_callback));
}

void GpuHostImpl::TerminateGpuProcess(const std::string& message) {
  delegate_->TerminateGpuProcess(message);
}

#endif  // defined(USE_OZONE)

std::string GpuHostImpl::GetShaderPrefixKey() {
  if (shader_prefix_key_.empty()) {
    const gpu::GPUInfo& info = delegate_->GetGPUInfo();
    const gpu::GPUInfo::GPUDevice& active_gpu = info.active_gpu();

    shader_prefix_key_ = params_.product + "-" + info.gl_vendor + "-" +
                         info.gl_renderer + "-" + active_gpu.driver_version +
                         "-" + active_gpu.driver_vendor;

#if defined(OS_ANDROID)
    std::string build_fp =
        base::android::BuildInfo::GetInstance()->android_build_fp();
    shader_prefix_key_ += "-" + build_fp;
#endif
  }

  return shader_prefix_key_;
}

void GpuHostImpl::LoadedShader(int32_t client_id,
                               const std::string& key,
                               const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string prefix = GetShaderPrefixKey();
  bool prefix_ok = !key.compare(0, prefix.length(), prefix);
  UMA_HISTOGRAM_BOOLEAN("GPU.ShaderLoadPrefixOK", prefix_ok);
  if (prefix_ok) {
    // Remove the prefix from the key before load.
    std::string key_no_prefix = key.substr(prefix.length() + 1);
    gpu_service_remote_->LoadedShader(client_id, key_no_prefix, data);
  }
}

void GpuHostImpl::CreateChannelCache(int32_t client_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::CreateChannelCache");

  scoped_refptr<gpu::ShaderDiskCache> cache =
      delegate_->GetShaderCacheFactory()->Get(client_id);
  if (!cache)
    return;

  cache->set_shader_loaded_callback(base::BindRepeating(
      &GpuHostImpl::LoadedShader, weak_ptr_factory_.GetWeakPtr(), client_id));

  client_id_to_shader_cache_[client_id] = cache;
}

void GpuHostImpl::OnChannelEstablished(
    int client_id,
    bool sync,
    mojo::ScopedMessagePipeHandle channel_handle,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::OnChannelEstablished");

  auto it = channel_requests_.find(client_id);
  if (it == channel_requests_.end())
    return;

  auto callback = std::move(it->second);
  channel_requests_.erase(it);

  // Currently if any of the GPU features are blocklisted, we don't establish a
  // GPU channel.
  if (channel_handle.is_valid() && !delegate_->GpuAccessAllowed()) {
    gpu_service_remote_->CloseChannel(client_id);
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            EstablishChannelStatus::kGpuAccessDenied);
    RecordLogMessage(logging::LOG_WARNING, "WARNING",
                     "Hardware acceleration is unavailable.");
    return;
  }

  // TODO(jam): always use GPUInfo & GpuFeatureInfo from the service once we
  // know there's no issue with the ProcessHostOnUI which is the only mode
  // that currently uses it. This is because in that mode the sync mojo call
  // in the caller means we won't get the async DidInitialize() call before
  // this point, so the delegate_ methods won't have the GPU info structs yet.
  if (sync) {
    std::move(callback).Run(std::move(channel_handle), gpu_info,
                            gpu_feature_info, EstablishChannelStatus::kSuccess);
  } else {
    std::move(callback).Run(std::move(channel_handle), delegate_->GetGPUInfo(),
                            delegate_->GetGpuFeatureInfo(),
                            EstablishChannelStatus::kSuccess);
  }
}

void GpuHostImpl::DidInitialize(
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const absl::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
    const absl::optional<gpu::GpuFeatureInfo>&
        gpu_feature_info_for_hardware_gpu,
    const gfx::GpuExtraInfo& gpu_extra_info) {
  UMA_HISTOGRAM_BOOLEAN("GPU.GPUProcessInitialized", true);

  // Set GPU driver bug workaround flags that are checked on the browser side.
  wake_up_gpu_before_drawing_ =
      gpu_feature_info.IsWorkaroundEnabled(gpu::WAKE_UP_GPU_BEFORE_DRAWING);

  delegate_->DidInitialize(gpu_info, gpu_feature_info,
                           gpu_info_for_hardware_gpu,
                           gpu_feature_info_for_hardware_gpu, gpu_extra_info);

  if (!params_.disable_gpu_shader_disk_cache) {
    CreateChannelCache(gpu::kDisplayCompositorClientId);

    bool use_gr_shader_cache = base::FeatureList::IsEnabled(
                                   features::kDefaultEnableOopRasterization) ||
                               features::IsUsingSkiaRenderer();
    if (use_gr_shader_cache)
      CreateChannelCache(gpu::kGrShaderCacheClientId);
  }
}

void GpuHostImpl::DidFailInitialize() {
  UMA_HISTOGRAM_BOOLEAN("GPU.GPUProcessInitialized", false);
  delegate_->DidFailInitialize();
}

void GpuHostImpl::DidCreateContextSuccessfully() {
  delegate_->DidCreateContextSuccessfully();
}

void GpuHostImpl::DidCreateOffscreenContext(const GURL& url) {
  urls_with_live_offscreen_contexts_.insert(url);
}

void GpuHostImpl::DidDestroyOffscreenContext(const GURL& url) {
  // We only want to remove *one* of the entries in the multiset for this
  // particular URL, so can't use the erase method taking a key.
  auto candidate = urls_with_live_offscreen_contexts_.find(url);
  if (candidate != urls_with_live_offscreen_contexts_.end())
    urls_with_live_offscreen_contexts_.erase(candidate);
}

void GpuHostImpl::DidDestroyChannel(int32_t client_id) {
  TRACE_EVENT0("gpu", "GpuHostImpl::DidDestroyChannel");
  client_id_to_shader_cache_.erase(client_id);
}

void GpuHostImpl::DidDestroyAllChannels() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!channel_requests_.empty())
    return;
  constexpr base::TimeDelta kShutDownTimeout = base::TimeDelta::FromSeconds(10);
  shutdown_timeout_.Start(FROM_HERE, kShutDownTimeout,
                          base::BindOnce(&GpuHostImpl::MaybeShutdownGpuProcess,
                                         base::Unretained(this)));
}

void GpuHostImpl::MaybeShutdownGpuProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(channel_requests_.empty());
  delegate_->MaybeShutdownGpuProcess();
}

void GpuHostImpl::DidLoseContext(bool offscreen,
                                 gpu::error::ContextLostReason reason,
                                 const GURL& active_url) {
  // TODO(kbr): would be nice to see the "offscreen" flag too.
  TRACE_EVENT2("gpu", "GpuHostImpl::DidLoseContext", "reason", reason, "url",
               active_url.possibly_invalid_spec());

  if (active_url.is_empty()) {
    return;
  }

  gpu::DomainGuilt guilt = gpu::DomainGuilt::kUnknown;
  switch (reason) {
    case gpu::error::kGuilty:
      guilt = gpu::DomainGuilt::kKnown;
      break;
    // Treat most other error codes as though they had unknown provenance.
    // In practice this doesn't affect the user experience. A lost context
    // of either known or unknown guilt still causes user-level 3D APIs
    // (e.g. WebGL) to be blocked on that domain until the user manually
    // reenables them.
    case gpu::error::kUnknown:
    case gpu::error::kOutOfMemory:
    case gpu::error::kMakeCurrentFailed:
    case gpu::error::kGpuChannelLost:
    case gpu::error::kInvalidGpuMessage:
      break;
    case gpu::error::kInnocent:
      return;
  }

  delegate_->BlockDomainFrom3DAPIs(active_url, guilt);
}

void GpuHostImpl::DisableGpuCompositing() {
  delegate_->DisableGpuCompositing();
}

void GpuHostImpl::DidUpdateGPUInfo(const gpu::GPUInfo& gpu_info) {
  delegate_->DidUpdateGPUInfo(gpu_info);
}

#if defined(OS_WIN)
void GpuHostImpl::DidUpdateOverlayInfo(const gpu::OverlayInfo& overlay_info) {
  delegate_->DidUpdateOverlayInfo(overlay_info);
}

void GpuHostImpl::DidUpdateHDRStatus(bool hdr_enabled) {
  delegate_->DidUpdateHDRStatus(hdr_enabled);
}

void GpuHostImpl::SetChildSurface(gpu::SurfaceHandle parent,
                                  gpu::SurfaceHandle child) {
  if (pid_ != base::kNullProcessId) {
    gfx::RenderingWindowManager::GetInstance()->RegisterChild(
        parent, child, /*expected_child_process_id=*/pid_);
  }
}
#endif  // defined(OS_WIN)

void GpuHostImpl::StoreShaderToDisk(int32_t client_id,
                                    const std::string& key,
                                    const std::string& shader) {
  TRACE_EVENT0("gpu", "GpuHostImpl::StoreShaderToDisk");
  auto iter = client_id_to_shader_cache_.find(client_id);
  // If the cache doesn't exist then this is an off the record profile.
  if (iter == client_id_to_shader_cache_.end())
    return;
  std::string prefix = GetShaderPrefixKey();
  iter->second->Cache(prefix + ":" + key, shader);
}

void GpuHostImpl::RecordLogMessage(int32_t severity,
                                   const std::string& header,
                                   const std::string& message) {
  delegate_->RecordLogMessage(severity, header, message);
}

#if BUILDFLAG(USE_VIZ_DEBUGGER)
void GpuHostImpl::LogFrame(base::Value frame_data) {
  if (!viz_debug_output_callback_.is_null())
    viz_debug_output_callback_.Run(std::move(frame_data));
}
#endif

}  // namespace viz
