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


// Implementations of the abstract Texture2D and TextureCUBE classes using
// the OpenGL graphics API.

#include "core/cross/gl/gl_headers.h"
#include "core/cross/error.h"
#include "core/cross/types.h"
#include "core/cross/pointer_utils.h"
#include "core/cross/gl/renderer_gl.h"
#include "core/cross/gl/render_surface_gl.h"
#include "core/cross/gl/texture_gl.h"
#include "core/cross/gl/utils_gl.h"
#include "core/cross/gl/utils_gl-inl.h"

namespace o3d {

namespace {

Texture::RGBASwizzleIndices g_gl_abgr32f_swizzle_indices = {0, 1, 2, 3};

}  // anonymous namespace.

// Converts an O3D texture format to a GL texture format.
// Input is 'format'.
// GL has 2 notions of the format:
// - the internal format which describes how the format should be stored on the
// GPU
// - the (format, type) pair which describes how the input data to glTexImage2D
// is laid out. If format is 0, the data is compressed and needs to be passed
// to glCompressedTexImage2D.
// The internal format is returned in internal_format.
// The format is the return value of the function.
// The type is returned in data_type.
static GLenum GLFormatFromO3DFormat(Texture::Format format,
                                    GLenum *internal_format,
                                    GLenum *data_type) {
  switch (format) {
    case Texture::XRGB8: {
      *internal_format = GL_RGB;
      *data_type = GL_UNSIGNED_BYTE;
      return GL_BGRA;
    }
    case Texture::ARGB8: {
      *internal_format = GL_RGBA;
      *data_type = GL_UNSIGNED_BYTE;
      return GL_BGRA;
    }
    case Texture::ABGR16F: {
      *internal_format = GL_RGBA16F_ARB;
      *data_type = GL_HALF_FLOAT_ARB;
      return GL_RGBA;
    }
    case Texture::R32F: {
      *internal_format = GL_LUMINANCE32F_ARB;
      *data_type = GL_FLOAT;
      return GL_LUMINANCE;
    }
    case Texture::ABGR32F: {
      *internal_format = GL_RGBA32F_ARB;
      *data_type = GL_FLOAT;
      return GL_BGRA;
    }
    case Texture::DXT1: {
      if (GL_EXT_texture_compression_s3tc) {
        *internal_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        *data_type = 0;
        return 0;
      } else {
        // TODO: we need to convert DXT1 -> RGBA8 but keep around the
        // pixels so that we can read them back (we won't try to convert back
        // to DXTC).
        LOG(ERROR) << "DXT1 compressed textures not supported yet.";
        *internal_format = 0;
        *data_type = GL_BYTE;
        return 0;
      }
    }
    case Texture::DXT3: {
      if (GL_EXT_texture_compression_s3tc) {
        *internal_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        *data_type = 0;
        return 0;
      } else {
        // TODO: we need to convert DXT3 -> RGBA8 but keep around the
        // pixels so that we can read them back (we won't try to convert back
        // to DXTC).
        LOG(ERROR) << "DXT3 compressed textures not supported yet.";
        *internal_format = 0;
        *data_type = GL_BYTE;
        return 0;
      }
    }
    case Texture::DXT5: {
      if (GL_EXT_texture_compression_s3tc) {
        *internal_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        *data_type = 0;
        return 0;
      } else {
        // TODO: we need to convert DXT3 -> RGBA8 but keep around the
        // pixels so that we can read them back (we won't try to convert back
        // to DXTC).
        LOG(ERROR) << "DXT5 compressed textures not supported yet.";
        *internal_format = 0;
        *data_type = GL_BYTE;
        return 0;
      }
    }
    case Texture::UNKNOWN_FORMAT:
      break;
  }
  // failed to find a matching format
  LOG(ERROR) << "Unrecognized Texture format type.";
  *internal_format = 0;
  *data_type = 0;
  return 0;
}

// Updates a GL image from a bitmap, rescaling if necessary.
static bool UpdateGLImageFromBitmap(GLenum target,
                                    unsigned int level,
                                    TextureCUBE::CubeFace face,
                                    const Bitmap &bitmap,
                                    bool resize_to_pot) {
  DCHECK(bitmap.image_data());
  unsigned int mip_width = std::max(1U, bitmap.width() >> level);
  unsigned int mip_height = std::max(1U, bitmap.height() >> level);
  const uint8 *mip_data = bitmap.GetMipData(level);
  size_t mip_size =
      image::ComputeBufferSize(mip_width, mip_height, bitmap.format());
  scoped_array<uint8> temp_data;
  if (resize_to_pot) {
    DCHECK(!Texture::IsCompressedFormat(bitmap.format()));
    unsigned int pot_width =
        std::max(1U, image::ComputePOTSize(bitmap.width()) >> level);
    unsigned int pot_height =
        std::max(1U, image::ComputePOTSize(bitmap.height()) >> level);
    size_t pot_size = image::ComputeBufferSize(pot_width, pot_height,
                                               bitmap.format());
    temp_data.reset(new uint8[pot_size]);
    image::Scale(mip_width, mip_height, bitmap.format(), mip_data,
                 pot_width, pot_height, temp_data.get(),
                 image::ComputePitch(bitmap.format(), pot_width));
    mip_width = pot_width;
    mip_height = pot_height;
    mip_size = pot_size;
    mip_data = temp_data.get();
  }
  GLenum gl_internal_format = 0;
  GLenum gl_data_type = 0;
  GLenum gl_format = GLFormatFromO3DFormat(bitmap.format(), &gl_internal_format,
                                           &gl_data_type);
  if (gl_format) {
    glTexSubImage2D(target, level, 0, 0, mip_width, mip_height,
                    gl_format, gl_data_type, mip_data);
  } else {
    glCompressedTexSubImage2D(target, level, 0, 0, mip_width, mip_height,
                              gl_internal_format, mip_size, mip_data);
  }
  return glGetError() == GL_NO_ERROR;
}

// Creates the array of GL images for a particular face and upload the pixel
// data from the bitmap.
static bool CreateGLImages(GLenum target,
                           GLenum internal_format,
                           GLenum gl_format,
                           GLenum type,
                           TextureCUBE::CubeFace face,
                           Texture::Format format,
                           int levels,
                           int width,
                           int height,
                           bool resize_to_pot) {
  unsigned int mip_width = width;
  unsigned int mip_height = height;
  if (resize_to_pot) {
    mip_width = image::ComputePOTSize(mip_width);
    mip_height = image::ComputePOTSize(mip_height);
  }
  // glCompressedTexImage2D does't accept NULL as a parameter, so we need
  // to pass in some data. If we can pass in the original pixel data, we'll
  // do that, otherwise we'll pass an empty buffer. In that case, prepare it
  // here once for all.
  scoped_array<uint8> temp_data;
  size_t size = image::ComputeBufferSize(mip_width, mip_height, format);
  temp_data.reset(new uint8[size]);
  memset(temp_data.get(), 0, size);

  for (int i = 0; i < levels; ++i) {
    if (gl_format) {
      glTexImage2D(target, i, internal_format, mip_width, mip_height,
                   0, gl_format, type, temp_data.get());
      if (glGetError() != GL_NO_ERROR) {
        DLOG(ERROR) << "glTexImage2D failed";
        return false;
      }
    } else {
      size_t mip_size = image::ComputeBufferSize(mip_width, mip_height, format);
      glCompressedTexImage2DARB(target, i, internal_format, mip_width,
                                mip_height, 0, mip_size, temp_data.get());
      if (glGetError() != GL_NO_ERROR) {
        DLOG(ERROR) << "glCompressedTexImage2D failed";
        return false;
      }
    }
    mip_width = std::max(1U, mip_width >> 1);
    mip_height = std::max(1U, mip_height >> 1);
  }
  return true;
}

// Texture2DGL -----------------------------------------------------------------

// Constructs a 2D texture object from an existing OpenGL 2D texture.
// NOTE: the Texture2DGL now owns the GL texture and will destroy it on exit.
Texture2DGL::Texture2DGL(ServiceLocator* service_locator,
                         GLint texture,
                         Texture::Format format,
                         int levels,
                         int width,
                         int height,
                         bool resize_to_pot,
                         bool enable_render_surfaces)
    : Texture2D(service_locator,
                width,
                height,
                format,
                levels,
                enable_render_surfaces),
      resize_to_pot_(resize_to_pot),
      renderer_(static_cast<RendererGL*>(
          service_locator->GetService<Renderer>())),
      gl_texture_(texture),
      backing_bitmap_(Bitmap::Ref(new Bitmap(service_locator))),
      has_levels_(0),
      locked_levels_(0) {
  DLOG(INFO) << "Texture2DGL Construct from GLint";
  DCHECK_NE(format, Texture::UNKNOWN_FORMAT);
}

// Creates a new texture object from scratch.
Texture2DGL* Texture2DGL::Create(ServiceLocator* service_locator,
                                 Texture::Format format,
                                 int levels,
                                 int width,
                                 int height,
                                 bool enable_render_surfaces) {
  DLOG(INFO) << "Texture2DGL Create";
  DCHECK_NE(format, Texture::UNKNOWN_FORMAT);
  RendererGL *renderer = static_cast<RendererGL *>(
      service_locator->GetService<Renderer>());
  renderer->MakeCurrentLazy();
  GLenum gl_internal_format = 0;
  GLenum gl_data_type = 0;
  GLenum gl_format = GLFormatFromO3DFormat(format,
                                           &gl_internal_format,
                                           &gl_data_type);
  if (gl_internal_format == 0) {
    DLOG(ERROR) << "Unsupported format in Texture2DGL::Create.";
    return NULL;
  }

  bool resize_to_pot = !renderer->supports_npot() &&
                       !image::IsPOT(width, height);

  // Creates the OpenGL texture object, with all the required mip levels.
  GLuint gl_texture = 0;
  glGenTextures(1, &gl_texture);
  glBindTexture(GL_TEXTURE_2D, gl_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
                  levels - 1);

  if (!CreateGLImages(GL_TEXTURE_2D, gl_internal_format, gl_format,
                      gl_data_type, TextureCUBE::FACE_POSITIVE_X,
                      format, levels, width, height, resize_to_pot)) {
    DLOG(ERROR) << "Failed to create texture images.";
    glDeleteTextures(1, &gl_texture);
    return NULL;
  }
  glTexParameteri(GL_TEXTURE_2D,
                  GL_TEXTURE_MIN_FILTER,
                  GL_NEAREST_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  GLint gl_width;
  GLint gl_height;
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &gl_width);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &gl_height);

