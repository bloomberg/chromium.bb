// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_trace_implementation.h"
#include "gpu/command_buffer/client/gpu_switches.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "services/ui/public/cpp/gpu/command_buffer_metrics.h"
#include "third_party/skia/include/core/SkTraceMemoryDump.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gl/trace_util.h"

class SkDiscardableMemory;

namespace {

// Derives from SkTraceMemoryDump and implements graphics specific memory
// backing functionality.
class SkiaGpuTraceMemoryDump : public SkTraceMemoryDump {
 public:
  // This should never outlive the provided ProcessMemoryDump, as it should
  // always be scoped to a single OnMemoryDump funciton call.
  explicit SkiaGpuTraceMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                                  uint64_t share_group_tracing_guid)
      : pmd_(pmd), share_group_tracing_guid_(share_group_tracing_guid) {}

  // Overridden from SkTraceMemoryDump:
  void dumpNumericValue(const char* dump_name,
                        const char* value_name,
                        const char* units,
                        uint64_t value) override {
    auto* dump = GetOrCreateAllocatorDump(dump_name);
    dump->AddScalar(value_name, units, value);
  }

  void setMemoryBacking(const char* dump_name,
                        const char* backing_type,
                        const char* backing_object_id) override {
    const uint64_t tracing_process_id =
        base::trace_event::MemoryDumpManager::GetInstance()
            ->GetTracingProcessId();

    // For uniformity, skia provides this value as a string. Convert back to a
    // uint32_t.
    uint32_t gl_id =
        std::strtoul(backing_object_id, nullptr /* str_end */, 10 /* base */);

    // Constants used by SkiaGpuTraceMemoryDump to identify different memory
    // types.
    const char* kGLTextureBackingType = "gl_texture";
    const char* kGLBufferBackingType = "gl_buffer";
    const char* kGLRenderbufferBackingType = "gl_renderbuffer";

    // Populated in if statements below.
    base::trace_event::MemoryAllocatorDumpGuid guid;

    if (strcmp(backing_type, kGLTextureBackingType) == 0) {
      guid = gl::GetGLTextureClientGUIDForTracing(share_group_tracing_guid_,
                                                  gl_id);
    } else if (strcmp(backing_type, kGLBufferBackingType) == 0) {
      guid = gl::GetGLBufferGUIDForTracing(tracing_process_id, gl_id);
    } else if (strcmp(backing_type, kGLRenderbufferBackingType) == 0) {
      guid = gl::GetGLRenderbufferGUIDForTracing(tracing_process_id, gl_id);
    }

    if (!guid.empty()) {
      pmd_->CreateSharedGlobalAllocatorDump(guid);

      auto* dump = GetOrCreateAllocatorDump(dump_name);

      const int kImportance = 2;
      pmd_->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  }

  void setDiscardableMemoryBacking(
      const char* dump_name,
      const SkDiscardableMemory& discardable_memory_object) override {
    // We don't use this class for dumping discardable memory.
    NOTREACHED();
  }

  LevelOfDetail getRequestedDetails() const override {
    // TODO(ssid): Use MemoryDumpArgs to create light dumps when requested
    // (crbug.com/499731).
    return kObjectsBreakdowns_LevelOfDetail;
  }

 private:
  // Helper to create allocator dumps.
  base::trace_event::MemoryAllocatorDump* GetOrCreateAllocatorDump(
      const char* dump_name) {
    auto* dump = pmd_->GetAllocatorDump(dump_name);
    if (!dump)
      dump = pmd_->CreateAllocatorDump(dump_name);
    return dump;
  }

  base::trace_event::ProcessMemoryDump* pmd_;
  uint64_t share_group_tracing_guid_;

  DISALLOW_COPY_AND_ASSIGN(SkiaGpuTraceMemoryDump);
};

}  // namespace

