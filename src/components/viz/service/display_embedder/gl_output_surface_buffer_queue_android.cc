// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/gl_output_surface_buffer_queue_android.h"

#include "components/viz/service/display/renderer_utils.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gl/color_space_utils.h"

namespace viz {

GLOutputSurfaceBufferQueueAndroid::GLOutputSurfaceBufferQueueAndroid(
    scoped_refptr<VizProcessContextProvider> context_provider,
    gpu::SurfaceHandle surface_handle,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gfx::BufferFormat buffer_format)
    : GLOutputSurfaceBufferQueue(context_provider,
                                 surface_handle,
                                 gpu_memory_buffer_manager,
                                 buffer_format) {
  overlay_candidate_validator_ =
      std::make_unique<OverlayCandidateValidatorSurfaceControl>();
}

GLOutputSurfaceBufferQueueAndroid::~GLOutputSurfaceBufferQueueAndroid() =
    default;

std::unique_ptr<OverlayCandidateValidator>
GLOutputSurfaceBufferQueueAndroid::TakeOverlayCandidateValidator() {
  return std::move(overlay_candidate_validator_);
}

void GLOutputSurfaceBufferQueueAndroid::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  context_provider()->ContextSupport()->SetDisplayTransform(transform);
  GLOutputSurfaceBufferQueue::SetDisplayTransformHint(transform);
}

void GLOutputSurfaceBufferQueueAndroid::Reshape(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    bool use_stencil) {
  GLOutputSurfaceBufferQueue::Reshape(size, device_scale_factor, color_space,
                                      has_alpha, use_stencil);
}

}  // namespace viz
