// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TEXTURE_DEFINITION_H_
#define GPU_COMMAND_BUFFER_SERVICE_TEXTURE_DEFINITION_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace gl {
class GLImage;
}

namespace gpu {
namespace gles2 {

class Texture;

class NativeImageBuffer : public base::RefCountedThreadSafe<NativeImageBuffer> {
 public:
  static scoped_refptr<NativeImageBuffer> Create(GLuint texture_id);

  virtual void AddClient(gl::GLImage* client) = 0;
  virtual void RemoveClient(gl::GLImage* client) = 0;
  virtual bool IsClient(gl::GLImage* client) = 0;
  virtual void BindToTexture(GLenum target) const = 0;

 protected:
  friend class base::RefCountedThreadSafe<NativeImageBuffer>;
  NativeImageBuffer() = default;
  virtual ~NativeImageBuffer() = default;

  DISALLOW_COPY_AND_ASSIGN(NativeImageBuffer);
};

// An immutable description that can be used to create a texture that shares
// the underlying image buffer(s).
class TextureDefinition {
 public:
  static void AvoidEGLTargetTextureReuse();

  TextureDefinition();
  TextureDefinition(Texture* texture,
                    unsigned int version,
                    const scoped_refptr<NativeImageBuffer>& image);
  TextureDefinition(const TextureDefinition& other);
  virtual ~TextureDefinition();

  Texture* CreateTexture() const;

  void UpdateTexture(Texture* texture) const;

  unsigned int version() const { return version_; }
  bool IsOlderThan(unsigned int version) const {
    return (version - version_) < 0x80000000;
  }
  bool Matches(const Texture* texture) const;

  scoped_refptr<NativeImageBuffer> image() const { return image_buffer_; }

 private:
  bool SafeToRenderFrom() const;
  void UpdateTextureInternal(Texture* texture) const;

  struct LevelInfo {
    LevelInfo();
    LevelInfo(GLenum target,
              GLenum internal_format,
              GLsizei width,
              GLsizei height,
              GLsizei depth,
              GLint border,
              GLenum format,
              GLenum type,
              const gfx::Rect& cleared_rect);
    LevelInfo(const LevelInfo& other);
    ~LevelInfo();

    GLenum target;
    GLenum internal_format;
    GLsizei width;
    GLsizei height;
    GLsizei depth;
    GLint border;
    GLenum format;
    GLenum type;
    gfx::Rect cleared_rect;
  };

  unsigned int version_;
  GLenum target_;
  scoped_refptr<NativeImageBuffer> image_buffer_;
  GLenum min_filter_;
  GLenum mag_filter_;
  GLenum wrap_s_;
  GLenum wrap_t_;
  GLenum usage_;
  bool immutable_;
  bool immutable_storage_;
  bool defined_;

  // Only support textures with one face and one level.
  LevelInfo level_info_;
};

}  // namespage gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TEXTURE_DEFINITION_H_