namespace ui {

ContextProviderCommandBuffer::SharedProviders::SharedProviders() = default;
ContextProviderCommandBuffer::SharedProviders::~SharedProviders() = default;

ContextProviderCommandBuffer::ContextProviderCommandBuffer(
    scoped_refptr<gpu::GpuChannelHost> channel,
    int32_t stream_id,
    gpu::SchedulingPriority stream_priority,
    gpu::SurfaceHandle surface_handle,
    const GURL& active_url,
    bool automatic_flushes,
    bool support_locking,
    const gpu::SharedMemoryLimits& memory_limits,
    const gpu::gles2::ContextCreationAttribHelper& attributes,
    ContextProviderCommandBuffer* shared_context_provider,
    command_buffer_metrics::ContextType type)
    : stream_id_(stream_id),
      stream_priority_(stream_priority),
      surface_handle_(surface_handle),
      active_url_(active_url),
      automatic_flushes_(automatic_flushes),
      support_locking_(support_locking),
      memory_limits_(memory_limits),
      attributes_(attributes),
      context_type_(type),
      shared_providers_(shared_context_provider
                            ? shared_context_provider->shared_providers_
                            : new SharedProviders),
      channel_(std::move(channel)) {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  DCHECK(channel_);
  context_thread_checker_.DetachFromThread();
}

ContextProviderCommandBuffer::~ContextProviderCommandBuffer() {
  DCHECK(main_thread_checker_.CalledOnValidThread() ||
         context_thread_checker_.CalledOnValidThread());

  {
    base::AutoLock hold(shared_providers_->lock);
    auto it = std::find(shared_providers_->list.begin(),
                        shared_providers_->list.end(), this);
    if (it != shared_providers_->list.end())
      shared_providers_->list.erase(it);
  }

  if (bind_tried_ && bind_result_ == gpu::ContextResult::kSuccess) {
    // Clear the lock to avoid DCHECKs that the lock is being held during
    // shutdown.
    command_buffer_->SetLock(nullptr);
    // Disconnect lost callbacks during destruction.
    gles2_impl_->SetLostContextCallback(base::Closure());
    // Unregister memory dump provider.
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        this);
  }
}

gpu::CommandBufferProxyImpl*
ContextProviderCommandBuffer::GetCommandBufferProxy() {
  return command_buffer_.get();
}

uint32_t ContextProviderCommandBuffer::GetCopyTextureInternalFormat() {
  if (attributes_.alpha_size > 0)
    return GL_RGBA;
  DCHECK_NE(attributes_.red_size, 0);
  DCHECK_NE(attributes_.green_size, 0);
  DCHECK_NE(attributes_.blue_size, 0);
  return GL_RGB;
}

