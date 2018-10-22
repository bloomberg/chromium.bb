// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_display_provider.h"

#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/test/fake_output_surface.h"

namespace viz {

TestDisplayProvider::TestDisplayProvider() = default;

TestDisplayProvider::~TestDisplayProvider() = default;

std::unique_ptr<Display> TestDisplayProvider::CreateDisplay(
    const FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    bool gpu_compositing,
    mojom::DisplayClient* display_client,
    ExternalBeginFrameSource* external_begin_frame_source,
    SyntheticBeginFrameSource* synthetic_begin_frame_source,
    const RendererSettings& renderer_settings,
    bool send_swap_size_notifications) {
  auto task_runner = base::ThreadTaskRunnerHandle::Get();
  DCHECK(task_runner);

  BeginFrameSource* begin_frame_source =
      synthetic_begin_frame_source
          ? static_cast<BeginFrameSource*>(synthetic_begin_frame_source)
          : static_cast<BeginFrameSource*>(external_begin_frame_source);

  std::unique_ptr<OutputSurface> output_surface;
  if (gpu_compositing) {
    output_surface = FakeOutputSurface::Create3d();
  } else {
    output_surface = FakeOutputSurface::CreateSoftware(
        std::make_unique<SoftwareOutputDevice>());
  }

  auto scheduler = std::make_unique<DisplayScheduler>(
      begin_frame_source, task_runner.get(),
      output_surface->capabilities().max_frames_pending);

  return std::make_unique<Display>(&shared_bitmap_manager_, renderer_settings,
                                   frame_sink_id, std::move(output_surface),
                                   std::move(scheduler), task_runner);
}

uint32_t TestDisplayProvider::GetRestartId() const {
  return BeginFrameSource::kNotRestartableId;
}

}  // namespace viz
