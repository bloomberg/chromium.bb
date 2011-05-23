// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/compositor.h"

#include <GL/gl.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/gl/gl_surface_glx.h"

namespace ui {

#if defined COMPOSITOR_2
namespace glHidden {

class CompositorGL;

class TextureGL : public Texture {
 public:
  TextureGL(CompositorGL* compositor);
  virtual ~TextureGL();

  virtual void SetBitmap(const SkBitmap& bitmap,
                         const gfx::Point& origin,
                         const gfx::Size& overall_size) OVERRIDE;

  // Draws the texture.
  virtual void Draw(const ui::Transform& transform) OVERRIDE;

 private:
  unsigned int texture_id_;
  gfx::Size size_;
  CompositorGL* compositor_;
  DISALLOW_COPY_AND_ASSIGN(TextureGL);
};

class CompositorGL : public Compositor {
 public:
  explicit CompositorGL(gfx::AcceleratedWidget widget);
  virtual ~CompositorGL();

  void MakeCurrent();
  gfx::Size GetSize();

 private:
  // Overridden from Compositor.
  virtual Texture* CreateTexture() OVERRIDE;
  virtual void NotifyStart() OVERRIDE;
  virtual void NotifyEnd() OVERRIDE;

  // The GL context used for compositing.
  scoped_ptr<gfx::GLSurface> gl_surface_;
  scoped_ptr<gfx::GLContext> gl_context_;
  gfx::Size size_;

  // Keep track of whether compositing has started or not.
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(CompositorGL);
};

TextureGL::TextureGL(CompositorGL* compositor) : texture_id_(0),
                                                 compositor_(compositor) {
}

TextureGL::~TextureGL() {
  if (texture_id_) {
    compositor_->MakeCurrent();
    glDeleteTextures(1, &texture_id_);
  }
}

void TextureGL::SetBitmap(const SkBitmap& bitmap,
                          const gfx::Point& origin,
                          const gfx::Size& overall_size) {
  // Verify bitmap pixels are contiguous.
  DCHECK_EQ(bitmap.rowBytes(),
            SkBitmap::ComputeRowBytes(bitmap.config(), bitmap.width()));
  SkAutoLockPixels lock (bitmap);
  void* pixels = bitmap.getPixels();

  if (!texture_id_) {
    // Texture needs to be created. We assume the first call is for
    // a full-sized canvas.
    size_ = overall_size;

    glGenTextures(1, &texture_id_);
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 size_.width(), size_.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Map ARGB -> RGBA, remembering the we are transferring using
    // GL_UNSIGNED_BYTE transfer, so endian-ness comes into play.
    // TODO(wjmaclean) Verify this will work with eglImage.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R_EXT, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B_EXT, GL_RED);

  } else if (size_ != overall_size) {  // Size has changed.
    size_ = overall_size;
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 size_.width(), size_.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  } else {  // Uploading partial texture.
    glBindTexture(GL_TEXTURE_2D, texture_id_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, origin.x(), origin.y(),
                    bitmap.width(), bitmap.height(),
                    GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  }
}

void TextureGL::Draw(const ui::Transform& transform) {
  gfx::Size window_size = compositor_->GetSize();

  ui::Transform t;
  t.ConcatTranslate(1,1);
  t.ConcatScale(size_.width()/2.0f, size_.height()/2.0f);
  t.ConcatTranslate(0, -size_.height());
  t.ConcatScale(1, -1);

  t.ConcatTransform(transform); // Add view transform.

  t.ConcatTranslate(0, -window_size.height());
  t.ConcatScale(1, -1);
  t.ConcatTranslate(-window_size.width()/2.0f, -window_size.height()/2.0f);
  t.ConcatScale(2.0f/window_size.width(), 2.0f/window_size.height());

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, texture_id_);

  if (!t.matrix().isIdentity()) {
    float m[16];
    const SkMatrix& matrix = t.matrix(); // *mat;

    // Convert 3x3 view transform matrix (row major) into 4x4 GL matrix (column
    // major). Assume 2-D rotations/translations restricted to XY plane.

    m[ 0] = matrix[0];
    m[ 1] = matrix[3];
    m[ 2] = 0;
    m[ 3] = matrix[6];

    m[ 4] = matrix[1];
    m[ 5] = matrix[4];
    m[ 6] = 0;
    m[ 7] = matrix[7];

    m[ 8] = 0;
    m[ 9] = 0;
    m[10] = 1;
    m[11] = 0;

    m[12] = matrix[2];
    m[13] = matrix[5];
    m[14] = 0;
    m[15] = matrix[8];

    glLoadMatrixf(m);
  } else {
    glLoadIdentity();
  }

  glBegin(GL_QUADS);
  glColor4f(1.0, 1.0, 1.0, 1.0);
  glTexCoord2d(0.0, 1.0); glVertex2d(-1, -1);
  glTexCoord2d(1.0, 1.0); glVertex2d( 1, -1);
  glTexCoord2d(1.0, 0.0); glVertex2d( 1,  1);
  glTexCoord2d(0.0, 0.0); glVertex2d(-1,  1);
  glEnd();

  if (!t.matrix().isIdentity()) {
    glLoadIdentity();
  }

  glDisable(GL_TEXTURE_2D);
}

CompositorGL::CompositorGL(gfx::AcceleratedWidget widget)
    : started_(false) {
  gl_surface_.reset(gfx::GLSurface::CreateViewGLSurface(widget));
  gl_context_.reset(gfx::GLContext::CreateGLContext(NULL, gl_surface.get())),
  gl_context_->MakeCurrent();
  glColorMask(true, true, true, true);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

CompositorGL::~CompositorGL() {
}

void CompositorGL::MakeCurrent() {
  gl_context_->MakeCurrent();
}

gfx::Size CompositorGL::GetSize() {
  return gl_context_->GetSize();
}

Texture* CompositorGL::CreateTexture() {
  Texture* texture = new TextureGL(this);
  return texture;
}

void CompositorGL::NotifyStart() {
  started_ = true;
  gl_context_->MakeCurrent(gl_surface_.get());
  glViewport(0, 0,
             gl_context_->GetSize().width(), gl_context_->GetSize().height());

  // Clear to 'psychedelic' purple to make it easy to spot un-rendered regions.
  glClearColor(223.0 / 255, 0, 1, 1);
  glColorMask(true, true, true, true);
  glClear(GL_COLOR_BUFFER_BIT);
  // Disable alpha writes, since we're using blending anyways.
  glColorMask(true, true, true, false);
}

void CompositorGL::NotifyEnd() {
  DCHECK(started_);
  gl_surface_->SwapBuffers();
  started_ = false;
}

} // namespace

// static
Compositor* Compositor::Create(gfx::AcceleratedWidget widget) {
  // TODO(backer) Remove this when GL thread patch lands. Should be init'd
  // by the new GPU thread.
  // http://codereview.chromium.org/6677055/
  gfx::InitializeGLBindings(gfx::kGLImplementationDesktopGL);
  gfx::GLSurfaceGLX::InitializeOneOff();

 if (gfx::GetGLImplementation() != gfx::kGLImplementationNone)
   return new glHidden::CompositorGL(widget);
  return NULL;
}
#else
class CompositorGL : public Compositor {
 public:
  explicit CompositorGL(gfx::AcceleratedWidget widget);