gpu::ContextResult ContextProviderCommandBuffer::BindToCurrentThread() {
  // This is called on the thread the context will be used.
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (bind_tried_)
    return bind_result_;

  bind_tried_ = true;
  // Any early-out should set this to a failure code and return it.
  bind_result_ = gpu::ContextResult::kSuccess;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      default_task_runner_;
  if (!task_runner)
    task_runner = base::ThreadTaskRunnerHandle::Get();

  // It's possible to be running BindToCurrentThread on two contexts
  // on different threads at the same time, but which will be in the same share
  // group. To ensure they end up in the same group, hold the lock on the
  // shared_providers_ (which they will share) after querying the group, until
  // this context has been added to the list.
  {
    ContextProviderCommandBuffer* shared_context_provider = nullptr;
    gpu::CommandBufferProxyImpl* shared_command_buffer = nullptr;
    scoped_refptr<gpu::gles2::ShareGroup> share_group;

    base::AutoLock hold(shared_providers_->lock);

    if (!shared_providers_->list.empty()) {
      shared_context_provider = shared_providers_->list.front();
      shared_command_buffer = shared_context_provider->command_buffer_.get();
      share_group = shared_context_provider->gles2_impl_->share_group();
      DCHECK_EQ(!!shared_command_buffer, !!share_group);
    }

    // This command buffer is a client-side proxy to the command buffer in the
    // GPU process.
    command_buffer_ = std::make_unique<gpu::CommandBufferProxyImpl>(
        std::move(channel_), stream_id_, task_runner);
    bind_result_ =
        command_buffer_->Initialize(surface_handle_, shared_command_buffer,
                                    stream_priority_, attributes_, active_url_);
    if (bind_result_ != gpu::ContextResult::kSuccess) {
      DLOG(ERROR) << "GpuChannelHost failed to create command buffer.";
      command_buffer_metrics::UmaRecordContextInitFailed(context_type_);
      return bind_result_;
    }

    // The GLES2 helper writes the command buffer protocol.
    gles2_helper_ =
        std::make_unique<gpu::gles2::GLES2CmdHelper>(command_buffer_.get());
    gles2_helper_->SetAutomaticFlushes(automatic_flushes_);
    bind_result_ =
        gles2_helper_->Initialize(memory_limits_.command_buffer_size);
    if (bind_result_ != gpu::ContextResult::kSuccess) {
      DLOG(ERROR) << "Failed to initialize GLES2CmdHelper.";
      return bind_result_;
    }

    // The transfer buffer is used to copy resources between the client
    // process and the GPU process.
    transfer_buffer_ =
        std::make_unique<gpu::TransferBuffer>(gles2_helper_.get());

    // The GLES2Implementation exposes the OpenGLES2 API, as well as the
    // gpu::ContextSupport interface.
    constexpr bool support_client_side_arrays = false;
    gles2_impl_ = std::make_unique<gpu::gles2::GLES2Implementation>(
        gles2_helper_.get(), share_group, transfer_buffer_.get(),
        attributes_.bind_generates_resource,
        attributes_.lose_context_when_out_of_memory, support_client_side_arrays,
        command_buffer_.get());
    bind_result_ = gles2_impl_->Initialize(memory_limits_);
    if (bind_result_ != gpu::ContextResult::kSuccess) {
      DLOG(ERROR) << "Failed to initialize GLES2Implementation.";
      return bind_result_;
    }

    if (command_buffer_->GetLastState().error != gpu::error::kNoError) {
      // The context was DOA, which can be caused by other contexts and we
      // could try again.
      LOG(ERROR) << "ContextResult::kTransientFailure: "
                    "Context dead on arrival. Last error: "
                 << command_buffer_->GetLastState().error;
      bind_result_ = gpu::ContextResult::kTransientFailure;
      return bind_result_;
    }

    // If any context in the share group has been lost, then abort and don't
    // continue since we need to go back to the caller of the constructor to
    // find the correct share group.
    // This may happen in between the share group being chosen at the
    // constructor, and getting to run this BindToCurrentThread method which
    // can be on some other thread.
    // We intentionally call this *after* creating the command buffer via the
    // GpuChannelHost. Once that has happened, the service knows we are in the
    // share group and if a shared context is lost, our context will be informed
    // also, and the lost context callback will occur for the owner of the
    // context provider. If we check sooner, the shared context may be lost in
    // between these two states and our context here would be left in an orphan
    // share group.
    if (share_group && share_group->IsLost()) {
      // The context was DOA, which can be caused by other contexts and we
      // could try again.
      LOG(ERROR) << "ContextResult::kTransientFailure: share group was lost";
      bind_result_ = gpu::ContextResult::kTransientFailure;
      return bind_result_;
    }

    shared_providers_->list.push_back(this);

    cache_controller_ = std::make_unique<viz::ContextCacheController>(
        gles2_impl_.get(), task_runner);
  }

  gles2_impl_->SetLostContextCallback(
      base::Bind(&ContextProviderCommandBuffer::OnLostContext,
                 // |this| owns the GLES2Implementation which holds the
                 // callback.
                 base::Unretained(this)));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableGpuClientTracing)) {
    // This wraps the real GLES2Implementation and we should always use this
    // instead when it's present.
    trace_impl_ = std::make_unique<gpu::gles2::GLES2TraceImplementation>(
        gles2_impl_.get());
  }

  // Do this last once the context is set up.
  std::string type_name =
      command_buffer_metrics::ContextTypeToString(context_type_);
  std::string unique_context_name =
      base::StringPrintf("%s-%p", type_name.c_str(), gles2_impl_.get());
  ContextGL()->TraceBeginCHROMIUM("gpu_toplevel", unique_context_name.c_str());
  // If support_locking_ is true, the context may be used from multiple
  // threads, and any async callstacks will need to hold the same lock, so
  // give it to the command buffer and cache controller.
  // We don't hold a lock here since there's no need, so set the lock very last
  // to prevent asserts that we're not holding it.
  if (support_locking_) {
    command_buffer_->SetLock(&context_lock_);
    cache_controller_->SetLock(&context_lock_);
  }
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "ContextProviderCommandBuffer", std::move(task_runner));
  return bind_result_;
}

