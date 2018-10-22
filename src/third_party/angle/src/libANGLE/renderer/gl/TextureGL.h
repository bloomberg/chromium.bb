//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureGL.h: Defines the class interface for TextureGL.

#ifndef LIBANGLE_RENDERER_GL_TEXTUREGL_H_
#define LIBANGLE_RENDERER_GL_TEXTUREGL_H_

#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/TextureImpl.h"
#include "libANGLE/Texture.h"

namespace rx
{

class BlitGL;
class FunctionsGL;
class StateManagerGL;
struct WorkaroundsGL;

struct LUMAWorkaroundGL
{
    bool enabled;
    GLenum workaroundFormat;

    LUMAWorkaroundGL();
    LUMAWorkaroundGL(bool enabled, GLenum workaroundFormat);
};

// Structure containing information about format and workarounds for each mip level of the
// TextureGL.
struct LevelInfoGL
{
    // Format of the data used in this mip level.
    GLenum sourceFormat;

    // Internal format used for the native call to define this texture
    GLenum nativeInternalFormat;

    // If this mip level requires sampler-state re-writing so that only a red channel is exposed.
    bool depthStencilWorkaround;

    // Information about luminance alpha texture workarounds in the core profile.
    LUMAWorkaroundGL lumaWorkaround;

    LevelInfoGL();
    LevelInfoGL(GLenum sourceFormat,
                GLenum nativeInternalFormat,
                bool depthStencilWorkaround,
                const LUMAWorkaroundGL &lumaWorkaround);
};

class TextureGL : public TextureImpl
{
  public:
    TextureGL(const gl::TextureState &state, GLuint id);
    ~TextureGL() override;

    gl::Error onDestroy(const gl::Context *context) override;

    gl::Error setImage(const gl::Context *context,
                       const gl::ImageIndex &index,
                       GLenum internalFormat,
                       const gl::Extents &size,
                       GLenum format,
                       GLenum type,
                       const gl::PixelUnpackState &unpack,
                       const uint8_t *pixels) override;
    gl::Error setSubImage(const gl::Context *context,
                          const gl::ImageIndex &index,
                          const gl::Box &area,
                          GLenum format,
                          GLenum type,
                          const gl::PixelUnpackState &unpack,
                          gl::Buffer *unpackBuffer,
                          const uint8_t *pixels) override;

    gl::Error setCompressedImage(const gl::Context *context,
                                 const gl::ImageIndex &index,
                                 GLenum internalFormat,
                                 const gl::Extents &size,
                                 const gl::PixelUnpackState &unpack,
                                 size_t imageSize,
                                 const uint8_t *pixels) override;
    gl::Error setCompressedSubImage(const gl::Context *context,
                                    const gl::ImageIndex &index,
                                    const gl::Box &area,
                                    GLenum format,
                                    const gl::PixelUnpackState &unpack,
                                    size_t imageSize,
                                    const uint8_t *pixels) override;

    gl::Error copyImage(const gl::Context *context,
                        const gl::ImageIndex &index,
                        const gl::Rectangle &sourceArea,
                        GLenum internalFormat,
                        gl::Framebuffer *source) override;
    gl::Error copySubImage(const gl::Context *context,
                           const gl::ImageIndex &index,
                           const gl::Offset &destOffset,
                           const gl::Rectangle &sourceArea,
                           gl::Framebuffer *source) override;

    gl::Error copyTexture(const gl::Context *context,
                          const gl::ImageIndex &index,
                          GLenum internalFormat,
                          GLenum type,
                          size_t sourceLevel,
                          bool unpackFlipY,
                          bool unpackPremultiplyAlpha,
                          bool unpackUnmultiplyAlpha,
                          const gl::Texture *source) override;
    gl::Error copySubTexture(const gl::Context *context,
                             const gl::ImageIndex &index,
                             const gl::Offset &destOffset,
                             size_t sourceLevel,
                             const gl::Rectangle &sourceArea,
                             bool unpackFlipY,
                             bool unpackPremultiplyAlpha,
                             bool unpackUnmultiplyAlpha,
                             const gl::Texture *source) override;
    gl::Error copySubTextureHelper(const gl::Context *context,
                                   gl::TextureTarget target,
                                   size_t level,
                                   const gl::Offset &destOffset,
                                   size_t sourceLevel,
                                   const gl::Rectangle &sourceArea,
                                   const gl::InternalFormat &destFormat,
                                   bool unpackFlipY,
                                   bool unpackPremultiplyAlpha,
                                   bool unpackUnmultiplyAlpha,
                                   const gl::Texture *source);

