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


// This file implements the texture-related GAPI functions on GL.

#include "command_buffer/service/gapi_gl.h"
#include "command_buffer/service/texture_gl.h"

namespace command_buffer {
namespace o3d {

namespace {

// Gets the GL internal format, format and type corresponding to a command
// buffer texture format.
bool GetGLFormatType(texture::Format format,
                     GLenum *internal_format,
                     GLenum *gl_format,
                     GLenum *gl_type) {
  switch (format) {
    case texture::kXRGB8: {
      *internal_format = GL_RGB;
      *gl_format = GL_BGRA;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
    }
    case texture::kARGB8: {
      *internal_format = GL_RGBA;
      *gl_format = GL_BGRA;
      *gl_type = GL_UNSIGNED_BYTE;
      break;
    }
    case texture::kABGR16F: {
      *internal_format = GL_RGBA16F_ARB;
      *gl_format = GL_RGBA;
      *gl_type = GL_HALF_FLOAT_ARB;
      break;
    }
    case texture::kR32F: {
      *internal_format = GL_LUMINANCE32F_ARB;
      *gl_format = GL_LUMINANCE;
      *gl_type = GL_FLOAT;
      break;
    }
    case texture::kABGR32F: {
      *internal_format = GL_RGBA32F_ARB;
      *gl_format = GL_BGRA;
      *gl_type = GL_FLOAT;
      break;
    }
    case texture::kDXT1: {
      *internal_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
      *gl_format = 0;
      *gl_type = 0;
      break;
    }
    // TODO(petersont): Add DXT3/5 support.
    default:
      return false;
  }

  return true;
}

// Helper class used to prepare image data to match the layout that
// glTexImage* and glCompressedTexImage* expect.
class SetImageHelper {
 public:
  SetImageHelper()
      : buffer_(NULL),
        image_data_(NULL),
        image_size_(0) {
  }

  // Initializes the helper with the input data, re-using the input buffer if
  // possible, or copying it into a temporary one.
  bool Initialize(const MipLevelInfo &mip_info,
                  const Volume& volume,
                  unsigned int row_pitch,
                  unsigned int slice_pitch,
                  unsigned int src_size,
                  const void *data) {
    TransferInfo src_transfer_info;
    MakeTransferInfo(&src_transfer_info, mip_info, volume, row_pitch,
                     slice_pitch);
    if (!CheckVolume(mip_info, volume) ||
        src_size < src_transfer_info.total_size)
      return false;
    if (!src_transfer_info.packed) {
      TransferInfo dst_transfer_info;
      MakePackedTransferInfo(&dst_transfer_info, mip_info, volume);
      buffer_.reset(new unsigned char[dst_transfer_info.total_size]);
      TransferVolume(volume, mip_info, dst_transfer_info, buffer_.get(),
                     src_transfer_info, data);
      image_data_ = buffer_.get();
      image_size_ = dst_transfer_info.total_size;
    } else {
      image_data_ = data;
      image_size_ = src_transfer_info.total_size;
    }
    return true;
  }

  // Gets the buffer that contains the data in the GL format.
  const void *image_data() { return image_data_; }
  // Gets the size of the buffer as GL expects it.
  unsigned int image_size() { return image_size_; }

 private:
  scoped_array<unsigned char> buffer_;
  const void *image_data_;
  unsigned int image_size_;
  DISALLOW_COPY_AND_ASSIGN(SetImageHelper);
};

// Helper class used to retrieve image data to match the layout that
// glGetTexImage and glGetCompressedTexImage expect.
class GetImageHelper {
 public:
  GetImageHelper()
      : dst_data_(NULL),
        buffer_(NULL),
        image_data_(NULL) {
    memset(&mip_info_, 0, sizeof(mip_info_));
    memset(&volume_, 0, sizeof(volume_));
    memset(&dst_transfer_info_, 0, sizeof(dst_transfer_info_));
    memset(&src_transfer_info_, 0, sizeof(src_transfer_info_));
  }

