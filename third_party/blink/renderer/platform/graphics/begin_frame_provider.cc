// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/begin_frame_provider.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/child/web_scheduler.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

BeginFrameProvider::BeginFrameProvider(
    const BeginFrameProviderParams& begin_frame_provider_params)
    : cfs_binding_(this),
      ocs_binding_(this),
      frame_sink_id_(begin_frame_provider_params.frame_sink_id),
      parent_frame_sink_id_(begin_frame_provider_params.parent_frame_sink_id) {}

void BeginFrameProvider::CreateCompositorFrameSink() {
  DCHECK(frame_sink_id_.is_valid());

  mojom::blink::OffscreenCanvasProviderPtr canvas_provider;
  Platform::Current()->GetInterfaceProvider()->GetInterface(
      mojo::MakeRequest(&canvas_provider));

  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  auto scheduler = blink::Platform::Current()->CurrentThread()->Scheduler();
  if (scheduler)
    task_runner = scheduler->CompositorTaskRunner();

  mojom::blink::OffscreenCanvasSurfaceClientPtr ocs_client;
  ocs_binding_.Bind(mojo::MakeRequest(&ocs_client), task_runner);

  canvas_provider->CreateOffscreenCanvasSurface(
      parent_frame_sink_id_, frame_sink_id_, std::move(ocs_client));

  viz::mojom::blink::CompositorFrameSinkClientPtr client;
  cfs_binding_.Bind(mojo::MakeRequest(&client), task_runner);

  canvas_provider->CreateCompositorFrameSink(
      frame_sink_id_, std::move(client),
      mojo::MakeRequest(&compositor_frame_sink_));

  compositor_frame_sink_->SetNeedsBeginFrame(true);
}

void BeginFrameProvider::OnBeginFrame(const viz::BeginFrameArgs& args) {
  viz::BeginFrameAck ack;
  ack.source_id = args.source_id;
  ack.sequence_number = args.sequence_number;
  ack.has_damage = false;
  compositor_frame_sink_->DidNotProduceFrame(ack);
}

}  // namespace blink
