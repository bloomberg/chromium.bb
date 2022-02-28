// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_USAGE_H_
#define GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_USAGE_H_

#include <stdint.h>
#include <string>

#include "gpu/gpu_export.h"

namespace gpu {

// Please update the function, CreateLabelForSharedImageUsage, when adding a new
// enum value.
enum SharedImageUsage : uint32_t {
  // Image will be used in GLES2Interface
  SHARED_IMAGE_USAGE_GLES2 = 1 << 0,
  // Image will be used as a framebuffer (hint)
  SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT = 1 << 1,
  // Image will be used in RasterInterface
  SHARED_IMAGE_USAGE_RASTER = 1 << 2,
  // Image will be used in Display
  SHARED_IMAGE_USAGE_DISPLAY = 1 << 3,
  // Image will be used as a scanout buffer (overlay)
  SHARED_IMAGE_USAGE_SCANOUT = 1 << 4,
  // Image will be used in OOP rasterization. This flag is used on top of
  // SHARED_IMAGE_USAGE_RASTER to indicate that the client will only use
  // RasterInterface for OOP rasterization. TODO(backer): Eliminate once we can
  // CPU raster to SkImage via RasterInterface.
  SHARED_IMAGE_USAGE_OOP_RASTERIZATION = 1 << 5,
  // Image will be used for RGB emulation in WebGL on Mac.
  SHARED_IMAGE_USAGE_RGB_EMULATION = 1 << 6,
  // Image will be used by Dawn (for WebGPU)
  SHARED_IMAGE_USAGE_WEBGPU = 1 << 7,
  // Image will be used in a protected Vulkan context on Fuchsia.
  SHARED_IMAGE_USAGE_PROTECTED = 1 << 8,
  // Image may use concurrent read/write access. Used by single buffered canvas.
  // TODO(crbug.com/969114): This usage is currently not supported in GL/Vulkan
  // interop cases.
  SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE = 1 << 9,
  // Image will be used for video decode acceleration on Chrome OS.
  SHARED_IMAGE_USAGE_VIDEO_DECODE = 1 << 10,
  // Image will be used as a WebGPU swapbuffer
  SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE = 1 << 11,
  // Image will be used by VideoToolbox on macOS. If this is set, then
  // GLImage::DisableInUseByWindowServer should be called on any GLImages that
  // use this SharedImage.
  SHARED_IMAGE_USAGE_MACOS_VIDEO_TOOLBOX = 1 << 12,
  // Image will be used with mipmap enabled
  SHARED_IMAGE_USAGE_MIPMAP = 1 << 13,
  // Image will be used for CPU Writes by client
  SHARED_IMAGE_USAGE_CPU_WRITE = 1 << 14,
  // Image will be used in RasterInterface with RawDraw.
  SHARED_IMAGE_USAGE_RAW_DRAW = 1 << 15,
};

// Create a string to label SharedImageUsage.
GPU_EXPORT std::string CreateLabelForSharedImageUsage(uint32_t usage);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SHARED_IMAGE_USAGE_H_