  DLOG(INFO) << "Created 2D texture (size=" << gl_width << "x" << gl_height
             << ", GLuint=" << gl_texture << ")";
  Texture2DGL *texture = new Texture2DGL(service_locator,
                                         gl_texture,
                                         format,
                                         levels,
                                         width,
                                         height,
                                         resize_to_pot,
                                         enable_render_surfaces);

  // If the hardware does not support npot textures, allocate a 0-initialized
  // mip-chain here for use during Texture2DGL::Lock.
  if (resize_to_pot) {
    texture->backing_bitmap_->Allocate(format, width, height, levels,
                                       Bitmap::IMAGE);
    texture->has_levels_ = (1 << levels) - 1;
  }
  CHECK_GL_ERROR();
  return texture;
}

void Texture2DGL::UpdateBackedMipLevel(unsigned int level) {
  DCHECK_LT(static_cast<int>(level), levels());
  DCHECK(backing_bitmap_->image_data());
  DCHECK_EQ(backing_bitmap_->width(), static_cast<unsigned int>(width()));
  DCHECK_EQ(backing_bitmap_->height(), static_cast<unsigned int>(height()));
  DCHECK_EQ(backing_bitmap_->format(), format());
  renderer_->MakeCurrentLazy();
  glBindTexture(GL_TEXTURE_2D, gl_texture_);
  UpdateGLImageFromBitmap(GL_TEXTURE_2D, level, TextureCUBE::FACE_POSITIVE_X,
                          *backing_bitmap_.Get(), resize_to_pot_);
}

