// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/compositor_frame_sink.h"

#include "base/bind.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/ui/public/cpp/context_provider.h"
#include "services/ui/public/cpp/gpu_service.h"
#include "services/ui/public/cpp/window_surface.h"

namespace ui {

CompositorFrameSink::CompositorFrameSink(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    std::unique_ptr<ui::WindowSurface> surface)
    : cc::CompositorFrameSink(
          make_scoped_refptr(new ContextProvider(std::move(gpu_channel_host))),
          nullptr),
      surface_(std::move(surface)) {
}

CompositorFrameSink::~CompositorFrameSink() {}

bool CompositorFrameSink::BindToClient(cc::CompositorFrameSinkClient* client) {
  surface_->BindToThread();
  surface_->set_client(this);

  // TODO(enne): Get this from the WindowSurface via ServerWindowSurface.
  begin_frame_source_.reset(new cc::DelayBasedBeginFrameSource(
      base::MakeUnique<cc::DelayBasedTimeSource>(
          base::ThreadTaskRunnerHandle::Get().get())));

  client->SetBeginFrameSource(begin_frame_source_.get());
  return cc::CompositorFrameSink::BindToClient(client);
}

void CompositorFrameSink::DetachFromClient() {
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();
  surface_.reset();
  cc::CompositorFrameSink::DetachFromClient();
}

void CompositorFrameSink::SwapBuffers(cc::CompositorFrame frame) {
  // CompositorFrameSink owns WindowSurface, and so if CompositorFrameSink is
  // destroyed then SubmitCompositorFrame's callback will never get called.
  // Thus, base::Unretained is safe here.
  surface_->SubmitCompositorFrame(
      std::move(frame), base::Bind(&CompositorFrameSink::SwapBuffersComplete,
                                   base::Unretained(this)));
}

void CompositorFrameSink::OnResourcesReturned(
    ui::WindowSurface* surface,
    mojo::Array<cc::ReturnedResource> resources) {
  client_->ReclaimResources(resources.To<cc::ReturnedResourceArray>());
}

void CompositorFrameSink::SwapBuffersComplete() {
  client_->DidSwapBuffersComplete();
}

}  // namespace ui
