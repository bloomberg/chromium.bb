// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/compositor.h"

#include <GL/gl.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_restrictions.h"
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

  GLuint program();
  GLuint a_pos_loc();
  GLuint a_tex_loc();
  GLuint u_tex_loc();
  GLuint u_mat_loc();

 private:
  // Overridden from Compositor.
  virtual Texture* CreateTexture() OVERRIDE;
  virtual void NotifyStart() OVERRIDE;
  virtual void NotifyEnd() OVERRIDE;
  virtual void Blur(const gfx::Rect& bounds) OVERRIDE;

  // Specific to CompositorGL.
  bool InitShaders();

  // The GL context used for compositing.
  scoped_refptr<gfx::GLSurface> gl_surface_;
  scoped_refptr<gfx::GLContext> gl_context_;
  gfx::Size size_;

  // Shader program, attributes and uniforms.
  // TODO(wjmaclean): Make these static so they ca be shared in a single
  // context.
  GLuint program_;
  GLuint a_pos_loc_;
  GLuint a_tex_loc_;
  GLuint u_tex_loc_;
  GLuint u_mat_loc_;

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

  DCHECK(compositor_->program());
  glUseProgram(compositor_->program());

  glActiveTexture(GL_TEXTURE0);
  glUniform1i(compositor_->u_tex_loc(), 0);
  glBindTexture(GL_TEXTURE_2D, texture_id_);

  GLfloat m[16];
  const SkMatrix& matrix = t.matrix();

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

  const GLfloat vertices[] = { -1., -1., +0., +0., +1.,
                               +1., -1., +0., +1., +1.,
                               +1., +1., +0., +1., +0.,
                               -1., +1., +0., +0., +0. };

  glVertexAttribPointer(compositor_->a_pos_loc(), 3, GL_FLOAT,
                        GL_FALSE, 5 * sizeof(GLfloat), vertices);
  glVertexAttribPointer(compositor_->a_tex_loc(), 2, GL_FLOAT,
                        GL_FALSE, 5 * sizeof(GLfloat), &vertices[3]);
  glEnableVertexAttribArray(compositor_->a_pos_loc());
  glEnableVertexAttribArray(compositor_->a_tex_loc());

  glUniformMatrix4fv(compositor_->u_mat_loc(), 1, GL_FALSE, m);

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

CompositorGL::CompositorGL(gfx::AcceleratedWidget widget)
    : started_(false) {
  gl_surface_ = gfx::GLSurface::CreateViewGLSurface(widget);
  gl_context_ = gfx::GLContext::CreateGLContext(NULL, gl_surface_.get());
  gl_context_->MakeCurrent(gl_surface_.get());
  if (!InitShaders())
    LOG(ERROR) << "Unable to initialize shaders (context = "
               << static_cast<void*>(gl_context_.get()) << ")" ;
  glColorMask(true, true, true, true);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

CompositorGL::~CompositorGL() {
}

void CompositorGL::MakeCurrent() {
  gl_context_->MakeCurrent(gl_surface_.get());
}

gfx::Size CompositorGL::GetSize() {
  return gl_surface_->GetSize();
}

GLuint CompositorGL::program() {
  return program_;
}

GLuint CompositorGL::a_pos_loc() {
  return a_pos_loc_;
}

GLuint CompositorGL::a_tex_loc() {
  return a_tex_loc_;
}

GLuint CompositorGL::u_tex_loc() {
  return u_tex_loc_;
}

GLuint CompositorGL::u_mat_loc() {
  return u_mat_loc_;
}

Texture* CompositorGL::CreateTexture() {
  Texture* texture = new TextureGL(this);
  return texture;
}

void CompositorGL::NotifyStart() {
  started_ = true;
  gl_context_->MakeCurrent(gl_surface_.get());
  glViewport(0, 0,
             gl_surface_->GetSize().width(), gl_surface_->GetSize().height());

#if defined(DEBUG)
  // Clear to 'psychedelic' purple to make it easy to spot un-rendered regions.
  glClearColor(223.0 / 255, 0, 1, 1);
#else
  // Clear to transparent black.
  glClearColor(0, 0, 0, 0);
#endif
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

void CompositorGL::Blur(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

namespace {

GLuint CompileShader(GLenum type, const GLchar* source) {
  GLuint shader = glCreateShader(type);
  if (!shader)
    return 0;

  glShaderSource(shader, 1, &source, 0);
  glCompileShader(shader);

  GLint compiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    GLint info_len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);

    if (info_len > 0) {
      char* info_log = reinterpret_cast<char*> (
          malloc(sizeof(info_log[0]) * info_len));
      glGetShaderInfoLog(shader, info_len, NULL, info_log);

      LOG(ERROR) << "Compile error: " <<  info_log;
      free(info_log);
      return 0;
    }
  }
  return shader;
}

} // namespace (anonymous)

bool CompositorGL::InitShaders() {
  const GLchar* vertex_shader_source =
        "attribute vec4 a_position;"
        "attribute vec2 a_texCoord;"
        "uniform mat4 u_matViewProjection;"
        "varying vec2 v_texCoord;"
        "void main()"
        "{"
        "  gl_Position = u_matViewProjection * a_position;"
        "  v_texCoord = a_texCoord;"
        "}";
  GLuint vertex_shader = CompileShader(GL_VERTEX_SHADER, vertex_shader_source);
  if (!vertex_shader)
    return false;

  const GLchar* frag_shader_source =
        "#ifdef GL_ES\n"
        "precision mediump float;\n"
        "#endif\n"
        "uniform sampler2D u_tex;"
        "varying vec2 v_texCoord;"
        "void main()"
        "{"
        "  gl_FragColor = texture2D(u_tex, v_texCoord).zyxw;"
        "}";
  GLuint frag_shader = CompileShader(GL_FRAGMENT_SHADER, frag_shader_source);
  if (!frag_shader)
    return false;

  program_ = glCreateProgram();
  glAttachShader(program_, vertex_shader);
  glAttachShader(program_, frag_shader);
  glLinkProgram(program_);
  if (glGetError() != GL_NO_ERROR)
    return false;

  // Store locations of program inputs.
  a_pos_loc_ = glGetAttribLocation(program_, "a_position");
  a_tex_loc_ = glGetAttribLocation(program_, "a_texCoord");
  u_tex_loc_ = glGetUniformLocation(program_, "u_tex");
  u_mat_loc_ = glGetUniformLocation(program_, "u_matViewProjection");

  return true;
}

} // namespace

// static
Compositor* Compositor::Create(gfx::AcceleratedWidget widget) {
  // The following line of code exists soley to disable IO restrictions
  // on this thread long enough to perform the GL bindings.
  // TODO(wjmaclean) Remove this when GL initialisation cleaned up.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  if (gfx::GLSurface::InitializeOneOff() &&
      gfx::GetGLImplementation() != gfx::kGLImplementationNone)
    return new glHidden::CompositorGL(widget);
  return NULL;
}

}  // namespace ui