 private:
  // Overridden from Compositor.
  void NotifyStart() OVERRIDE;
  void NotifyEnd() OVERRIDE;
  void DrawTextureWithTransform(TextureID txt,
                                const ui::Transform& transform) OVERRIDE;
  void SaveTransform() OVERRIDE;
  void RestoreTransform() OVERRIDE;

  // The GL context used for compositing.
  scoped_ptr<gfx::GLSurface> gl_surface_;
  scoped_ptr<gfx::GLContext> gl_context_;

  // Keep track of whether compositing has started or not.
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(CompositorGL);
};

CompositorGL::CompositorGL(gfx::AcceleratedWidget widget)
    : started_(false) {
  gl_surface_.reset(gfx::GLSurface::CreateViewGLSurface(widget));
  gl_context_.reset(gfx::GLContext::CreateGLContext(NULL, gl_surface_.get()));
}

void CompositorGL::NotifyStart() {
  started_ = true;
  gl_context_->MakeCurrent(gl_surface_.get());
}

void CompositorGL::NotifyEnd() {
  DCHECK(started_);
  gl_surface_->SwapBuffers();
  started_ = false;
}

void CompositorGL::DrawTextureWithTransform(TextureID txt,
                                            const ui::Transform& transform) {
  DCHECK(started_);

  // TODO(wjmaclean):
  NOTIMPLEMENTED();
}

void CompositorGL::SaveTransform() {
  // TODO(sadrul):
  NOTIMPLEMENTED();
}

void CompositorGL::RestoreTransform() {
  // TODO(sadrul):
  NOTIMPLEMENTED();
}

// static
Compositor* Compositor::Create(gfx::AcceleratedWidget widget) {
  if (gfx::GetGLImplementation() != gfx::kGLImplementationNone)
    return new CompositorGL(widget);
  return NULL;
}
#endif

}  // namespace ui