Texture2DGL::~Texture2DGL() {
  DLOG(INFO) << "Texture2DGL Destruct";
  if (gl_texture_) {
    renderer_->MakeCurrentLazy();
    glDeleteTextures(1, &gl_texture_);
    gl_texture_ = 0;
  }
  CHECK_GL_ERROR();
}

void Texture2DGL::SetRect(int level,
                          unsigned dst_left,
                          unsigned dst_top,
                          unsigned src_width,
                          unsigned src_height,
                          const void* src_data,
                          int src_pitch) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to SetRect on non-existent level " << level
        << " on Texture \"" << name() << "\"";
    return;
  }
  if (render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to SetRect a render-target texture: " << name();
    return;
  }

  unsigned mip_width = image::ComputeMipDimension(level, width());
  unsigned mip_height = image::ComputeMipDimension(level, height());

  if (dst_left + src_width > mip_width ||
      dst_top + src_height > mip_height) {
    O3D_ERROR(service_locator())
        << "SetRect(" << level << ", " << dst_left << ", " << dst_top << ", "
        << src_width << ", " << src_height << ") out of range for texture << \""
        << name() << "\"";
    return;
  }

  bool entire_rect = dst_left == 0 && dst_top == 0 &&
                     src_width == mip_width && src_height == mip_height;
  bool compressed = IsCompressed();

  if (compressed && !entire_rect) {
    O3D_ERROR(service_locator())
        << "SetRect must be full rectangle for compressed textures";
    return;
  }

  if (resize_to_pot_) {
    DCHECK(backing_bitmap_->image_data());
    DCHECK(!compressed);
    // We need to update the backing mipmap and then use that to update the
    // texture.
    backing_bitmap_->SetRect(
        level, dst_left, dst_top, src_width, src_height, src_data, src_pitch);
    UpdateBackedMipLevel(level);
  } else {
    renderer_->MakeCurrentLazy();
    glBindTexture(GL_TEXTURE_2D, gl_texture_);
    GLenum gl_internal_format = 0;
    GLenum gl_data_type = 0;
    GLenum gl_format = GLFormatFromO3DFormat(format(), &gl_internal_format,
                                             &gl_data_type);
    if (gl_format) {
      if (src_pitch == image::ComputePitch(format(), src_width)) {
        glTexSubImage2D(GL_TEXTURE_2D, level,
                        dst_left, dst_top,
                        src_width, src_height,
                        gl_format,
                        gl_data_type,
                        src_data);
      } else {
        int limit = src_height;
        for (int yy = 0; yy < limit; ++yy) {
          glTexSubImage2D(GL_TEXTURE_2D, level,
                          dst_left, dst_top + yy,
                          src_width, 1,
                          gl_format,
                          gl_data_type,
                          src_data);
          src_data = AddPointerOffset<const void*>(src_data, src_pitch);
        }
      }
    } else {
      glCompressedTexSubImage2D(
          GL_TEXTURE_2D, level, 0, 0, src_width, src_height,
          gl_internal_format,
          image::ComputeMipChainSize(src_width, src_height, format(), 1),
          src_data);
    }
  }
}

