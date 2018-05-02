// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/begin_frame_provider.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

BeginFrameProvider::BeginFrameProvider(
    const BeginFrameProviderParams& begin_frame_provider_params,
    BeginFrameProviderClient* client)
    : needs_begin_frame_(false),
      cfs_binding_(this),
      efs_binding_(this),
      frame_sink_id_(begin_frame_provider_params.frame_sink_id),
      parent_frame_sink_id_(begin_frame_provider_params.parent_frame_sink_id),
      begin_frame_client_(client) {}

void BeginFrameProvider::CreateCompositorFrameSink() {
  DCHECK(frame_sink_id_.is_valid());

  mojom::blink::EmbeddedFrameSinkProviderPtr provider;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&provider));

  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  auto* scheduler = blink::Platform::Current()->CurrentThread()->Scheduler();
  if (scheduler)
    task_runner = scheduler->CompositorTaskRunner();

  mojom::blink::EmbeddedFrameSinkClientPtr efs_client;
  efs_binding_.Bind(mojo::MakeRequest(&efs_client), task_runner);

  viz::mojom::blink::CompositorFrameSinkClientPtr client;
  cfs_binding_.Bind(mojo::MakeRequest(&client), task_runner);

  provider->CreateSimpleCompositorFrameSink(
      parent_frame_sink_id_, frame_sink_id_, std::move(efs_client),
      std::move(client), mojo::MakeRequest(&compositor_frame_sink_));
}

void BeginFrameProvider::RequestBeginFrame() {
  if (needs_begin_frame_)
    return;

  if (!compositor_frame_sink_.is_bound()) {
    CreateCompositorFrameSink();
    if (!compositor_frame_sink_.is_bound()) {
      return;
    }
  }

  needs_begin_frame_ = true;
  compositor_frame_sink_->SetNeedsBeginFrame(true);
}

void BeginFrameProvider::OnBeginFrame(const viz::BeginFrameArgs& args) {
  // TODO(fserb): we could potentially be nicer here.
  DCHECK(compositor_frame_sink_.is_bound());

  // OnBeginFrame sometimes happens without a request. So we just skip it.
  if (needs_begin_frame_) {
    needs_begin_frame_ = false;
    compositor_frame_sink_->SetNeedsBeginFrame(false);

    begin_frame_client_->BeginFrame();
  }

  viz::BeginFrameAck ack;
  ack.source_id = args.source_id;
  ack.sequence_number = args.sequence_number;
  ack.has_damage = false;
  compositor_frame_sink_->DidNotProduceFrame(ack);
}

}  // namespace blink
