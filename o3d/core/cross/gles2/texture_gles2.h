/*
 * Copyright 2009, Google Inc.
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


// This file contains the declarations for Texture2DGL and TextureCUBEGL.

#ifndef O3D_CORE_CROSS_GLES2_TEXTURE_GLES2_H_
#define O3D_CORE_CROSS_GLES2_TEXTURE_GLES2_H_

// Precompiled header comes before everything else.

// Disable compiler warning for openGL calls that require a void* to
// be cast to a GLuint
#if defined(OS_WIN)
#pragma warning(disable : 4312)
#pragma warning(disable : 4311)
#endif

#include "core/cross/bitmap.h"
#include "core/cross/texture.h"
#include "core/cross/types.h"

namespace o3d {

class RendererGL;

// Texture2DGL -----------------------------------------------------------------

// Texture2DGL implements the Texture2D interface with OpenGL.
class Texture2DGL : public Texture2D {
 public:
  typedef SmartPointer<Texture2DGL> Ref;

  virtual ~Texture2DGL();

  // Overridden from Texture2D
  virtual void SetRect(int level,
                       unsigned left,
                       unsigned top,
                       unsigned width,
                       unsigned height,
                       const void* src_data,
                       int src_pitch);

  // Creates a new Texture2DGL with the given specs. If the GL texture
  // creation fails then it returns NULL otherwise it returns a pointer to the
  // newly created Texture object.
  // The created texture takes ownership of the bitmap data.
  static Texture2DGL* Create(ServiceLocator* service_locator,
                             Texture::Format format,
                             int levels,
                             int width,
                             int height,
                             bool enable_render_surfaces);

  // Returns the implementation-specific texture handle for this texture.
  void* GetTextureHandle() const {
    return reinterpret_cast<void*>(gl_texture_);
  }

  // Gets the GL texture handle.
  GLuint gl_texture() const { return gl_texture_; }

  // Gets a RGBASwizzleIndices that contains a mapping from
  // RGBA to the internal format used by the rendering API.
  virtual const RGBASwizzleIndices& GetABGR32FSwizzleIndices();

 protected:
  // Overridden from Texture2D
  virtual bool PlatformSpecificLock(
      int level, void** texture_data, int* pitch, AccessMode mode);

  // Overridden from Texture2D
  virtual bool PlatformSpecificUnlock(int level);

  // Overridden from Texture2D
  virtual RenderSurface::Ref PlatformSpecificGetRenderSurface(int mip_level);

 private:
  // Initializes the Texture2DGL from a preexisting OpenGL texture handle
  // and raw Bitmap data.
  // The texture takes ownership of the bitmap data.
  Texture2DGL(ServiceLocator* service_locator,
              GLint texture,
              Texture::Format format,
              int levels,
              int width,
              int height,
              bool resize_npot,
              bool enable_render_surfaces);

  // Updates a mip level, sending it from the backing bitmap to GL, rescaling
  // it if resize_to_pot_ is set.
  void UpdateBackedMipLevel(unsigned int level);

  // Returns true if the backing bitmap has the data for the level.
  bool HasLevel(unsigned int level) const {
    DCHECK_LT(static_cast<int>(level), levels());
    return (has_levels_ & (1 << level)) != 0;
  }

  // Whether or not this texture needs to be resized from NPOT to pot behind
  // the scenes.
  bool resize_to_pot_;

  RendererGL* renderer_;

  // The handle of the OpenGL texture object.
  GLuint gl_texture_;

  // A bitmap used to back the NPOT textures on POT-only hardware, and to back
  // the pixel buffer for Lock().
  Bitmap::Ref backing_bitmap_;

  // Bitfield that indicates mip levels that are currently stored in the
  // backing bitmap.
  unsigned int has_levels_;

  // Bitfield that indicates which levels are currently locked.
  unsigned int locked_levels_;
};


// TextureCUBEGL ---------------------------------------------------------------

// TextureCUBEGL implements the TextureCUBE interface with OpenGL.
class TextureCUBEGL : public TextureCUBE {
 public:
  typedef SmartPointer<TextureCUBEGL> Ref;
  virtual ~TextureCUBEGL();

  // Create a new Cube texture from scratch.
  static TextureCUBEGL* Create(ServiceLocator* service_locator,
                               Texture::Format format,
                               int levels,
                               int edge_length,
                               bool enable_render_surfaces);

  // Overridden from TextureCUBE
  virtual void SetRect(CubeFace face,
                       int level,
                       unsigned dst_left,
                       unsigned dst_top,
                       unsigned width,
                       unsigned height,
                       const void* src_data,
                       int src_pitch);

  // Returns the implementation-specific texture handle for this texture.
  virtual void* GetTextureHandle() const {
    return reinterpret_cast<void*>(gl_texture_);
  }

  // Gets the GL texture handle.
  GLuint gl_texture() const { return gl_texture_; }

  // Gets a RGBASwizzleIndices that contains a mapping from
  // RGBA to the internal format used by the rendering API.
  virtual const RGBASwizzleIndices& GetABGR32FSwizzleIndices();

 protected:
  // Overridden from TextureCUBE
  virtual bool PlatformSpecificLock(
      CubeFace face, int level, void** texture_data, int* pitch,
      AccessMode mode);

  // Overridden from TextureCUBE
  virtual bool PlatformSpecificUnlock(CubeFace face, int level);

  // Overridden from TextureCUBE.
  virtual RenderSurface::Ref PlatformSpecificGetRenderSurface(CubeFace face,
                                                              int level);
 private:
  // Creates a texture from a pre-existing GL texture object.
  TextureCUBEGL(ServiceLocator* service_locator,
                GLint texture,
                Texture::Format format,
                int levels,
                int edge_length,
                bool resize_to_pot,
                bool enable_render_surfaces);

  // Updates a mip level, sending it from the backing bitmap to GL, rescaling
  // it if resize_to_pot_ is set.
  void UpdateBackedMipLevel(unsigned int level, CubeFace face);

  // Returns true if the backing bitmap has the data for the level.
  bool HasLevel(CubeFace face, unsigned int level) const {
    DCHECK_LT(static_cast<int>(level), levels());
    return (has_levels_[face] & (1 << level)) != 0;
  }

  // Whether or not this texture needs to be resized from NPOT to pot behind
  // the scenes.
  bool resize_to_pot_;

  RendererGL* renderer_;

  // The handle of the OpenGL texture object.
  GLuint gl_texture_;

  // Bitmaps used to back the NPOT textures on POT-only hardware.
  Bitmap::Ref backing_bitmaps_[NUMBER_OF_FACES];

  // Bitfields that indicates mip levels that are currently stored in the
  // backing bitmap, one per face.
  unsigned int has_levels_[NUMBER_OF_FACES];

  // Bitfields that indicates which levels are currently locked, one per face.
  unsigned int locked_levels_[NUMBER_OF_FACES];
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_GLES2_TEXTURE_GLES2_H_
