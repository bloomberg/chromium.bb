// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/mus_context_factory.h"

#include "base/bind.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/ws/public/cpp/gpu/command_buffer_metrics.h"
#include "services/ws/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/window_tree_host.h"

namespace aura {
namespace {

constexpr int32_t kContextStreamId = 0;

// TODO(crbug.com/947271): If MusContextFactory is used for non-priviledged
// clients then stream priority should be configurable so non-priviledged
// clients use a lower priority.
constexpr gpu::SchedulingPriority kContextStreamPriority =
    gpu::SchedulingPriority::kHigh;

constexpr ws::command_buffer_metrics::ContextType kContextType =
    ws::command_buffer_metrics::ContextType::MUS_CLIENT;

// URL used to identify contexts providers created here.
const char* kContextUrl = "chrome://gpu/MusContextFactory";

// TODO(kylechar): Refactor this function into another target. Also remove the
// DEPS entry for gpu/command_buffer/client/gles2_interface.h at the same time.
bool IsContextLost(viz::ContextProvider* context_provider) {
  if (!context_provider)
    return false;

  return context_provider->ContextGL()->GetGraphicsResetStatusKHR() !=
         GL_NO_ERROR;
}

// There is no software compositing fallback available on Chrome OS and we don't
// expect fatal errors to happen. If there is a fatal failure it's not expected
// to be recoverable so crash instead of drawing nothing.
void OnFatalContextCreationError() {
  LOG(FATAL) << "Unexpected fatal context failure";
}

}  // namespace

MusContextFactory::MusContextFactory(
    gpu::GpuChannelEstablishFactory* gpu_channel_establish_factory)
    : gpu_channel_establish_factory_(gpu_channel_establish_factory),
      shared_worker_context_provider_factory_(kContextStreamId,
                                              kContextStreamPriority,
                                              GURL(kContextUrl),
                                              kContextType) {}

MusContextFactory::~MusContextFactory() = default;

void MusContextFactory::ResetContextProviders() {
  shared_worker_context_provider_factory_.Reset();
  main_context_provider_.reset();
}

gpu::ContextResult MusContextFactory::ValidateMainContextProvider(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel) {
  if (IsContextLost(main_context_provider_.get()))
    main_context_provider_.reset();

  if (main_context_provider_)
    return gpu::ContextResult::kSuccess;

  if (!gpu_channel)
    return gpu::ContextResult::kFatalFailure;

  // This is for an offscreen context for a mus client, so the default
  // framebuffer doesn't need alpha, depth, stencil or antialiasing.
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;
  attributes.lose_context_when_out_of_memory = true;
  attributes.buffer_preserved = false;
  attributes.enable_gles2_interface = true;
  attributes.enable_raster_interface = false;
  attributes.enable_oop_rasterization = false;

  // TODO(kylechar): Share main thread context creation parameters with
  // VizProcessTransportFactory.
  main_context_provider_ =
      base::MakeRefCounted<ws::ContextProviderCommandBuffer>(
          std::move(gpu_channel), GetGpuMemoryBufferManager(), kContextStreamId,
          kContextStreamPriority, gpu::kNullSurfaceHandle, GURL(kContextUrl),
          /*automatic_flushes=*/false,
          /*support_locking=*/false,
          /*support_grcontext=*/false, gpu::SharedMemoryLimits(), attributes,
          kContextType);

  gpu::ContextResult context_result =
      main_context_provider_->BindToCurrentThread();

  // If this is a transient error then we'll retry.
  if (context_result != gpu::ContextResult::kSuccess)
    main_context_provider_.reset();

  return context_result;
}

void MusContextFactory::OnEstablishedGpuChannel(
    base::WeakPtr<ui::Compositor> compositor,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel) {
  if (!compositor)
    return;
  WindowTreeHost* host =
      WindowTreeHost::GetForAcceleratedWidget(compositor->widget());
  WindowPortMus* window_port = WindowPortMus::Get(host->window());
  // There should always be a WindowPortMus for WindowTreeHost::window(). If
  // there isn't, it likely means we got the wrong WindowTreeHost.
  //
  // TODO(sky): make Compositor extend SupportsUserData so that this code
  // doesn't need to use GetForAcceleratedWidget().
  CHECK(window_port);

  DCHECK_EQ(host->compositor(), compositor.get());

  auto context_result = ValidateMainContextProvider(gpu_channel);
  if (gpu::IsFatalOrSurfaceFailure(context_result)) {
    OnFatalContextCreationError();
  } else if (context_result != gpu::ContextResult::kSuccess) {
    // Try again for transient failures until kSuccess or kFatalFailure.
    gpu_channel_establish_factory_->EstablishGpuChannel(
        base::BindOnce(&MusContextFactory::OnEstablishedGpuChannel,
                       weak_ptr_factory_.GetWeakPtr(), compositor));
    return;
  }

  auto worker_context_result = shared_worker_context_provider_factory_.Validate(
      std::move(gpu_channel), GetGpuMemoryBufferManager());
  if (gpu::IsFatalOrSurfaceFailure(worker_context_result)) {
    OnFatalContextCreationError();
  } else if (worker_context_result != gpu::ContextResult::kSuccess) {
    // Try again for transient failures until kSuccess or kFatalFailure.
    gpu_channel_establish_factory_->EstablishGpuChannel(
        base::BindOnce(&MusContextFactory::OnEstablishedGpuChannel,
                       weak_ptr_factory_.GetWeakPtr(), compositor));
    return;
  }

  window_port->CreateLayerTreeFrameSink(
      main_context_provider_,
      shared_worker_context_provider_factory_.provider(),
      GetGpuMemoryBufferManager());
}

void MusContextFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  gpu_channel_establish_factory_->EstablishGpuChannel(
      base::BindOnce(&MusContextFactory::OnEstablishedGpuChannel,
                     weak_ptr_factory_.GetWeakPtr(), compositor));
}

scoped_refptr<viz::ContextProvider>
MusContextFactory::SharedMainThreadContextProvider() {
  auto context_result = gpu::ContextResult::kTransientFailure;
  while (context_result != gpu::ContextResult::kSuccess) {
    context_result = ValidateMainContextProvider(
        gpu_channel_establish_factory_->EstablishGpuChannelSync());
    if (gpu::IsFatalOrSurfaceFailure(context_result))
      OnFatalContextCreationError();
  }
  return main_context_provider_;
}

void MusContextFactory::RemoveCompositor(ui::Compositor* compositor) {
  // No per compositor state is kept so there is nothing to do here.
}

gpu::GpuMemoryBufferManager* MusContextFactory::GetGpuMemoryBufferManager() {
  return gpu_channel_establish_factory_->GetGpuMemoryBufferManager();
}

cc::TaskGraphRunner* MusContextFactory::GetTaskGraphRunner() {
  return raster_thread_helper_.task_graph_runner();
}

bool MusContextFactory::SyncTokensRequiredForDisplayCompositor() {
  // The display compositor is out-of-process, so must be using a different
  // context from the UI compositor, and requires synchronization between them.
  return true;
}

}  // namespace aura