  // Initialize the helper to make available a buffer to get the data from GL.
  // It will re-use the passed in buffer if the layout matches GL, or allocate
  // a temporary one.
  bool Initialize(const MipLevelInfo &mip_info,
                  const Volume& volume,
                  unsigned int row_pitch,
                  unsigned int slice_pitch,
                  unsigned int dst_size,
                  void *dst_data) {
    mip_info_ = mip_info;
    volume_ = volume;
    dst_data_ = dst_data;
    MakeTransferInfo(&dst_transfer_info_, mip_info, volume, row_pitch,
                     slice_pitch);
    if (!CheckVolume(mip_info, volume) ||
        dst_size < dst_transfer_info_.total_size)
      return false;

    if (!IsFullVolume(mip_info, volume) || !dst_transfer_info_.packed) {
      // We can only retrieve the full image from GL.
      Volume full_volume = {
        0, 0, 0,
        mip_info.width, mip_info.height, mip_info.depth
      };
      MakePackedTransferInfo(&src_transfer_info_, mip_info, full_volume);
      buffer_.reset(new unsigned char[src_transfer_info_.total_size]);
      image_data_ = buffer_.get();
    } else {
      image_data_ = dst_data;
    }
    return true;
  }

  // Finalize the helper, copying the data into the final buffer if needed.
  void Finalize() {
    if (!buffer_.get()) return;
    unsigned int offset =
        volume_.x / mip_info_.block_size_x * mip_info_.block_bpp +
        volume_.y / mip_info_.block_size_y * src_transfer_info_.row_pitch +
        volume_.z * src_transfer_info_.slice_pitch;
    src_transfer_info_.row_size = dst_transfer_info_.row_size;
    TransferVolume(volume_, mip_info_, dst_transfer_info_, dst_data_,
                   src_transfer_info_, buffer_.get() + offset);
  }

  // Gets the buffer that can receive the data from GL.
  void *image_data() { return image_data_; }

 private:
  MipLevelInfo mip_info_;
  Volume volume_;
  TransferInfo dst_transfer_info_;
  TransferInfo src_transfer_info_;
  void *dst_data_;
  scoped_array<unsigned char> buffer_;
  void *image_data_;
  DISALLOW_COPY_AND_ASSIGN(GetImageHelper);
};

}  // anonymous namespace

TextureGL::~TextureGL() {
  glDeleteTextures(1, &gl_texture_);
  CHECK_GL_ERROR();
}

Texture2DGL *Texture2DGL::Create(unsigned int width,
                                 unsigned int height,
                                 unsigned int levels,
                                 texture::Format format,
                                 unsigned int flags,
                                 bool enable_render_surfaces) {
  DCHECK_GT(width, 0U);
  DCHECK_GT(height, 0U);
  DCHECK_GT(levels, 0U);
  GLenum gl_internal_format = 0;
  GLenum gl_format = 0;
  GLenum gl_type = 0;
  bool r = GetGLFormatType(format, &gl_internal_format, &gl_format, &gl_type);
  DCHECK(r);  // Was checked in the decoder.
  GLuint gl_texture = 0;
  glGenTextures(1, &gl_texture);
  glBindTexture(GL_TEXTURE_2D, gl_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels - 1);
  // glCompressedTexImage2D does't accept NULL as a parameter, so we need
  // to pass in some data.
  scoped_array<unsigned char> buffer;
  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format, width, height, 1, 0);
  unsigned int size = GetMipLevelSize(mip_info);
  buffer.reset(new unsigned char[size]);
  memset(buffer.get(), 0, size);

  unsigned int mip_width = width;
  unsigned int mip_height = height;

  for (unsigned int i = 0; i < levels; ++i) {
    if (gl_format) {
      glTexImage2D(GL_TEXTURE_2D, i, gl_internal_format, mip_width, mip_height,
                   0, gl_format, gl_type, buffer.get());
    } else {
      MipLevelInfo mip_info;
      MakeMipLevelInfo(&mip_info, format, width, height, 1, i);
      unsigned int size = GetMipLevelSize(mip_info);
      glCompressedTexImage2D(GL_TEXTURE_2D, i, gl_internal_format, mip_width,
                             mip_height, 0, size, buffer.get());
    }
    mip_width = std::max(1U, mip_width >> 1);
    mip_height = std::max(1U, mip_height >> 1);
  }
  return new Texture2DGL(
      levels, format, enable_render_surfaces, flags, width, height, gl_texture);
}

// Sets data into a 2D texture resource.
bool Texture2DGL::SetData(const Volume& volume,
                          unsigned int level,
                          texture::Face face,
                          unsigned int row_pitch,
                          unsigned int slice_pitch,
                          unsigned int size,
                          const void *data) {
  if (level >= levels())
    return false;
  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format(), width_, height_, 1, level);
  SetImageHelper helper;
  if (!helper.Initialize(mip_info, volume, row_pitch, slice_pitch, size, data))
    return false;

