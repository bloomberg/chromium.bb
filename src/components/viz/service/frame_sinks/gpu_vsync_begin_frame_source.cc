// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/gpu_vsync_begin_frame_source.h"

#include "base/bind.h"
#include "components/viz/service/display/output_surface.h"

namespace viz {

GpuVSyncBeginFrameSource::GpuVSyncBeginFrameSource(
    uint32_t restart_id,
    OutputSurface* output_surface)
    : ExternalBeginFrameSource(this, restart_id),
      output_surface_(output_surface) {
  DCHECK(output_surface->capabilities().supports_gpu_vsync);
  output_surface->SetGpuVSyncCallback(base::BindRepeating(
      &GpuVSyncBeginFrameSource::OnGpuVSync, base::Unretained(this)));
}

GpuVSyncBeginFrameSource::~GpuVSyncBeginFrameSource() = default;

void GpuVSyncBeginFrameSource::OnGpuVSync(base::TimeTicks vsync_time,
                                          base::TimeDelta vsync_interval) {
  ExternalBeginFrameSource::OnBeginFrame(BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, source_id(), next_begin_frame_sequence_number_++,
      vsync_time, vsync_time + vsync_interval, vsync_interval,
      BeginFrameArgs::NORMAL));
}

void GpuVSyncBeginFrameSource::OnNeedsBeginFrames(bool needs_begin_frames) {
  output_surface_->SetGpuVSyncEnabled(needs_begin_frames);
}

}  // namespace viz
