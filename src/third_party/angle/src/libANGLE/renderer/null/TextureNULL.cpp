//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TextureNULL.cpp:
//    Implements the class methods for TextureNULL.
//

#include "libANGLE/renderer/null/TextureNULL.h"

#include "common/debug.h"

namespace rx
{

TextureNULL::TextureNULL(const gl::TextureState &state) : TextureImpl(state)
{
}

TextureNULL::~TextureNULL()
{
}

gl::Error TextureNULL::setImage(const gl::Context *context,
                                const gl::ImageIndex &index,
                                GLenum internalFormat,
                                const gl::Extents &size,
                                GLenum format,
                                GLenum type,
                                const gl::PixelUnpackState &unpack,
                                const uint8_t *pixels)
{
    // TODO(geofflang): Read all incoming pixel data (maybe hash it?) to make sure we don't read out
    // of bounds due to validation bugs.
    return gl::NoError();
}

gl::Error TextureNULL::setSubImage(const gl::Context *context,
                                   const gl::ImageIndex &index,
                                   const gl::Box &area,
                                   GLenum format,
                                   GLenum type,
                                   const gl::PixelUnpackState &unpack,
                                   gl::Buffer *unpackBuffer,
                                   const uint8_t *pixels)
{
    return gl::NoError();
}

gl::Error TextureNULL::setCompressedImage(const gl::Context *context,
                                          const gl::ImageIndex &index,
                                          GLenum internalFormat,
                                          const gl::Extents &size,
                                          const gl::PixelUnpackState &unpack,
                                          size_t imageSize,
                                          const uint8_t *pixels)
{
    return gl::NoError();
}

gl::Error TextureNULL::setCompressedSubImage(const gl::Context *context,
                                             const gl::ImageIndex &index,
                                             const gl::Box &area,
                                             GLenum format,
                                             const gl::PixelUnpackState &unpack,
                                             size_t imageSize,
                                             const uint8_t *pixels)
{
    return gl::NoError();
}

gl::Error TextureNULL::copyImage(const gl::Context *context,
                                 const gl::ImageIndex &index,
                                 const gl::Rectangle &sourceArea,
                                 GLenum internalFormat,
                                 gl::Framebuffer *source)
{
    return gl::NoError();
}

gl::Error TextureNULL::copySubImage(const gl::Context *context,
                                    const gl::ImageIndex &index,
                                    const gl::Offset &destOffset,
                                    const gl::Rectangle &sourceArea,
                                    gl::Framebuffer *source)
{
    return gl::NoError();
}

gl::Error TextureNULL::setStorage(const gl::Context *context,
                                  gl::TextureType type,
                                  size_t levels,
                                  GLenum internalFormat,
                                  const gl::Extents &size)
{
    return gl::NoError();
}

gl::Error TextureNULL::setEGLImageTarget(const gl::Context *context,
                                         gl::TextureType type,
                                         egl::Image *image)
{
    return gl::NoError();
}

gl::Error TextureNULL::setImageExternal(const gl::Context *context,
                                        gl::TextureType type,
                                        egl::Stream *stream,
                                        const egl::Stream::GLTextureDescription &desc)
{
    return gl::NoError();
}

gl::Error TextureNULL::generateMipmap(const gl::Context *context)
{
    return gl::NoError();
}

gl::Error TextureNULL::setBaseLevel(const gl::Context *context, GLuint baseLevel)
{
    return gl::NoError();
}

gl::Error TextureNULL::bindTexImage(const gl::Context *context, egl::Surface *surface)
{
    return gl::NoError();
}

gl::Error TextureNULL::releaseTexImage(const gl::Context *context)
{
    return gl::NoError();
}

gl::Error TextureNULL::syncState(const gl::Context *context,
                                 const gl::Texture::DirtyBits &dirtyBits)
{
    return gl::NoError();
}

gl::Error TextureNULL::setStorageMultisample(const gl::Context *context,
                                             gl::TextureType type,
                                             GLsizei samples,
                                             GLint internalformat,
                                             const gl::Extents &size,
                                             bool fixedSampleLocations)
{
    return gl::NoError();
}

gl::Error TextureNULL::initializeContents(const gl::Context *context,
                                          const gl::ImageIndex &imageIndex)
{
    return gl::NoError();
}

}  // namespace rx
