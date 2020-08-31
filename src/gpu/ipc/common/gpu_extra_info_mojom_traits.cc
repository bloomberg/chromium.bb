// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_extra_info_mojom_traits.h"

#include "build/build_config.h"
#include "ui/gfx/mojom/buffer_types_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::ANGLEFeatureDataView, gpu::ANGLEFeature>::Read(
    gpu::mojom::ANGLEFeatureDataView data,
    gpu::ANGLEFeature* out) {
  return data.ReadName(&out->name) && data.ReadCategory(&out->category) &&
         data.ReadDescription(&out->description) && data.ReadBug(&out->bug) &&
         data.ReadStatus(&out->status) && data.ReadCondition(&out->condition);
}

// static
bool StructTraits<gpu::mojom::GpuExtraInfoDataView, gpu::GpuExtraInfo>::Read(
    gpu::mojom::GpuExtraInfoDataView data,
    gpu::GpuExtraInfo* out) {
  if (!data.ReadAngleFeatures(&out->angle_features))
    return false;
#if defined(USE_X11)
  out->system_visual = data.system_visual();
  out->rgba_visual = data.rgba_visual();
  if (!data.ReadGpuMemoryBufferSupportX11(&out->gpu_memory_buffer_support_x11))
    return false;
#endif
  return true;
}

}  // namespace mojo
