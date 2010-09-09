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

TextureCairo::TextureCairo(ServiceLocator* service_locator,
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
          service_locator->GetService<Renderer>())) {
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
  DLOG(INFO) << "Texture2DCairo Create";
  TextureCairo* texture = new TextureCairo(service_locator,
                                           format,
                                           levels,
                                           width,
                                           height,
                                           enable_render_surfaces);
  return texture;
}

// In 2D: is not really used
const Texture::RGBASwizzleIndices& TextureCairo::GetABGR32FSwizzleIndices() {
  NOTIMPLEMENTED();
  return g_gl_abgr32f_swizzle_indices;
}

TextureCairo::~TextureCairo() {
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

  renderer_->SetNewFrame(src_data,
                         src_width, src_height,
                         src_pitch);
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

}  // namespace o3d