// Locks the given mipmap level of this texture for loading from main memory,
// and returns a pointer to the buffer.
bool Texture2DGL::PlatformSpecificLock(
    int level, void** data, int* pitch, Texture::AccessMode mode) {
  DLOG(INFO) << "Texture2DGL Lock";
  DCHECK(data);
  DCHECK(pitch);
  DCHECK_GE(level, 0);
  DCHECK_LT(level, levels());
  renderer_->MakeCurrentLazy();
  if (!backing_bitmap_->image_data()) {
    DCHECK_EQ(has_levels_, 0u);
    backing_bitmap_->Allocate(format(), width(), height(), levels(),
                              Bitmap::IMAGE);
  }
  *data = backing_bitmap_->GetMipData(level);
  unsigned int mip_width = image::ComputeMipDimension(level, width());
  if (!IsCompressed()) {
    *pitch = image::ComputePitch(format(), mip_width);
  } else {
    unsigned blocks_across = (mip_width + 3) / 4;
    unsigned bytes_per_block = format() == Texture::DXT1 ? 8 : 16;
    unsigned bytes_per_row = bytes_per_block * blocks_across;
    *pitch = bytes_per_row;
  }
  if (mode != kWriteOnly && !HasLevel(level)) {
    DCHECK(!resize_to_pot_);
    GLenum gl_internal_format = 0;
    GLenum gl_data_type = 0;
    GLenum gl_format = GLFormatFromO3DFormat(format(),
                                             &gl_internal_format,
                                             &gl_data_type);
    glBindTexture(GL_TEXTURE_2D, gl_texture_);
    glGetTexImage(GL_TEXTURE_2D, level, gl_format, gl_data_type, *data);
    has_levels_ |= 1 << level;
  }
  locked_levels_ |= 1 << level;
  CHECK_GL_ERROR();
  return true;
}

