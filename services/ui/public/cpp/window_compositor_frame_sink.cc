// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/window_compositor_frame_sink.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_sink_client.h"

namespace ui {

// static
std::unique_ptr<WindowCompositorFrameSink> WindowCompositorFrameSink::Create(
    const cc::FrameSinkId& frame_sink_id,
    scoped_refptr<cc::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    std::unique_ptr<WindowCompositorFrameSinkBinding>*
        compositor_frame_sink_binding) {
  cc::mojom::MojoCompositorFrameSinkPtr compositor_frame_sink;
  cc::mojom::MojoCompositorFrameSinkClientPtr compositor_frame_sink_client;
  cc::mojom::MojoCompositorFrameSinkClientRequest
      compositor_frame_sink_client_request =
          MakeRequest(&compositor_frame_sink_client);

  compositor_frame_sink_binding->reset(new WindowCompositorFrameSinkBinding(
      MakeRequest(&compositor_frame_sink),
      compositor_frame_sink_client.PassInterface()));
  return base::WrapUnique(new WindowCompositorFrameSink(
      frame_sink_id, std::move(context_provider), gpu_memory_buffer_manager,
      compositor_frame_sink.PassInterface(),
      std::move(compositor_frame_sink_client_request)));
}

WindowCompositorFrameSink::~WindowCompositorFrameSink() {}

bool WindowCompositorFrameSink::BindToClient(
    cc::CompositorFrameSinkClient* client) {
  if (!cc::CompositorFrameSink::BindToClient(client))
    return false;

  DCHECK(!thread_checker_);
  thread_checker_.reset(new base::ThreadChecker());
  compositor_frame_sink_.Bind(std::move(compositor_frame_sink_info_));
  client_binding_.reset(
      new mojo::Binding<cc::mojom::MojoCompositorFrameSinkClient>(
          this, std::move(client_request_)));

  begin_frame_source_ = base::MakeUnique<cc::ExternalBeginFrameSource>(this);

  client->SetBeginFrameSource(begin_frame_source_.get());
  return true;
}

void WindowCompositorFrameSink::DetachFromClient() {
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();
  client_binding_.reset();
  compositor_frame_sink_.reset();
  cc::CompositorFrameSink::DetachFromClient();
}

void WindowCompositorFrameSink::SubmitCompositorFrame(
    cc::CompositorFrame frame) {
  DCHECK(thread_checker_);
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!compositor_frame_sink_)
    return;

  gfx::Size frame_size = last_submitted_frame_size_;
  if (!frame.render_pass_list.empty())
    frame_size = frame.render_pass_list.back()->output_rect.size();
  if (!local_surface_id_.is_valid() || frame_size != last_submitted_frame_size_)
    local_surface_id_ = id_allocator_.GenerateId();

  compositor_frame_sink_->SubmitCompositorFrame(local_surface_id_,
                                                std::move(frame));

  last_submitted_frame_size_ = frame_size;
}

WindowCompositorFrameSink::WindowCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    scoped_refptr<cc::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    cc::mojom::MojoCompositorFrameSinkPtrInfo compositor_frame_sink_info,
    cc::mojom::MojoCompositorFrameSinkClientRequest client_request)
    : cc::CompositorFrameSink(std::move(context_provider),
                              nullptr,
                              gpu_memory_buffer_manager,
                              nullptr),
      compositor_frame_sink_info_(std::move(compositor_frame_sink_info)),
      client_request_(std::move(client_request)),
      frame_sink_id_(frame_sink_id) {}

void WindowCompositorFrameSink::DidReceiveCompositorFrameAck() {
  DCHECK(thread_checker_);
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!client_)
    return;
  client_->DidReceiveCompositorFrameAck();
}

void WindowCompositorFrameSink::OnBeginFrame(
    const cc::BeginFrameArgs& begin_frame_args) {
  begin_frame_source_->OnBeginFrame(begin_frame_args);
}

void WindowCompositorFrameSink::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  DCHECK(thread_checker_);
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!client_)
    return;
  client_->ReclaimResources(resources);
}

void WindowCompositorFrameSink::WillDrawSurface(
    const cc::LocalSurfaceId& local_surface_id,
    const gfx::Rect& damage_rect) {
  // TODO(fsamuel, staraz): Implement this.
}

void WindowCompositorFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  compositor_frame_sink_->SetNeedsBeginFrame(needs_begin_frames);
}

void WindowCompositorFrameSink::OnDidFinishFrame(const cc::BeginFrameAck& ack) {
  // TODO(eseckler): Pass on the ack to compositor_frame_sink_.
}

WindowCompositorFrameSinkBinding::~WindowCompositorFrameSinkBinding() {}

WindowCompositorFrameSinkBinding::WindowCompositorFrameSinkBinding(
    cc::mojom::MojoCompositorFrameSinkRequest compositor_frame_sink_request,
    cc::mojom::MojoCompositorFrameSinkClientPtrInfo
        compositor_frame_sink_client)
    : compositor_frame_sink_request_(std::move(compositor_frame_sink_request)),
      compositor_frame_sink_client_(std::move(compositor_frame_sink_client)) {}

cc::mojom::MojoCompositorFrameSinkRequest
WindowCompositorFrameSinkBinding::TakeFrameSinkRequest() {
  return std::move(compositor_frame_sink_request_);
}

cc::mojom::MojoCompositorFrameSinkClientPtrInfo
WindowCompositorFrameSinkBinding::TakeFrameSinkClient() {
  return std::move(compositor_frame_sink_client_);
}

}  // namespace ui
