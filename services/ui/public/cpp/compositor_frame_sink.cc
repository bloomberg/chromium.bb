// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/compositor_frame_sink.h"

#include "base/bind.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_sink_client.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/ui/public/cpp/gpu_service.h"
#include "services/ui/public/cpp/window_surface.h"

namespace ui {

CompositorFrameSink::CompositorFrameSink(
    scoped_refptr<cc::ContextProvider> context_provider,
    std::unique_ptr<ui::WindowSurface> surface)
    : cc::CompositorFrameSink(std::move(context_provider), nullptr),
      surface_(std::move(surface)) {}

CompositorFrameSink::~CompositorFrameSink() {}

bool CompositorFrameSink::BindToClient(cc::CompositorFrameSinkClient* client) {
  if (!cc::CompositorFrameSink::BindToClient(client))
    return false;

  surface_->BindToThread();
  surface_->set_client(this);

  // TODO(enne): Get this from the WindowSurface via ServerWindowSurface.
  begin_frame_source_.reset(new cc::DelayBasedBeginFrameSource(
      base::MakeUnique<cc::DelayBasedTimeSource>(
          base::ThreadTaskRunnerHandle::Get().get())));

  client->SetBeginFrameSource(begin_frame_source_.get());
  return true;
}

void CompositorFrameSink::DetachFromClient() {
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();
  surface_.reset();
  cc::CompositorFrameSink::DetachFromClient();
}

void CompositorFrameSink::SubmitCompositorFrame(cc::CompositorFrame frame) {
  surface_->SubmitCompositorFrame(std::move(frame));
}

void CompositorFrameSink::DidReceiveCompositorFrameAck() {
  client_->DidReceiveCompositorFrameAck();
}

void CompositorFrameSink::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  client_->ReclaimResources(resources);
}

}  // namespace ui