// Unlocks the given mipmap level of this texture, uploading the main memory
// data buffer to GL.
bool Texture2DGL::PlatformSpecificUnlock(int level) {
  DLOG(INFO) << "Texture2DGL Unlock";
  DCHECK_GE(level, 0);
  DCHECK_LT(level, levels());
  if (LockedMode(level) != kReadOnly) {
    renderer_->MakeCurrentLazy();
    UpdateBackedMipLevel(level);
  }
  locked_levels_ &= ~(1 << level);
  if (!resize_to_pot_ && (locked_levels_ == 0)) {
    backing_bitmap_->FreeData();
    has_levels_ = 0;
  }
  CHECK_GL_ERROR();
  return true;
}

RenderSurface::Ref Texture2DGL::PlatformSpecificGetRenderSurface(
    int mip_level) {
  DCHECK_LT(mip_level, levels());
  if (!render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to get RenderSurface from non-render-surface-enabled"
        << " Texture: " << name();
    return RenderSurface::Ref(NULL);
  }

  if (mip_level >= levels() || mip_level < 0) {
    O3D_ERROR(service_locator())
        << "Attempting to access non-existent mip_level " << mip_level
        << " in render-target texture \"" << name() << "\".";
    return RenderSurface::Ref(NULL);
  }

  return RenderSurface::Ref(new RenderSurfaceGL(
      service_locator(),
      width()>> mip_level,
      height() >> mip_level,
      0,
      mip_level,
      this));
}

const Texture::RGBASwizzleIndices& Texture2DGL::GetABGR32FSwizzleIndices() {
  return g_gl_abgr32f_swizzle_indices;
}

// TextureCUBEGL ---------------------------------------------------------------

// Creates a texture from a pre-existing GL texture object.
TextureCUBEGL::TextureCUBEGL(ServiceLocator* service_locator,
                             GLint texture,
                             Texture::Format format,
                             int levels,
                             int edge_length,
                             bool resize_to_pot,
                             bool enable_render_surfaces)
    : TextureCUBE(service_locator,
                  edge_length,
                  format,
                  levels,
                  enable_render_surfaces),
      resize_to_pot_(resize_to_pot),
      renderer_(static_cast<RendererGL*>(
          service_locator->GetService<Renderer>())),
      gl_texture_(texture) {
  DLOG(INFO) << "TextureCUBEGL Construct";
  for (int ii = 0; ii < static_cast<int>(NUMBER_OF_FACES); ++ii) {
    backing_bitmaps_[ii] = Bitmap::Ref(new Bitmap(service_locator));
    has_levels_[ii] = 0;
    locked_levels_[ii] = 0;
  }
}