  GLenum gl_internal_format = 0;
  GLenum gl_format = 0;
  GLenum gl_type = 0;
  bool r = GetGLFormatType(format(), &gl_internal_format, &gl_format, &gl_type);
  DCHECK(r);
  glBindTexture(GL_TEXTURE_2D, gl_texture_);
  if (gl_format) {
    glTexSubImage2D(GL_TEXTURE_2D, level, volume.x, volume.y, volume.width,
                    volume.height, gl_format, gl_type, helper.image_data());
  } else {
    glCompressedTexSubImage2D(GL_TEXTURE_2D, level, volume.x, volume.y,
                              volume.width, volume.height, gl_internal_format,
                              helper.image_size(), helper.image_data());
  }
  return true;
}

bool Texture2DGL::GetData(const Volume& volume,
                          unsigned int level,
                          texture::Face face,
                          unsigned int row_pitch,
                          unsigned int slice_pitch,
                          unsigned int size,
                          void *data) {
  if (level >= levels())
    return false;
  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format(), width_, height_, 1, level);
  GetImageHelper helper;
  if (!helper.Initialize(mip_info, volume, row_pitch, slice_pitch, size, data))
    return false;

  GLenum gl_internal_format = 0;
  GLenum gl_format = 0;
  GLenum gl_type = 0;
  bool r = GetGLFormatType(format(), &gl_internal_format, &gl_format, &gl_type);
  DCHECK(r);
  glBindTexture(GL_TEXTURE_2D, gl_texture_);
  if (gl_format) {
    glGetTexImage(GL_TEXTURE_2D, level, gl_format, gl_type,
                  helper.image_data());
  } else {
    glGetCompressedTexImage(GL_TEXTURE_2D, level, helper.image_data());
  }

  helper.Finalize();
  return true;
}

bool Texture2DGL::CreateRenderSurface(int width,
                                      int height,
                                      int mip_level,
                                      int side) {
  return false;
}

bool Texture2DGL::InstallFrameBufferObjects(
    RenderSurfaceGL *gl_surface) {
  ::glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT,
        GL_COLOR_ATTACHMENT0_EXT,
        GL_TEXTURE_2D,
        gl_texture_,
        gl_surface->mip_level());

  GLenum framebuffer_status = ::glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (GL_FRAMEBUFFER_COMPLETE_EXT != framebuffer_status) {
    return false;
  }

  return true;
}

Texture3DGL *Texture3DGL::Create(unsigned int width,
                                 unsigned int height,
                                 unsigned int depth,
                                 unsigned int levels,
                                 texture::Format format,
                                 unsigned int flags,
                                 bool enable_render_surfaces) {
  DCHECK_GT(width, 0U);
  DCHECK_GT(height, 0U);
  DCHECK_GT(depth, 0U);
  DCHECK_GT(levels, 0U);
  GLenum gl_internal_format = 0;
  GLenum gl_format = 0;
  GLenum gl_type = 0;
  bool r = GetGLFormatType(format, &gl_internal_format, &gl_format, &gl_type);
  DCHECK(r);  // Was checked in the decoder.
  GLuint gl_texture = 0;
  glGenTextures(1, &gl_texture);
  glBindTexture(GL_TEXTURE_3D, gl_texture);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, levels - 1);
  // glCompressedTexImage3D does't accept NULL as a parameter, so we need
  // to pass in some data.
  scoped_array<unsigned char> buffer;
  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format, width, height, depth, 0);
  unsigned int size = GetMipLevelSize(mip_info);
  buffer.reset(new unsigned char[size]);
  memset(buffer.get(), 0, size);

  unsigned int mip_width = width;
  unsigned int mip_height = height;
  unsigned int mip_depth = depth;
  for (unsigned int i = 0; i < levels; ++i) {
    if (gl_format) {
      glTexImage3D(GL_TEXTURE_3D, i, gl_internal_format, mip_width, mip_height,
                   mip_depth, 0, gl_format, gl_type, buffer.get());
    } else {
      MipLevelInfo mip_info;
      MakeMipLevelInfo(&mip_info, format, width, height, depth, i);
      unsigned int size = GetMipLevelSize(mip_info);
      glCompressedTexImage3D(GL_TEXTURE_3D, i, gl_internal_format, mip_width,
                             mip_height, mip_depth, 0, size, buffer.get());
    }
    mip_width = std::max(1U, mip_width >> 1);
    mip_height = std::max(1U, mip_height >> 1);
    mip_depth = std::max(1U, mip_depth >> 1);
  }
  return new Texture3DGL(levels, format, enable_render_surfaces, flags, width,
      height, depth, gl_texture);
}

