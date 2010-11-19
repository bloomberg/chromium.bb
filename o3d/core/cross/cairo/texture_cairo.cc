/*
 * Copyright 2010, Google Inc.
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
                           cairo_t* image_surface_context,
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
      image_surface_(image_surface),
      image_surface_context_(image_surface_context) {
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
  cairo_t* image_surface_context;
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
  image_surface_context = cairo_create(image_surface);
  status = cairo_status(image_surface_context);
  if (CAIRO_STATUS_SUCCESS != status) {
    DLOG(ERROR) << "Error creating Cairo image surface draw context: "
                << status;
    goto fail2;
  }

  return new TextureCairo(service_locator,
                          image_surface,
                          image_surface_context,
                          format,
                          levels,
                          width,
                          height,
                          enable_render_surfaces);

 fail2:
  cairo_destroy(image_surface_context);
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
  cairo_destroy(image_surface_context_);
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
                           const void* src_data,
                           int src_pitch) {
  DLOG(INFO) << "Texture2DCairo SetRect";

  // Create image surface to represent the source.
  cairo_surface_t* source_image_surface = cairo_image_surface_create_for_data(
      const_cast<unsigned char*>(
          static_cast<const unsigned char*>(src_data)),
      cairo_image_surface_get_format(image_surface_),
      src_width,
      src_height,
      src_pitch);

  // Set that surface as the source for paint operations to our texture.
  cairo_set_source_surface(image_surface_context_,
                           source_image_surface,
                           dst_left,
                           dst_top);

  // Paint to the texture. This copies the data.
  cairo_paint(image_surface_context_);

  // Discard our reference to the source surface.
  cairo_surface_destroy(source_image_surface);
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
