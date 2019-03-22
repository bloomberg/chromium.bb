// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_
#define GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_

#include <GLES2/gl2.h>
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace cc {
class DisplayItemList;
class ImageProvider;
struct RasterColorSpace;
}  // namespace cc

namespace gfx {
class ColorSpace;
class Rect;
class Size;
class Vector2dF;
enum class BufferUsage;
}  // namespace gfx

extern "C" typedef struct _ClientBuffer* ClientBuffer;
extern "C" typedef struct _GLColorSpace* GLColorSpace;

namespace gpu {
namespace raster {

enum RasterTexStorageFlags { kNone = 0, kOverlay = (1 << 0) };

class RasterInterface {
 public:
  RasterInterface() {}
  virtual ~RasterInterface() {}

  // OOP-Raster
  virtual void BeginRasterCHROMIUM(
      GLuint sk_color,
      GLuint msaa_sample_count,
      GLboolean can_use_lcd_text,
      GLint pixel_config,
      const cc::RasterColorSpace& raster_color_space,
      const GLbyte* mailbox) = 0;
  virtual void RasterCHROMIUM(const cc::DisplayItemList* list,
                              cc::ImageProvider* provider,
                              const gfx::Size& content_size,
                              const gfx::Rect& full_raster_rect,
                              const gfx::Rect& playback_rect,
                              const gfx::Vector2dF& post_translate,
                              GLfloat post_scale,
                              bool requires_clear) = 0;

  // Schedules a hardware-accelerated image decode and a sync token that's
  // released when the image decode is complete. If the decode could not be
  // scheduled, an empty sync token is returned.
  virtual SyncToken ScheduleImageDecode(
      base::span<const uint8_t> encoded_data,
      const gfx::Size& output_size,
      uint32_t transfer_cache_entry_id,
      const gfx::ColorSpace& target_color_space,
      bool needs_mips) = 0;

  // Raster via GrContext.
  virtual void BeginGpuRaster() = 0;
  virtual void EndGpuRaster() = 0;

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/raster_interface_autogen.h"
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_RASTER_INTERFACE_H_