void ContextProviderCommandBuffer::DetachFromThread() {
  context_thread_checker_.DetachFromThread();
}

gpu::gles2::GLES2Interface* ContextProviderCommandBuffer::ContextGL() {
  DCHECK(bind_tried_);
  DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (trace_impl_)
    return trace_impl_.get();
  return gles2_impl_.get();
}

gpu::ContextSupport* ContextProviderCommandBuffer::ContextSupport() {
  return gles2_impl_.get();
}

class GrContext* ContextProviderCommandBuffer::GrContext() {
  DCHECK(bind_tried_);
  DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(new skia_bindings::GrContextForGLES2Interface(
      ContextGL(), ContextCapabilities()));
  cache_controller_->SetGrContext(gr_context_->get());

  // If GlContext is already lost, also abandon the new GrContext.
  if (gr_context_->get() &&
      ContextGL()->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    gr_context_->get()->abandonContext();

  return gr_context_->get();
}

viz::ContextCacheController* ContextProviderCommandBuffer::CacheController() {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  return cache_controller_.get();
}

void ContextProviderCommandBuffer::InvalidateGrContext(uint32_t state) {
  if (gr_context_) {
    DCHECK(bind_tried_);
    DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);
    DCHECK(context_thread_checker_.CalledOnValidThread());
    gr_context_->ResetContext(state);
  }
}

void ContextProviderCommandBuffer::SetDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> default_task_runner) {
  DCHECK(!bind_tried_);
  default_task_runner_ = std::move(default_task_runner);
}

base::Lock* ContextProviderCommandBuffer::GetLock() {
  DCHECK(support_locking_);
  return &context_lock_;
}

const gpu::Capabilities& ContextProviderCommandBuffer::ContextCapabilities()
    const {
  DCHECK(bind_tried_);
  DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);
  DCHECK(context_thread_checker_.CalledOnValidThread());
  // Skips past the trace_impl_ as it doesn't have capabilities.
  return gles2_impl_->capabilities();
}

const gpu::GpuFeatureInfo& ContextProviderCommandBuffer::GetGpuFeatureInfo()
    const {
  DCHECK(bind_tried_);
  DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);
  DCHECK(context_thread_checker_.CalledOnValidThread());
  if (!command_buffer_ || !command_buffer_->channel()) {
    static const gpu::GpuFeatureInfo default_gpu_feature_info;
    return default_gpu_feature_info;
  }
  return command_buffer_->channel()->gpu_feature_info();
}

void ContextProviderCommandBuffer::OnLostContext() {
  DCHECK(context_thread_checker_.CalledOnValidThread());

  if (!lost_context_callback_.is_null())
    lost_context_callback_.Run();
  if (gr_context_)
    gr_context_->OnLostContext();

  gpu::CommandBuffer::State state = GetCommandBufferProxy()->GetLastState();
  command_buffer_metrics::UmaRecordContextLost(context_type_, state.error,
                                               state.context_lost_reason);
}

void ContextProviderCommandBuffer::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  DCHECK(context_thread_checker_.CalledOnValidThread());
  DCHECK(lost_context_callback_.is_null() || lost_context_callback.is_null());
  lost_context_callback_ = lost_context_callback;
}

bool ContextProviderCommandBuffer::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(bind_tried_);
  DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);

  base::Optional<base::AutoLock> hold;
  if (support_locking_)
    hold.emplace(context_lock_);

  gles2_impl_->OnMemoryDump(args, pmd);
  gles2_helper_->OnMemoryDump(args, pmd);

  if (gr_context_) {
    context_thread_checker_.DetachFromThread();
    SkiaGpuTraceMemoryDump trace_memory_dump(
        pmd, gles2_impl_->ShareGroupTracingGUID());
    gr_context_->get()->dumpMemoryStatistics(&trace_memory_dump);
    context_thread_checker_.DetachFromThread();
  }
  return true;
}

}  // namespace ui
