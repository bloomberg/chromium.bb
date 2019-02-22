//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureImpl.h: Defines the abstract rx::TextureImpl classes.

#ifndef LIBANGLE_RENDERER_TEXTUREIMPL_H_
#define LIBANGLE_RENDERER_TEXTUREIMPL_H_

#include <stdint.h>

#include "angle_gl.h"
#include "common/angleutils.h"
#include "libANGLE/Error.h"
#include "libANGLE/ImageIndex.h"
#include "libANGLE/Stream.h"
#include "libANGLE/Texture.h"
#include "libANGLE/renderer/FramebufferAttachmentObjectImpl.h"

namespace egl
{
class Surface;
class Image;
}

namespace gl
{
struct Box;
struct Extents;
struct Offset;
struct Rectangle;
class Framebuffer;
struct PixelUnpackState;
struct TextureState;
}

namespace rx
{
class ContextImpl;

class TextureImpl : public FramebufferAttachmentObjectImpl
{
  public:
    TextureImpl(const gl::TextureState &state);
    ~TextureImpl() override;

    virtual gl::Error onDestroy(const gl::Context *context);

    virtual gl::Error setImage(const gl::Context *context,
                               const gl::ImageIndex &index,
                               GLenum internalFormat,
                               const gl::Extents &size,
                               GLenum format,
                               GLenum type,
                               const gl::PixelUnpackState &unpack,
                               const uint8_t *pixels)    = 0;
    virtual gl::Error setSubImage(const gl::Context *context,
                                  const gl::ImageIndex &index,
                                  const gl::Box &area,
                                  GLenum format,
                                  GLenum type,
                                  const gl::PixelUnpackState &unpack,
                                  gl::Buffer *unpackBuffer,
                                  const uint8_t *pixels) = 0;

    virtual gl::Error setCompressedImage(const gl::Context *context,
                                         const gl::ImageIndex &index,
                                         GLenum internalFormat,
                                         const gl::Extents &size,
                                         const gl::PixelUnpackState &unpack,
                                         size_t imageSize,
                                         const uint8_t *pixels)    = 0;
    virtual gl::Error setCompressedSubImage(const gl::Context *context,
                                            const gl::ImageIndex &index,
                                            const gl::Box &area,
                                            GLenum format,
                                            const gl::PixelUnpackState &unpack,
                                            size_t imageSize,
                                            const uint8_t *pixels) = 0;

    virtual gl::Error copyImage(const gl::Context *context,
                                const gl::ImageIndex &index,
                                const gl::Rectangle &sourceArea,
                                GLenum internalFormat,
                                gl::Framebuffer *source)    = 0;
    virtual gl::Error copySubImage(const gl::Context *context,
                                   const gl::ImageIndex &index,
                                   const gl::Offset &destOffset,
                                   const gl::Rectangle &sourceArea,
                                   gl::Framebuffer *source) = 0;

    virtual gl::Error copyTexture(const gl::Context *context,
                                  const gl::ImageIndex &index,
                                  GLenum internalFormat,
                                  GLenum type,
                                  size_t sourceLevel,
                                  bool unpackFlipY,
                                  bool unpackPremultiplyAlpha,
                                  bool unpackUnmultiplyAlpha,
                                  const gl::Texture *source);
    virtual gl::Error copySubTexture(const gl::Context *context,
                                     const gl::ImageIndex &index,
                                     const gl::Offset &destOffset,
                                     size_t sourceLevel,
                                     const gl::Box &sourceBox,
                                     bool unpackFlipY,
                                     bool unpackPremultiplyAlpha,
                                     bool unpackUnmultiplyAlpha,
                                     const gl::Texture *source);

    virtual gl::Error copyCompressedTexture(const gl::Context *context, const gl::Texture *source);

    virtual gl::Error copy3DTexture(const gl::Context *context,
                                    gl::TextureTarget target,
                                    GLenum internalFormat,
                                    GLenum type,
                                    size_t sourceLevel,
                                    size_t destLevel,
                                    bool unpackFlipY,
                                    bool unpackPremultiplyAlpha,
                                    bool unpackUnmultiplyAlpha,
                                    const gl::Texture *source);
    virtual gl::Error copy3DSubTexture(const gl::Context *context,
                                       const gl::TextureTarget target,
                                       const gl::Offset &destOffset,
                                       size_t sourceLevel,
                                       size_t destLevel,
                                       const gl::Box &srcBox,
                                       bool unpackFlipY,
                                       bool unpackPremultiplyAlpha,
                                       bool unpackUnmultiplyAlpha,
                                       const gl::Texture *source);

    virtual gl::Error setStorage(const gl::Context *context,
                                 gl::TextureType type,
                                 size_t levels,
                                 GLenum internalFormat,
                                 const gl::Extents &size) = 0;

    virtual gl::Error setStorageMultisample(const gl::Context *context,
                                            gl::TextureType type,
                                            GLsizei samples,
                                            GLint internalformat,
                                            const gl::Extents &size,
                                            bool fixedSampleLocations) = 0;

    virtual gl::Error setEGLImageTarget(const gl::Context *context,
                                        gl::TextureType type,
                                        egl::Image *image) = 0;

    virtual gl::Error setImageExternal(const gl::Context *context,
                                       gl::TextureType type,
                                       egl::Stream *stream,
                                       const egl::Stream::GLTextureDescription &desc) = 0;

    virtual gl::Error generateMipmap(const gl::Context *context) = 0;

    virtual gl::Error setBaseLevel(const gl::Context *context, GLuint baseLevel) = 0;

    virtual gl::Error bindTexImage(const gl::Context *context, egl::Surface *surface) = 0;
    virtual gl::Error releaseTexImage(const gl::Context *context) = 0;

    virtual angle::Result syncState(const gl::Context *context,
                                    const gl::Texture::DirtyBits &dirtyBits) = 0;

  protected:
    const gl::TextureState &mState;
};

}

#endif // LIBANGLE_RENDERER_TEXTUREIMPL_H_