    gl::Error setStorage(const gl::Context *context,
                         gl::TextureType type,
                         size_t levels,
                         GLenum internalFormat,
                         const gl::Extents &size) override;

    gl::Error setStorageMultisample(const gl::Context *context,
                                    gl::TextureType type,
                                    GLsizei samples,
                                    GLint internalFormat,
                                    const gl::Extents &size,
                                    bool fixedSampleLocations) override;

    gl::Error setImageExternal(const gl::Context *context,
                               gl::TextureType type,
                               egl::Stream *stream,
                               const egl::Stream::GLTextureDescription &desc) override;

    gl::Error generateMipmap(const gl::Context *context) override;

    gl::Error bindTexImage(const gl::Context *context, egl::Surface *surface) override;
    gl::Error releaseTexImage(const gl::Context *context) override;

    gl::Error setEGLImageTarget(const gl::Context *context,
                                gl::TextureType type,
                                egl::Image *image) override;

    GLuint getTextureID() const { return mTextureID; }

    gl::TextureType getType() const;

    gl::Error syncState(const gl::Context *context,
                        const gl::Texture::DirtyBits &dirtyBits) override;
    bool hasAnyDirtyBit() const;

    gl::Error setBaseLevel(const gl::Context *context, GLuint baseLevel) override;

    gl::Error initializeContents(const gl::Context *context,
                                 const gl::ImageIndex &imageIndex) override;

    void setMinFilter(const gl::Context *context, GLenum filter);
    void setMagFilter(const gl::Context *context, GLenum filter);

    void setSwizzle(const gl::Context *context, GLint swizzle[4]);

    GLenum getNativeInternalFormat(const gl::ImageIndex &index) const;

  private:
    void setImageHelper(const gl::Context *context,
                        gl::TextureTarget target,
                        size_t level,
                        GLenum internalFormat,
                        const gl::Extents &size,
                        GLenum format,
                        GLenum type,
                        const uint8_t *pixels);
    // This changes the current pixel unpack state that will have to be reapplied.
    void reserveTexImageToBeFilled(const gl::Context *context,
                                   gl::TextureTarget target,
                                   size_t level,
                                   GLenum internalFormat,
                                   const gl::Extents &size,
                                   GLenum format,
                                   GLenum type);
    gl::Error setSubImageRowByRowWorkaround(const gl::Context *context,
                                            gl::TextureTarget target,
                                            size_t level,
                                            const gl::Box &area,
                                            GLenum format,
                                            GLenum type,
                                            const gl::PixelUnpackState &unpack,
                                            const gl::Buffer *unpackBuffer,
                                            const uint8_t *pixels);

    gl::Error setSubImagePaddingWorkaround(const gl::Context *context,
                                           gl::TextureTarget target,
                                           size_t level,
                                           const gl::Box &area,
                                           GLenum format,
                                           GLenum type,
                                           const gl::PixelUnpackState &unpack,
                                           const gl::Buffer *unpackBuffer,
                                           const uint8_t *pixels);

    void syncTextureStateSwizzle(const FunctionsGL *functions,
                                 GLenum name,
                                 GLenum value,
                                 GLenum *outValue);

    void setLevelInfo(gl::TextureTarget target,
                      size_t level,
                      size_t levelCount,
                      const LevelInfoGL &levelInfo);
    void setLevelInfo(gl::TextureType type,
                      size_t level,
                      size_t levelCount,
                      const LevelInfoGL &levelInfo);
    const LevelInfoGL &getLevelInfo(gl::TextureTarget target, size_t level) const;
    const LevelInfoGL &getBaseLevelInfo() const;

    std::vector<LevelInfoGL> mLevelInfo;
    gl::Texture::DirtyBits mLocalDirtyBits;

    gl::SwizzleState mAppliedSwizzle;
    gl::SamplerState mAppliedSampler;
    GLuint mAppliedBaseLevel;
    GLuint mAppliedMaxLevel;

    GLuint mTextureID;
};

}

#endif // LIBANGLE_RENDERER_GL_TEXTUREGL_H_