bool Texture3DGL::SetData(const Volume& volume,
                          unsigned int level,
                          texture::Face face,
                          unsigned int row_pitch,
                          unsigned int slice_pitch,
                          unsigned int size,
                          const void *data) {
  if (level >= levels())
    return false;
  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format(), width_, height_, depth_, level);
  SetImageHelper helper;
  if (!helper.Initialize(mip_info, volume, row_pitch, slice_pitch, size, data))
    return false;

  GLenum gl_internal_format = 0;
  GLenum gl_format = 0;
  GLenum gl_type = 0;
  bool r = GetGLFormatType(format(), &gl_internal_format, &gl_format, &gl_type);
  DCHECK(r);
  glBindTexture(GL_TEXTURE_3D, gl_texture_);
  if (gl_format) {
    glTexSubImage3D(GL_TEXTURE_3D, level, volume.x, volume.y, volume.z,
                    volume.width, volume.height, volume.depth,
                    gl_format, gl_type, helper.image_data());
  } else {
    glCompressedTexSubImage3D(GL_TEXTURE_3D, level, volume.x, volume.y,
                              volume.z, volume.width, volume.height,
                              volume.depth, gl_internal_format,
                              helper.image_size(), helper.image_data());
  }
  return true;
}

bool Texture3DGL::GetData(const Volume& volume,
                          unsigned int level,
                          texture::Face face,
                          unsigned int row_pitch,
                          unsigned int slice_pitch,
                          unsigned int size,
                          void *data) {
  if (level >= levels())
    return false;
  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format(), width_, height_, depth_, level);
  GetImageHelper helper;
  if (!helper.Initialize(mip_info, volume, row_pitch, slice_pitch, size, data))
    return false;

  GLenum gl_internal_format = 0;
  GLenum gl_format = 0;
  GLenum gl_type = 0;
  bool r = GetGLFormatType(format(), &gl_internal_format, &gl_format, &gl_type);
  DCHECK(r);
  glBindTexture(GL_TEXTURE_3D, gl_texture_);
  if (gl_format) {
    glGetTexImage(GL_TEXTURE_3D, level, gl_format, gl_type,
                  helper.image_data());
  } else {
    glGetCompressedTexImage(GL_TEXTURE_3D, level, helper.image_data());
  }

  helper.Finalize();
  return true;
}

bool Texture3DGL::CreateRenderSurface(int width,
                                       int height,
                                       int mip_level,
                                       int side) {
  return false;
}

bool Texture3DGL::InstallFrameBufferObjects(
    RenderSurfaceGL *gl_surface) {
  return false;
}

TextureCubeGL *TextureCubeGL::Create(unsigned int side,
                                     unsigned int levels,
                                     texture::Format format,
                                     unsigned int flags,
                                     bool enable_render_surfaces) {
  DCHECK_GT(side, 0U);
  DCHECK_GT(levels, 0U);
  GLenum gl_internal_format = 0;
  GLenum gl_format = 0;
  GLenum gl_type = 0;
  bool r = GetGLFormatType(format, &gl_internal_format, &gl_format, &gl_type);
  DCHECK(r);  // Was checked in the decoder.
  GLuint gl_texture = 0;
  glGenTextures(1, &gl_texture);
  glBindTexture(GL_TEXTURE_CUBE_MAP, gl_texture);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL,
                  levels-1);
  // glCompressedTexImage2D does't accept NULL as a parameter, so we need
  // to pass in some data.
  scoped_array<unsigned char> buffer;
  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format, side, side, 1, 0);
  unsigned int size = GetMipLevelSize(mip_info);
  buffer.reset(new unsigned char[size]);
  memset(buffer.get(), 0, size);

  unsigned int mip_side = side;
  for (unsigned int i = 0; i < levels; ++i) {
    if (gl_format) {
      for (unsigned int face = 0; face < 6; ++face) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, i,
                     gl_internal_format, mip_side, mip_side,
                     0, gl_format, gl_type, buffer.get());
      }
    } else {
      MipLevelInfo mip_info;
      MakeMipLevelInfo(&mip_info, format, side, side, 1, i);
      unsigned int size = GetMipLevelSize(mip_info);
      for (unsigned int face = 0; face < 6; ++face) {
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, i,
                               gl_internal_format, mip_side, mip_side, 0, size,
                               buffer.get());
      }
    }
    mip_side = std::max(1U, mip_side >> 1);
  }
  return new TextureCubeGL(
      levels, format, enable_render_surfaces, flags, side, gl_texture);
}

