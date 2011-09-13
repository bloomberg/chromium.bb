// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/compositor_gl.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_restrictions.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_surface.h"

// Wraps a simple GL program for drawing textures to the screen.
// Need the declaration before the subclasses in the anonymous namespace below.
class ui::TextureProgramGL {
 public:
  TextureProgramGL();
  virtual ~TextureProgramGL() {}

  // Returns false if it was unable to initialize properly.
  //
  // Host GL context must be current when this is called.
  virtual bool Initialize() = 0;

  // Make the program active in the current GL context.
  void Use() const { glUseProgram(program_); }

  // Location of vertex position attribute in vertex shader.
  GLuint a_pos_loc() const { return a_pos_loc_; }

  // Location of texture co-ordinate attribute in vertex shader.
  GLuint a_tex_loc() const { return a_tex_loc_; }

  // Location of transformation matrix uniform in vertex shader.
  GLuint u_mat_loc() const { return u_mat_loc_; }

  // Location of texture unit uniform that we texture map from
  // in the fragment shader.
  GLuint u_tex_loc() const { return u_tex_loc_; }

 protected:
  // Only the fragment shaders differ. This handles the initialization
  // of all the other fields.
  bool InitializeCommon();

  GLuint frag_shader_;

 private:
  GLuint program_;
  GLuint vertex_shader_;

  GLuint a_pos_loc_;
  GLuint a_tex_loc_;
  GLuint u_tex_loc_;
  GLuint u_mat_loc_;
};

namespace {

class TextureProgramNoSwizzleGL : public ui::TextureProgramGL {
 public:
  TextureProgramNoSwizzleGL() {}
  virtual bool Initialize();
 private:
  DISALLOW_COPY_AND_ASSIGN(TextureProgramNoSwizzleGL);
};

class TextureProgramSwizzleGL : public ui::TextureProgramGL {
 public:
  TextureProgramSwizzleGL() {}
  virtual bool Initialize();
 private:
  DISALLOW_COPY_AND_ASSIGN(TextureProgramSwizzleGL);
};

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
      scoped_array<char> info_log(new char[info_len]);
      glGetShaderInfoLog(shader, info_len, NULL, info_log.get());
      LOG(ERROR) << "Compile error: " <<  info_log.get();
      return 0;
    }
  }
  return shader;
}

bool TextureProgramNoSwizzleGL::Initialize() {
  const GLchar* frag_shader_source =
      "#ifdef GL_ES\n"
      "precision mediump float;\n"
      "#endif\n"
      "uniform sampler2D u_tex;"
      "varying vec2 v_texCoord;"
      "void main()"
      "{"
      "  gl_FragColor = texture2D(u_tex, v_texCoord);"
      "}";

  frag_shader_ = CompileShader(GL_FRAGMENT_SHADER, frag_shader_source);
  if (!frag_shader_)
    return false;

  return InitializeCommon();
}

bool TextureProgramSwizzleGL::Initialize() {
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

  frag_shader_ = CompileShader(GL_FRAGMENT_SHADER, frag_shader_source);
  if (!frag_shader_)
    return false;

  return InitializeCommon();
}

}  // namespace