TextureCUBEGL::~TextureCUBEGL() {
  DLOG(INFO) << "TextureCUBEGL Destruct";
  if (gl_texture_) {
    renderer_->MakeCurrentLazy();
    glDeleteTextures(1, &gl_texture_);
    gl_texture_ = 0;
  }
  CHECK_GL_ERROR();
}

static const int kCubemapFaceList[] = {
  GL_TEXTURE_CUBE_MAP_POSITIVE_X,
  GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
  GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
  GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
  GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
  GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
};

// Create a new Cube texture from scratch.
TextureCUBEGL* TextureCUBEGL::Create(ServiceLocator* service_locator,
                                     Texture::Format format,
                                     int levels,
                                     int edge_length,
                                     bool enable_render_surfaces) {
  DLOG(INFO) << "TextureCUBEGL Create";
  CHECK_GL_ERROR();
  RendererGL *renderer = static_cast<RendererGL *>(
      service_locator->GetService<Renderer>());
  renderer->MakeCurrentLazy();

  bool resize_to_pot = !renderer->supports_npot() &&
                       !image::IsPOT(edge_length, edge_length);

  // Get gl formats
  GLenum gl_internal_format = 0;
  GLenum gl_data_type = 0;
  GLenum gl_format = GLFormatFromO3DFormat(format,
                                           &gl_internal_format,
                                           &gl_data_type);
  if (gl_internal_format == 0) {
    DLOG(ERROR) << "Unsupported format in TextureCUBEGL::Create.";
    return NULL;
  }

  // Creates the OpenGL texture object, with all the required mip levels.
  GLuint gl_texture = 0;
  glGenTextures(1, &gl_texture);
  glBindTexture(GL_TEXTURE_CUBE_MAP, gl_texture);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL,
                  levels - 1);

  for (int face = 0; face < static_cast<int>(NUMBER_OF_FACES); ++face) {
    CreateGLImages(kCubemapFaceList[face], gl_internal_format,
                   gl_format, gl_data_type,
                   static_cast<CubeFace>(face),
                   format, levels, edge_length, edge_length,
                   resize_to_pot);
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP,
                  GL_TEXTURE_MIN_FILTER,
                  GL_NEAREST_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Create a new texture object, which initializes the base Texture class
  // from the Bitmap information.
  TextureCUBEGL* texture = new TextureCUBEGL(service_locator,
                                             gl_texture,
                                             format,
                                             levels,
                                             edge_length,
                                             resize_to_pot,
                                             enable_render_surfaces);
  // If the hardware does not support npot textures, allocate a 0-initialized
  // mip-chain here for use during TextureCUBEGL::Lock.
  if (resize_to_pot) {
    for (int face = 0; face < static_cast<int>(NUMBER_OF_FACES); ++face) {
      texture->backing_bitmaps_[face]->Allocate(
          format, edge_length, edge_length, levels, Bitmap::IMAGE);
      texture->has_levels_[face] = (1 << levels) - 1;
    }
  }
  CHECK_GL_ERROR();
  DLOG(INFO) << "Created cube map texture (GLuint=" << gl_texture << ")";
  return texture;
}