// Check that GL_TEXTURE_CUBE_MAP_POSITIVE_X + face yields the correct GLenum.
COMPILE_ASSERT(GL_TEXTURE_CUBE_MAP_POSITIVE_X + texture::kFacePositiveX ==
               GL_TEXTURE_CUBE_MAP_POSITIVE_X, POSITIVE_X_ENUMS_DO_NOT_MATCH);
COMPILE_ASSERT(GL_TEXTURE_CUBE_MAP_POSITIVE_X + texture::kFaceNegativeX ==
               GL_TEXTURE_CUBE_MAP_NEGATIVE_X, NEGATIVE_X_ENUMS_DO_NOT_MATCH);
COMPILE_ASSERT(GL_TEXTURE_CUBE_MAP_POSITIVE_X + texture::kFacePositiveY ==
               GL_TEXTURE_CUBE_MAP_POSITIVE_Y, POSITIVE_Y_ENUMS_DO_NOT_MATCH);
COMPILE_ASSERT(GL_TEXTURE_CUBE_MAP_POSITIVE_X + texture::kFaceNegativeY ==
               GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, NEGATIVE_Y_ENUMS_DO_NOT_MATCH);
COMPILE_ASSERT(GL_TEXTURE_CUBE_MAP_POSITIVE_X + texture::kFacePositiveZ ==
               GL_TEXTURE_CUBE_MAP_POSITIVE_Z, POSITIVE_Z_ENUMS_DO_NOT_MATCH);
COMPILE_ASSERT(GL_TEXTURE_CUBE_MAP_POSITIVE_X + texture::kFaceNegativeZ ==
               GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, NEGATIVE_Z_ENUMS_DO_NOT_MATCH);

bool TextureCubeGL::SetData(const Volume& volume,
                            unsigned int level,
                            texture::Face face,
                            unsigned int row_pitch,
                            unsigned int slice_pitch,
                            unsigned int size,
                            const void *data) {
  if (level >= levels())
    return false;
  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format(), side_, side_, 1, level);
  SetImageHelper helper;
  if (!helper.Initialize(mip_info, volume, row_pitch, slice_pitch, size, data))
    return false;

  GLenum gl_internal_format = 0;
  GLenum gl_format = 0;
  GLenum gl_type = 0;
  bool r = GetGLFormatType(format(), &gl_internal_format, &gl_format, &gl_type);
  DCHECK(r);
  glBindTexture(GL_TEXTURE_CUBE_MAP, gl_texture_);
  GLenum face_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
  if (gl_format) {
    glTexSubImage2D(face_target, level, volume.x, volume.y, volume.width,
                    volume.height, gl_format, gl_type, helper.image_data());
  } else {
    glCompressedTexSubImage2D(face_target, level, volume.x, volume.y,
                              volume.width, volume.height, gl_internal_format,
                              helper.image_size(), helper.image_data());
  }
  return true;
}

bool TextureCubeGL::GetData(const Volume& volume,
                            unsigned int level,
                            texture::Face face,
                            unsigned int row_pitch,
                            unsigned int slice_pitch,
                            unsigned int size,
                            void *data) {
  if (level >= levels())
    return false;
  MipLevelInfo mip_info;
  MakeMipLevelInfo(&mip_info, format(), side_, side_, 1, level);
  GetImageHelper helper;
  if (!helper.Initialize(mip_info, volume, row_pitch, slice_pitch, size, data))
    return false;

  GLenum gl_internal_format = 0;
  GLenum gl_format = 0;
  GLenum gl_type = 0;
  bool r = GetGLFormatType(format(), &gl_internal_format, &gl_format, &gl_type);
  DCHECK(r);
  glBindTexture(GL_TEXTURE_CUBE_MAP, gl_texture_);
  GLenum face_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
  if (gl_format) {
    glGetTexImage(face_target, level, gl_format, gl_type,
                  helper.image_data());
  } else {
    glGetCompressedTexImage(face_target, level, helper.image_data());
  }

  helper.Finalize();
  return true;
}

bool TextureCubeGL::CreateRenderSurface(int width,
                                        int height,
                                        int mip_level,
                                        int side) {
  return false;
}

