/*
 * Copyright 2011, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// Implementations of the abstract Texture2D.
// Texture Class for handling Cairo Rendering Mode.

#include "core/cross/cairo/texture_cairo.h"

#include <string.h>

#include "core/cross/cairo/renderer_cairo.h"

namespace o3d {

namespace {

Texture::RGBASwizzleIndices g_gl_abgr32f_swizzle_indices = {0, 1, 2, 3};

}  // anonymous namespace.

namespace o2d {

static int CairoFormatFromO3DFormat(
    Texture::Format format) {
  switch (format) {
    case Texture::ARGB8:
      return CAIRO_FORMAT_ARGB32;
    case Texture::XRGB8:
      return CAIRO_FORMAT_RGB24;
    default:
      return -1;
  }
  // Cairo also supports two other pure-alpha formats, but we don't expose those
  // capabilities.
}

TextureCairo::TextureCairo(ServiceLocator* service_locator,
                           cairo_surface_t* image_surface,
                           Texture::Format format,
                           int levels,
                           int width,
                           int height,
                           bool enable_render_surfaces)
    : Texture2D(service_locator,
                width,
                height,
                format,
                levels,
                enable_render_surfaces),
      renderer_(static_cast<RendererCairo*>(
          service_locator->GetService<Renderer>())),
      image_surface_(image_surface) {
  DLOG(INFO) << "Texture2D Construct";
  DCHECK_NE(format, Texture::UNKNOWN_FORMAT);
}

// Creates a new texture object from scratch.
TextureCairo* TextureCairo::Create(ServiceLocator* service_locator,
                                   Texture::Format format,
                                   int levels,
                                   int width,
                                   int height,
                                   bool enable_render_surfaces) {
  int cairo_format = CairoFormatFromO3DFormat(format);
  cairo_surface_t* image_surface;
  cairo_status_t status;
  if (-1 == cairo_format) {
    DLOG(ERROR) << "Texture format " << format << " not supported by Cairo";
    goto fail0;
  }
  image_surface = cairo_image_surface_create(
      static_cast<cairo_format_t>(cairo_format),
      width,
      height);
  status = cairo_surface_status(image_surface);
  if (CAIRO_STATUS_SUCCESS != status) {
    DLOG(ERROR) << "Error creating Cairo image surface: " << status;
    goto fail1;
  }

  return new TextureCairo(service_locator,
                          image_surface,
                          format,
                          levels,
                          width,
                          height,
                          enable_render_surfaces);

 fail1:
  cairo_surface_destroy(image_surface);
 fail0:
  return NULL;
}

// In 2D: is not really used
const Texture::RGBASwizzleIndices& TextureCairo::GetABGR32FSwizzleIndices() {
  NOTIMPLEMENTED();
  return g_gl_abgr32f_swizzle_indices;
}

TextureCairo::~TextureCairo() {
  cairo_surface_destroy(image_surface_);
  renderer_ = NULL;
  DLOG(INFO) << "Texture2DCairo Destruct";
}

// Set the image data to the renderer
void TextureCairo::SetRect(int level,
                           unsigned dst_left,
                           unsigned dst_top,
                           unsigned src_width,
                           unsigned src_height,
                           const void* src_data_void,
                           int src_pitch) {
  DLOG(INFO) << "Texture2DCairo SetRect";

  if (0 != level) {
    // Cairo does not support/need mip-maps.
    return;
  }

  cairo_surface_flush(image_surface_);

  const unsigned char* src_data = reinterpret_cast<const unsigned char*>(
      src_data_void);

  unsigned char* dst_data = cairo_image_surface_get_data(image_surface_);

  int dst_pitch = cairo_image_surface_get_stride(image_surface_);

  dst_data += dst_top * dst_pitch + dst_left * 4;
  
  if (ARGB8 == format()) {
    // Cairo supports only premultiplied alpha, but we get the images as
    // non-premultiplied alpha, so we have to convert.
    for (unsigned i = 0; i < src_height; ++i) {
      for (unsigned j = 0; j < src_width; ++j) {
        // NOTE: This assumes a little-endian architecture (e.g., x86). It works
        // for RGBA or BGRA where alpha is in byte 3.
        // Get alpha.
        uint8 alpha = src_data[3];
        // Convert each colour.
        for (int i = 0; i < 3; i++) {
          dst_data[i] = (src_data[i] * alpha + 128U) / 255U;
        }
        // Copy alpha.
        dst_data[3] = alpha;
        src_data += 4;
        dst_data += 4;
      }
      src_data += src_pitch - src_width * 4;
      dst_data += dst_pitch - src_width * 4;
    }
  } else {
    // Just copy the data.
    for (unsigned i = 0; i < src_height; ++i) {
      memcpy(dst_data, src_data, src_width * 4);
      src_data += src_pitch;
      dst_data += dst_pitch;
    }
  }

  cairo_surface_mark_dirty(image_surface_);

  TextureUpdated();
}

// Locks the given mipmap level of this texture for loading from main memory,
// and returns a pointer to the buffer.
bool TextureCairo::PlatformSpecificLock(
    int level, void** data, int* pitch, Texture::AccessMode mode) {
  NOTIMPLEMENTED();
  return true;
}

// In Open GL: Unlocks the given mipmap level of this texture, uploading
// the main memory data buffer to GL.
//
// In 2D: is not really used
bool TextureCairo::PlatformSpecificUnlock(int level) {
  NOTIMPLEMENTED();
  return true;
}

// In 2D: is not really used
RenderSurface::Ref TextureCairo::PlatformSpecificGetRenderSurface(
    int mip_level) {
  DCHECK_LT(mip_level, levels());
  NOTIMPLEMENTED();
  return RenderSurface::Ref(NULL);
}

// Returns the implementation-specific texture handle for this texture.
void* TextureCairo::GetTextureHandle() const {
  NOTIMPLEMENTED();
  return reinterpret_cast<void*>(NULL);
}

}  // namespace o2d

}  // namespace o3d
