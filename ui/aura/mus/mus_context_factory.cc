// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/mus_context_factory.h"

#include "base/command_line.h"
#include "cc/base/switches.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/host/renderer_settings_creation.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "services/ws/public/cpp/gpu/command_buffer_metrics.h"
#include "services/ws/public/cpp/gpu/gpu.h"
#include "services/ws/public/cpp/gpu/shared_worker_context_provider_factory.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_bindings.h"

namespace aura {

MusContextFactory::MusContextFactory(ws::Gpu* gpu)
    : gpu_(gpu), weak_ptr_factory_(this) {}

MusContextFactory::~MusContextFactory() {}

void MusContextFactory::ResetSharedWorkerContextProvider() {
  shared_worker_context_provider_factory_.reset();
}

void MusContextFactory::OnEstablishedGpuChannel(
    base::WeakPtr<ui::Compositor> compositor,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel) {
  if (!compositor)
    return;
  WindowTreeHost* host =
      WindowTreeHost::GetForAcceleratedWidget(compositor->widget());
  WindowPortMus* window_port = WindowPortMus::Get(host->window());
  DCHECK(window_port);

  scoped_refptr<viz::ContextProvider> context_provider =
      gpu_->CreateContextProvider(gpu_channel);
  // If the binding fails, then we need to return early since the compositor
  // expects a successfully initialized/bound provider.
  if (context_provider->BindToCurrentThread() != gpu::ContextResult::kSuccess) {
    // TODO(danakj): We should retry if the result was not kFatalFailure.
    return;
  }

  if (!shared_worker_context_provider_factory_) {
    // TODO(sky): unify these with values from ws::Gpu.
    const int32_t stream_id = 0;
    const gpu::SchedulingPriority stream_priority =
        gpu::SchedulingPriority::kNormal;
    const GURL identity_url("chrome://gpu/MusContextFactory");
    shared_worker_context_provider_factory_ =
        std::make_unique<ws::SharedWorkerContextProviderFactory>(
            stream_id, stream_priority, identity_url,
            ws::command_buffer_metrics::ContextType::MUS_CLIENT);
  }
  scoped_refptr<viz::RasterContextProvider> shared_worker_context_provider;

  auto shared_worker_validate_result =
      shared_worker_context_provider_factory_->Validate(
          gpu_channel, GetGpuMemoryBufferManager());
  if (shared_worker_validate_result == gpu::ContextResult::kSuccess) {
    shared_worker_context_provider =
        shared_worker_context_provider_factory_->provider();
  } else {
    shared_worker_context_provider_factory_.reset();
  }

  std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink =
      window_port->RequestLayerTreeFrameSink(
          std::move(context_provider),
          std::move(shared_worker_context_provider),
          gpu_->gpu_memory_buffer_manager());
  compositor->SetLayerTreeFrameSink(std::move(layer_tree_frame_sink));
}

void MusContextFactory::CreateLayerTreeFrameSink(
    base::WeakPtr<ui::Compositor> compositor) {
  gpu_->EstablishGpuChannel(
      base::BindOnce(&MusContextFactory::OnEstablishedGpuChannel,
                     weak_ptr_factory_.GetWeakPtr(), compositor));
}

scoped_refptr<viz::ContextProvider>
MusContextFactory::SharedMainThreadContextProvider() {
  if (!shared_main_thread_context_provider_) {
    scoped_refptr<gpu::GpuChannelHost> gpu_channel =
        gpu_->EstablishGpuChannelSync();
    shared_main_thread_context_provider_ =
        gpu_->CreateContextProvider(std::move(gpu_channel));
    if (shared_main_thread_context_provider_->BindToCurrentThread() !=
        gpu::ContextResult::kSuccess)
      shared_main_thread_context_provider_ = nullptr;
  }
  return shared_main_thread_context_provider_;
}

void MusContextFactory::RemoveCompositor(ui::Compositor* compositor) {
  // NOTIMPLEMENTED();
}

gpu::GpuMemoryBufferManager* MusContextFactory::GetGpuMemoryBufferManager() {
  return gpu_->gpu_memory_buffer_manager();
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