void TextureCUBEGL::UpdateBackedMipLevel(unsigned int level,
                                         TextureCUBE::CubeFace face) {
  Bitmap* backing_bitmap = backing_bitmaps_[face].Get();
  DCHECK_LT(static_cast<int>(level), levels());
  DCHECK(backing_bitmap->image_data());
  DCHECK_EQ(backing_bitmap->width(), static_cast<unsigned int>(edge_length()));
  DCHECK_EQ(backing_bitmap->height(), static_cast<unsigned int>(edge_length()));
  DCHECK_EQ(backing_bitmap->format(), format());
  renderer_->MakeCurrentLazy();
  glBindTexture(GL_TEXTURE_2D, gl_texture_);
  UpdateGLImageFromBitmap(kCubemapFaceList[face], level, face,
                          *backing_bitmap,
                          resize_to_pot_);
}

RenderSurface::Ref TextureCUBEGL::PlatformSpecificGetRenderSurface(
    TextureCUBE::CubeFace face,
    int mip_level) {
  DCHECK_LT(mip_level, levels());
  if (!render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to get RenderSurface from non-render-surface-enabled"
        << " Texture: " << name();
    return RenderSurface::Ref(NULL);
  }

  if (mip_level >= levels() || mip_level < 0) {
    O3D_ERROR(service_locator())
        << "Attempting to access non-existent mip_level " << mip_level
        << " in render-target texture \"" << name() << "\".";
    return RenderSurface::Ref(NULL);
  }

  return RenderSurface::Ref(new RenderSurfaceGL(
      service_locator(),
      edge_length() >> mip_level,
      edge_length() >> mip_level,
      kCubemapFaceList[face],
      mip_level,
      this));
}

void TextureCUBEGL::SetRect(TextureCUBE::CubeFace face,
                            int level,
                            unsigned dst_left,
                            unsigned dst_top,
                            unsigned src_width,
                            unsigned src_height,
                            const void* src_data,
                            int src_pitch) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to SetRect non-existent level " << level
        << " on Texture \"" << name() << "\"";
    return;
  }
  if (render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to SetRect a render-target texture: " << name();
    return;
  }

  unsigned mip_width = image::ComputeMipDimension(level, edge_length());
  unsigned mip_height = mip_width;

  if (dst_left + src_width > mip_width ||
      dst_top + src_height > mip_height) {
    O3D_ERROR(service_locator())
        << "SetRect(" << level << ", " << dst_left << ", " << dst_top << ", "
        << src_width << ", " << src_height << ") out of range for texture << \""
        << name() << "\"";
    return;
  }

  bool entire_rect = dst_left == 0 && dst_top == 0 &&
                     src_width == mip_width && src_height == mip_height;
  bool compressed = IsCompressed();

  if (compressed && !entire_rect) {
    O3D_ERROR(service_locator())
        << "SetRect must be full rectangle for compressed textures";
    return;
  }

  if (resize_to_pot_) {
    Bitmap* backing_bitmap = backing_bitmaps_[face].Get();
    DCHECK(backing_bitmap->image_data());
    DCHECK(!compressed);
    // We need to update the backing mipmap and then use that to update the
    // texture.
    backing_bitmap->SetRect(
        level, dst_left, dst_top, src_width, src_height, src_data, src_pitch);
    UpdateBackedMipLevel(level, face);
  } else {
    // TODO(gman): Should this bind be using a FACE id?
    renderer_->MakeCurrentLazy();
    glBindTexture(GL_TEXTURE_2D, gl_texture_);
    GLenum gl_internal_format = 0;
    GLenum gl_data_type = 0;
    GLenum gl_format = GLFormatFromO3DFormat(format(), &gl_internal_format,
                                             &gl_data_type);
    int gl_face = kCubemapFaceList[face];
    if (gl_format) {
      if (src_pitch == image::ComputePitch(format(), src_width)) {
        glTexSubImage2D(gl_face, level,
                        dst_left, dst_top,
                        src_width, src_height,
                        gl_format,
                        gl_data_type,
                        src_data);
      } else {
        int limit = src_height;
        for (int yy = 0; yy < limit; ++yy) {
          glTexSubImage2D(gl_face, level,
                          dst_left, dst_top + yy,
                          src_width, 1,
                          gl_format,
                          gl_data_type,
                          src_data);
          src_data = AddPointerOffset<const void*>(src_data, src_pitch);
        }
      }
    } else {
      glCompressedTexSubImage2D(
          gl_face, level, 0, 0, src_width, src_height,
          gl_internal_format,
          image::ComputeMipChainSize(src_width, src_height, format(), 1),
          src_data);
    }
  }
}

