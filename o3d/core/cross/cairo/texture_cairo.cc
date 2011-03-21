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

static const cairo_content_t kCairoContentInvalid =
    static_cast<cairo_content_t>(-1);

static cairo_content_t CairoContentFromO3DFormat(
    Texture::Format format) {
  switch (format) {
    case Texture::ARGB8:
      return CAIRO_CONTENT_COLOR_ALPHA;
    case Texture::XRGB8:
      return CAIRO_CONTENT_COLOR;
    default:
      return kCairoContentInvalid;
  }
  // Cairo also supports a pure-alpha surface, but we don't expose that.
}

TextureCairo::TextureCairo(ServiceLocator* service_locator,
                           cairo_surface_t* surface,
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
      surface_(surface),
      content_dirty_(false) {
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
  cairo_content_t content = CairoContentFromO3DFormat(format);
  if (kCairoContentInvalid == content) {
    DLOG(ERROR) << "Texture format " << format << " not supported by Cairo";
    return NULL;
  }

  RendererCairo* renderer = static_cast<RendererCairo*>(
      service_locator->GetService<Renderer>());
  // NOTE(tschmelcher): Strangely, on Windows if COMPOSITING_TO_IMAGE is turned
  // off in RendererCairo then we actually get much better performance if we use
  // an image surface for textures rather than using CreateSimilarSurface(). But
  // the performance of COMPOSITING_TO_IMAGE narrowly takes first place.
  cairo_surface_t* surface = renderer->CreateSimilarSurface(
      content,
      width,
      height);
  if (!surface) {
    DLOG(ERROR) << "Failed to create texture surface";
    return NULL;
  }

  return new TextureCairo(service_locator,
                          surface,
                          format,
                          levels,
                          width,
                          height,
                          enable_render_surfaces);
}

// In 2D: is not really used
const Texture::RGBASwizzleIndices& TextureCairo::GetABGR32FSwizzleIndices() {
  NOTIMPLEMENTED();
  return g_gl_abgr32f_swizzle_indices;
}

TextureCairo::~TextureCairo() {
  cairo_surface_destroy(surface_);
  DLOG(INFO) << "Texture2DCairo Destruct";
}

static void CopyNonPremultipliedAlphaToPremultipliedAlpha(
    const unsigned char* src_data,
    int src_pitch,
    unsigned char* dst_data,
    int dst_pitch,
    unsigned width,
    unsigned height) {
  // Cairo supports only premultiplied alpha, but we get images as
  // non-premultiplied alpha, so we have to convert.
  for (unsigned i = 0; i < height; ++i) {
    for (unsigned j = 0; j < width; ++j) {
      // NOTE: This assumes a little-endian architecture (e.g., x86). It
      // works for RGBA or BGRA where alpha is in byte 3.
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
    src_data += src_pitch - width * 4;
    dst_data += dst_pitch - width * 4;
  }
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

  unsigned char* src_data =
      reinterpret_cast<unsigned char*>(const_cast<void*>(src_data_void));

  if (cairo_surface_get_type(surface_) == CAIRO_SURFACE_TYPE_IMAGE) {
    // Copy directly to the image surface's data buffer.

    cairo_surface_flush(surface_);

    unsigned char* dst_data = cairo_image_surface_get_data(surface_);

    int dst_pitch = cairo_image_surface_get_stride(surface_);

    dst_data += dst_top * dst_pitch + dst_left * 4;
  
    if (ARGB8 == format()) {
      CopyNonPremultipliedAlphaToPremultipliedAlpha(src_data,
                                                    src_pitch,
                                                    dst_data,
                                                    dst_pitch,
                                                    src_width,
                                                    src_height);
    } else {
      for (unsigned i = 0; i < src_height; ++i) {
        memcpy(dst_data, src_data, src_width * 4);
        src_data += src_pitch;
        dst_data += dst_pitch;
      }
    }

    cairo_surface_mark_dirty(surface_);
  } else {
    // No way to get a pointer to a data buffer for the surface, so we have to
    // update it using cairo operations.

    // Create a source surface for the paint operation.
    cairo_surface_t* source_surface;
    if (ARGB8 == format()) {
      // Have to convert to premultiplied alpha, so we need to make a temporary
      // image surface and copy to it.
      source_surface = cairo_image_surface_create(
          CAIRO_FORMAT_ARGB32,
          src_width,
          src_height);

      cairo_surface_flush(source_surface);

      unsigned char* dst_data = cairo_image_surface_get_data(source_surface);

      int dst_pitch = cairo_image_surface_get_stride(source_surface);

      CopyNonPremultipliedAlphaToPremultipliedAlpha(src_data,
                                                    src_pitch,
                                                    dst_data,
                                                    dst_pitch,
                                                    src_width,
                                                    src_height);

      cairo_surface_mark_dirty(source_surface);
    } else {
      // No conversion needed, so we can paint directly from the input buffer.
      source_surface = cairo_image_surface_create_for_data(
          src_data,
          CAIRO_FORMAT_RGB24,
          src_width,
          src_height,
          src_pitch);
    }

    // Now paint it to this texture's surface.
    cairo_t* context = cairo_create(surface_);
    cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(context, source_surface, dst_left, dst_top);
    cairo_rectangle(context, dst_left, dst_top, src_width, src_height);
    cairo_fill(context);
    cairo_destroy(context);

    cairo_surface_destroy(source_surface);
  }

  set_content_dirty(true);

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