namespace ui {

TextureProgramGL::TextureProgramGL()
    : program_(0),
      a_pos_loc_(0),
      a_tex_loc_(0),
      u_tex_loc_(0),
      u_mat_loc_(0) {
}

bool TextureProgramGL::InitializeCommon() {
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

  vertex_shader_ = CompileShader(GL_VERTEX_SHADER, vertex_shader_source);
  if (!vertex_shader_)
    return false;

  program_ = glCreateProgram();
  glAttachShader(program_, vertex_shader_);
  glAttachShader(program_, frag_shader_);
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

SharedResources::SharedResources() : initialized_(false) {
}


SharedResources::~SharedResources() {
}

// static
SharedResources* SharedResources::GetInstance() {
  // We use LeakySingletonTraits so that we don't race with
  // the tear down of the gl_bindings.
  SharedResources* instance = Singleton<SharedResources,
      LeakySingletonTraits<SharedResources> >::get();
  if (instance->Initialize()) {
    return instance;
  } else {
    instance->Destroy();
    return NULL;
  }
}

bool SharedResources::Initialize() {
  if (initialized_)
    return true;

  {
    // The following line of code exists soley to disable IO restrictions
    // on this thread long enough to perform the GL bindings.
    // TODO(wjmaclean) Remove this when GL initialisation cleaned up.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (!gfx::GLSurface::InitializeOneOff() ||
        gfx::GetGLImplementation() == gfx::kGLImplementationNone) {
      LOG(ERROR) << "Could not load the GL bindings";
      return false;
    }
  }

  surface_ = gfx::GLSurface::CreateOffscreenGLSurface(false, gfx::Size(1, 1));
  if (!surface_.get()) {
    LOG(ERROR) << "Unable to create offscreen GL surface.";
    return false;
  }

  context_ = gfx::GLContext::CreateGLContext(NULL, surface_.get());
  if (!context_.get()) {
    LOG(ERROR) << "Unable to create GL context.";
    return false;
  }

  program_no_swizzle_.reset();
  program_swizzle_.reset();

  context_->MakeCurrent(surface_.get());

  scoped_ptr<ui::TextureProgramGL> temp_program_no_swizzle(
      new TextureProgramNoSwizzleGL());
  if (!temp_program_no_swizzle->Initialize()) {
    LOG(ERROR) << "Unable to initialize shader.";
    return false;
  }

  scoped_ptr<ui::TextureProgramGL> temp_program_swizzle(
      new TextureProgramSwizzleGL());
  if (!temp_program_swizzle->Initialize()) {
    LOG(ERROR) << "Unable to initialize shader.";
    return false;
  }

  program_no_swizzle_.swap(temp_program_no_swizzle);
  program_swizzle_.swap(temp_program_swizzle);

  initialized_ = true;
  return true;
}

void SharedResources::Destroy() {
  program_swizzle_.reset();
  program_no_swizzle_.reset();

  context_ = NULL;
  surface_ = NULL;

  initialized_ = false;
}

bool SharedResources::MakeSharedContextCurrent() {
  if (!initialized_)
    return false;
  else
    return context_->MakeCurrent(surface_.get());
}

scoped_refptr<gfx::GLContext> SharedResources::CreateContext(
    gfx::GLSurface* surface) {
  if (initialized_)
    return gfx::GLContext::CreateGLContext(context_->share_group(), surface);
  else
    return NULL;
}

TextureGL::TextureGL() : texture_id_(0) {
}

TextureGL::TextureGL(const gfx::Size& size) : texture_id_(0), size_(size) {
}

TextureGL::~TextureGL() {
  if (texture_id_) {
    SharedResources* instance = SharedResources::GetInstance();
    DCHECK(instance);
    instance->MakeSharedContextCurrent();
    glDeleteTextures(1, &texture_id_);
  }
}

void TextureGL::SetCanvas(const SkCanvas& canvas,
                          const gfx::Point& origin,
                          const gfx::Size& overall_size) {
  const SkBitmap& bitmap = canvas.getDevice()->accessBitmap(false);
  // Verify bitmap pixels are contiguous.
  DCHECK_EQ(bitmap.rowBytes(),
            SkBitmap::ComputeRowBytes(bitmap.config(), bitmap.width()));
  SkAutoLockPixels lock(bitmap);
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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

void TextureGL::Draw(const ui::TextureDrawParams& params,
                     const gfx::Rect& clip_bounds_in_texture) {
  SharedResources* instance = SharedResources::GetInstance();
  DCHECK(instance);
  DrawInternal(*instance->program_swizzle(),
               params,
               clip_bounds_in_texture);
}

void TextureGL::DrawInternal(const ui::TextureProgramGL& program,
                             const ui::TextureDrawParams& params,
                             const gfx::Rect& clip_bounds_in_texture) {
  // Clip clip_bounds_in_texture to size of texture.
  gfx::Rect clip_bounds = clip_bounds_in_texture.Intersect(
      gfx::Rect(gfx::Point(0, 0), size_));

  // Verify that compositor_size has been set.
  DCHECK(params.compositor_size != gfx::Size(0,0));

  if (params.blend)
    glEnable(GL_BLEND);
  else
    glDisable(GL_BLEND);

  program.Use();

  glActiveTexture(GL_TEXTURE0);
  glUniform1i(program.u_tex_loc(), 0);
  glBindTexture(GL_TEXTURE_2D, texture_id_);

  ui::Transform t;
  t.ConcatTranslate(1, 1);
  t.ConcatScale(size_.width()/2.0f, size_.height()/2.0f);
  t.ConcatTranslate(0, -size_.height());
  t.ConcatScale(1, -1);

  t.ConcatTransform(params.transform);  // Add view transform.

  t.ConcatTranslate(0, -params.compositor_size.height());
  t.ConcatScale(1, -1);
  t.ConcatTranslate(-params.compositor_size.width() / 2.0f,
                    -params.compositor_size.height() / 2.0f);
  t.ConcatScale(2.0f / params.compositor_size.width(),
                2.0f / params.compositor_size.height());

  GLfloat m[16];
  t.matrix().asColMajorf(m);

  SkRect texture_rect = SkRect::MakeXYWH(
      clip_bounds.x(),
      clip_bounds.y(),
      clip_bounds.width(),
      clip_bounds.height());

  ui::Transform texture_rect_transform;
  texture_rect_transform.ConcatScale(1.0f / size_.width(),
                                     1.0f / size_.height());
  SkMatrix texture_transform_matrix = texture_rect_transform.matrix();
  texture_transform_matrix.mapRect(&texture_rect);

  SkRect clip_rect = SkRect::MakeXYWH(
      clip_bounds.x(),
      clip_bounds.y(),
      clip_bounds.width(),
      clip_bounds.height());

  ui::Transform clip_rect_transform;
  clip_rect_transform.ConcatScale(2.0f / size_.width(),
                                  2.0f / size_.height());
  clip_rect_transform.ConcatScale(1, -1);
  clip_rect_transform.ConcatTranslate(-1.0f, 1.0f);
  SkMatrix clip_transform_matrix = clip_rect_transform.matrix();
  clip_transform_matrix.mapRect(&clip_rect);

  GLfloat clip_vertices[] = { clip_rect.left(), clip_rect.top(), +0.,
                              clip_rect.right(), clip_rect.top(), +0.,
                              clip_rect.right(), clip_rect.bottom(), +0.,
                              clip_rect.left(), clip_rect.bottom(), +0.};

  GLfloat texture_vertices[]  = { texture_rect.left(), texture_rect.bottom(),
                                  texture_rect.right(), texture_rect.bottom(),
                                  texture_rect.right(), texture_rect.top(),
                                  texture_rect.left(), texture_rect.top()};

  glVertexAttribPointer(program.a_pos_loc(), 3, GL_FLOAT,
                        GL_FALSE, 3 * sizeof(GLfloat), clip_vertices);
  glVertexAttribPointer(program.a_tex_loc(), 2, GL_FLOAT,
                        GL_FALSE, 2 * sizeof(GLfloat), texture_vertices);
  glEnableVertexAttribArray(program.a_pos_loc());
  glEnableVertexAttribArray(program.a_tex_loc());

  glUniformMatrix4fv(program.u_mat_loc(), 1, GL_FALSE, m);

  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

CompositorGL::CompositorGL(CompositorDelegate* delegate,
                           gfx::AcceleratedWidget widget,
                           const gfx::Size& size)
    : Compositor(delegate, size),
      started_(false) {
  gl_surface_ = gfx::GLSurface::CreateViewGLSurface(false, widget);
  gl_context_ = SharedResources::GetInstance()->
      CreateContext(gl_surface_.get());
  gl_context_->MakeCurrent(gl_surface_.get());
  glColorMask(true, true, true, true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

CompositorGL::~CompositorGL() {
  gl_context_ = NULL;
}

void CompositorGL::MakeCurrent() {
  gl_context_->MakeCurrent(gl_surface_.get());
}

void CompositorGL::OnWidgetSizeChanged() {
}

Texture* CompositorGL::CreateTexture() {
  Texture* texture = new TextureGL();
  return texture;
}

void CompositorGL::OnNotifyStart(bool clear) {
  started_ = true;
  gl_context_->MakeCurrent(gl_surface_.get());
  glViewport(0, 0, size().width(), size().height());
  glColorMask(true, true, true, true);

  if (clear) {
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
  }
#if !defined(NDEBUG)
  else {
    // In debug mode, when we're not forcing a clear, clear to 'psychedelic'
    // purple to make it easy to spot un-rendered regions.
    glClearColor(223.0 / 255, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
  }
#endif
}

void CompositorGL::OnNotifyEnd() {
  DCHECK(started_);
  gl_surface_->SwapBuffers();
  started_ = false;
}

void CompositorGL::Blur(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

// static
Compositor* Compositor::Create(CompositorDelegate* owner,
                               gfx::AcceleratedWidget widget,
                               const gfx::Size& size) {
  if (SharedResources::GetInstance() == NULL)
    return NULL;
  else
    return new CompositorGL(owner, widget, size);
}

}  // namespace ui