// Locks the given face and mipmap level of this texture for loading from
// main memory, and returns a pointer to the buffer.
bool TextureCUBEGL::PlatformSpecificLock(
    CubeFace face, int level, void** data, int* pitch,
    Texture::AccessMode mode) {
  DLOG(INFO) << "TextureCUBEGL Lock";
  DCHECK_GE(level, 0);
  DCHECK_LT(level, levels());
  renderer_->MakeCurrentLazy();
  Bitmap* backing_bitmap = backing_bitmaps_[face].Get();
  if (!backing_bitmap->image_data()) {
    for (int i = 0; i < static_cast<int>(NUMBER_OF_FACES); ++i) {
      DCHECK_EQ(has_levels_[i], 0u);
    }
    backing_bitmap->Allocate(format(), edge_length(), edge_length(), levels(),
                             Bitmap::IMAGE);
  }
  *data = backing_bitmap->GetMipData(level);
  unsigned int mip_width = image::ComputeMipDimension(level, edge_length());
  if (!IsCompressed()) {
    *pitch = image::ComputePitch(format(), mip_width);
  } else {
    unsigned blocks_across = (mip_width + 3) / 4;
    unsigned bytes_per_block = format() == Texture::DXT1 ? 8 : 16;
    unsigned bytes_per_row = bytes_per_block * blocks_across;
    *pitch = bytes_per_row;
  }
  GLenum gl_target = kCubemapFaceList[face];
  if (mode != kWriteOnly && !HasLevel(face, level)) {
    // TODO: add some API so we don't have to copy back the data if we
    // will rewrite it all.
    DCHECK(!resize_to_pot_);
    GLenum gl_internal_format = 0;
    GLenum gl_data_type = 0;
    GLenum gl_format = GLFormatFromO3DFormat(format(),
                                             &gl_internal_format,
                                             &gl_data_type);
    glBindTexture(GL_TEXTURE_CUBE_MAP, gl_texture_);
    glGetTexImage(gl_target, level, gl_format, gl_data_type, *data);
    has_levels_[face] |= 1 << level;
  }
  CHECK_GL_ERROR();

  locked_levels_[face] |= 1 << level;

  return false;
}

// Unlocks the given face and mipmap level of this texture.
bool TextureCUBEGL::PlatformSpecificUnlock(CubeFace face, int level) {
  DLOG(INFO) << "TextureCUBEGL Unlock";
  DCHECK_GE(level, 0);
  DCHECK_LT(level, levels());
  if (LockedMode(face, level) != kReadOnly) {
    renderer_->MakeCurrentLazy();
    UpdateBackedMipLevel(level, face);
  }
  locked_levels_[face] &= ~(1 << level);

  if (!resize_to_pot_) {
    // See if we can throw away the backing bitmap.
    Bitmap* backing_bitmap = backing_bitmaps_[face].Get();
    bool has_locked_level = false;
    for (int i = 0; i < static_cast<int>(NUMBER_OF_FACES); ++i) {
      if (locked_levels_[i]) {
        has_locked_level = true;
        break;
      }
    }
    if (!has_locked_level) {
      backing_bitmap->FreeData();
      for (int i = 0; i < static_cast<int>(NUMBER_OF_FACES); ++i) {
        has_levels_[i] = 0;
      }
    }
  }
  CHECK_GL_ERROR();
  return false;
}

const Texture::RGBASwizzleIndices& TextureCUBEGL::GetABGR32FSwizzleIndices() {
  return g_gl_abgr32f_swizzle_indices;
}

}  // namespace o3d
