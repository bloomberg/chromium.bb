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

#ifndef O3D_CORE_CROSS_CAIRO_TEXTURE_CAIRO_H_
#define O3D_CORE_CROSS_CAIRO_TEXTURE_CAIRO_H_

#include "core/cross/texture.h"

typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;

namespace o3d {

namespace o2d {

class RendererCairo;

// Texture2DCairo implements the Texture2D interface.
class TextureCairo : public Texture2D {
 public:
  typedef SmartPointer<TextureCairo> Ref;

  // Creates a new Texture2DCairo with the given specs. If the texture
  // creation fails then it returns NULL otherwise it returns a pointer to the
  // newly created Texture object.
  static TextureCairo* Create(ServiceLocator* service_locator,
                              Texture::Format format,
                              int levels,
                              int width,
                              int height,
                              bool enable_render_surfaces);

  virtual ~TextureCairo();

  // Overridden from Texture2D
  virtual void SetRect(int level,
                       unsigned left,
                       unsigned top,
                       unsigned width,
                       unsigned height,
                       const void* src_data,
                       int src_pitch);

  // Gets a RGBASwizzleIndices that contains a mapping from
  // RGBA to the internal format used by the rendering API.
  virtual const RGBASwizzleIndices& GetABGR32FSwizzleIndices();

  cairo_surface_t* image_surface() const {
    return image_surface_;
  }

 protected:
  // Overridden from Texture2D
  virtual bool PlatformSpecificLock(int level, void** texture_data, int* pitch,
                                    AccessMode mode);

  // Overridden from Texture2D
  virtual bool PlatformSpecificUnlock(int level);

  // Overridden from Texture2D
  virtual RenderSurface::Ref PlatformSpecificGetRenderSurface(int mip_level);

  // Returns the implementation-specific texture handle for this texture.
  virtual void* GetTextureHandle() const;

 private:
  // Initializes the Texture2D.
  TextureCairo(ServiceLocator* service_locator,
               cairo_surface_t* image_surface,
               Texture::Format format,
               int levels,
               int width,
               int height,
               bool enable_render_surfaces);

  // The Cairo image for this texture.
  cairo_surface_t* image_surface_;
};

}  // namespace o2d

}  // namespace o3d

#endif  // O3D_CORE_CROSS_CAIRO_TEXTURE_CAIRO_H_