bool TextureCubeGL::InstallFrameBufferObjects(
    RenderSurfaceGL *gl_surface) {
  ::glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT,
        GL_COLOR_ATTACHMENT0_EXT,
        gl_surface->side(),
        gl_texture_,
        gl_surface->mip_level());

  GLenum framebuffer_status = ::glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (GL_FRAMEBUFFER_COMPLETE_EXT != framebuffer_status) {
    return false;
  }

  return true;
}


// Destroys a texture resource.
parse_error::ParseError GAPIGL::DestroyTexture(ResourceId id) {
  // Dirty effect, because this texture id may be used.
  DirtyEffect();
  return textures_.Destroy(id) ?
      parse_error::kParseNoError :
      parse_error::kParseInvalidArguments;
}

// Creates a 2D texture resource.
parse_error::ParseError GAPIGL::CreateTexture2D(
    ResourceId id,
    unsigned int width,
    unsigned int height,
    unsigned int levels,
    texture::Format format,
    unsigned int flags,
    bool enable_render_surfaces) {
  Texture2DGL *texture = Texture2DGL::Create(
      width, height, levels, format, flags, enable_render_surfaces);
  if (!texture) return parse_error::kParseInvalidArguments;
  // Dirty effect, because this texture id may be used.
  DirtyEffect();
  textures_.Assign(id, texture);
  return parse_error::kParseNoError;
}

// Creates a 3D texture resource.
parse_error::ParseError GAPIGL::CreateTexture3D(
    ResourceId id,
    unsigned int width,
    unsigned int height,
    unsigned int depth,
    unsigned int levels,
    texture::Format format,
    unsigned int flags,
    bool enable_render_surfaces) {
  Texture3DGL *texture = Texture3DGL::Create(
      width, height, depth, levels, format, flags, enable_render_surfaces);
  if (!texture) return parse_error::kParseInvalidArguments;
  // Dirty effect, because this texture id may be used.
  DirtyEffect();
  textures_.Assign(id, texture);
  return parse_error::kParseNoError;
}

// Creates a cube map texture resource.
parse_error::ParseError GAPIGL::CreateTextureCube(
    ResourceId id,
    unsigned int side,
    unsigned int levels,
    texture::Format format,
    unsigned int flags,
    bool enable_render_surfaces) {
  TextureCubeGL *texture = TextureCubeGL::Create(
      side, levels, format, flags, enable_render_surfaces);
  if (!texture) return parse_error::kParseInvalidArguments;
  // Dirty effect, because this texture id may be used.
  DirtyEffect();
  textures_.Assign(id, texture);
  return parse_error::kParseNoError;
}

// Copies the data into a texture resource.
parse_error::ParseError GAPIGL::SetTextureData(
    ResourceId id,
    unsigned int x,
    unsigned int y,
    unsigned int z,
    unsigned int width,
    unsigned int height,
    unsigned int depth,
    unsigned int level,
    texture::Face face,
    unsigned int row_pitch,
    unsigned int slice_pitch,
    unsigned int size,
    const void *data) {
  TextureGL *texture = textures_.Get(id);
  if (!texture)
    return parse_error::kParseInvalidArguments;
  Volume volume = {x, y, z, width, height, depth};
  // Dirty effect: SetData may need to call glBindTexture which will mess up the
  // sampler parameters.
  DirtyEffect();
  return texture->SetData(volume, level, face, row_pitch, slice_pitch,
                          size, data) ?
      parse_error::kParseNoError :
      parse_error::kParseInvalidArguments;
}

// Copies the data from a texture resource.
parse_error::ParseError GAPIGL::GetTextureData(
    ResourceId id,
    unsigned int x,
    unsigned int y,
    unsigned int z,
    unsigned int width,
    unsigned int height,
    unsigned int depth,
    unsigned int level,
    texture::Face face,
    unsigned int row_pitch,
    unsigned int slice_pitch,
    unsigned int size,
    void *data) {
  TextureGL *texture = textures_.Get(id);
  if (!texture)
    return parse_error::kParseInvalidArguments;
  Volume volume = {x, y, z, width, height, depth};
  // Dirty effect: GetData may need to call glBindTexture which will mess up the
  // sampler parameters.
  DirtyEffect();
  return texture->GetData(volume, level, face, row_pitch, slice_pitch,
                          size, data) ?
      parse_error::kParseNoError :
      parse_error::kParseInvalidArguments;
}

}  // namespace o3d
}  // namespace command_buffer
