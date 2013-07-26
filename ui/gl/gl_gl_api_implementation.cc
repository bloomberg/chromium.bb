// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_gl_api_implementation.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_state_restorer.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"

namespace gfx {

// The GL Api being used. This could be g_real_gl or gl_trace_gl
static GLApi* g_gl;
// A GL Api that calls directly into the driver.
static RealGLApi* g_real_gl;
// A GL Api that calls TRACE and then calls another GL api.
static TraceGLApi* g_trace_gl;

namespace {

static inline GLenum GetTexInternalFormat(GLenum internal_format) {
  if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2) {
    if (internal_format == GL_BGRA_EXT || internal_format == GL_BGRA8_EXT)
      return GL_RGBA8;
  }
  return internal_format;
}

// TODO(epenner): Could the above function be merged into this and removed?
static inline GLenum GetTexInternalFormat(GLenum internal_format,
                                          GLenum format,
                                          GLenum type) {
  GLenum gl_internal_format = GetTexInternalFormat(internal_format);

  if (gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2)
    return gl_internal_format;

  if (type == GL_FLOAT) {
    switch (format) {
      case GL_RGBA:
        gl_internal_format = GL_RGBA32F_ARB;
        break;
      case GL_RGB:
        gl_internal_format = GL_RGB32F_ARB;
        break;
      case GL_LUMINANCE_ALPHA:
        gl_internal_format = GL_LUMINANCE_ALPHA32F_ARB;
        break;
      case GL_LUMINANCE:
        gl_internal_format = GL_LUMINANCE32F_ARB;
        break;
      case GL_ALPHA:
        gl_internal_format = GL_ALPHA32F_ARB;
        break;
      default:
        NOTREACHED();
        break;
    }
  } else if (type == GL_HALF_FLOAT_OES) {
    switch (format) {
      case GL_RGBA:
        gl_internal_format = GL_RGBA16F_ARB;
        break;
      case GL_RGB:
        gl_internal_format = GL_RGB16F_ARB;
        break;
      case GL_LUMINANCE_ALPHA:
        gl_internal_format = GL_LUMINANCE_ALPHA16F_ARB;
        break;
      case GL_LUMINANCE:
        gl_internal_format = GL_LUMINANCE16F_ARB;
        break;
      case GL_ALPHA:
        gl_internal_format = GL_ALPHA16F_ARB;
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  return gl_internal_format;
}

static inline GLenum GetTexType(GLenum type) {
   if (gfx::GetGLImplementation() != gfx::kGLImplementationEGLGLES2) {
     if (type == GL_HALF_FLOAT_OES)
       return GL_HALF_FLOAT_ARB;
   }
   return type;
}

static void GL_BINDING_CALL CustomTexImage2D(
    GLenum target, GLint level, GLint internalformat,
    GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  GLenum gl_internal_format = GetTexInternalFormat(
      internalformat, format, type);
  GLenum gl_type = GetTexType(type);
  return g_driver_gl.orig_fn.glTexImage2DFn(
      target, level, gl_internal_format, width, height, border, format, gl_type,
      pixels);
}

static void GL_BINDING_CALL CustomTexSubImage2D(
      GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
      GLsizei height, GLenum format, GLenum type, const void* pixels) {
  GLenum gl_type = GetTexType(type);
  return g_driver_gl.orig_fn.glTexSubImage2DFn(
      target, level, xoffset, yoffset, width, height, format, gl_type, pixels);
}

static void GL_BINDING_CALL CustomTexStorage2DEXT(
    GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
    GLsizei height) {
  GLenum gl_internal_format = GetTexInternalFormat(internalformat);
  return g_driver_gl.orig_fn.glTexStorage2DEXTFn(
      target, levels, gl_internal_format, width, height);
}

}  // anonymous namespace

void DriverGL::Initialize() {
  InitializeBindings();
}

void DriverGL::InitializeExtensions(GLContext* context) {
  InitializeExtensionBindings(context);
  orig_fn = fn;
  fn.glTexImage2DFn =
      reinterpret_cast<glTexImage2DProc>(CustomTexImage2D);
  fn.glTexSubImage2DFn =
      reinterpret_cast<glTexSubImage2DProc>(CustomTexSubImage2D);
  fn.glTexStorage2DEXTFn =
      reinterpret_cast<glTexStorage2DEXTProc>(CustomTexStorage2DEXT);
}

void InitializeGLBindingsGL() {
  g_current_gl_context_tls = new base::ThreadLocalPointer<GLApi>;
  g_driver_gl.Initialize();
  if (!g_real_gl) {
    g_real_gl = new RealGLApi();
    g_trace_gl = new TraceGLApi(g_real_gl);
  }
  g_real_gl->Initialize(&g_driver_gl);
  g_gl = g_real_gl;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableGPUServiceTracing)) {
    g_gl = g_trace_gl;
  }
  SetGLToRealGLApi();
}

GLApi* GetCurrentGLApi() {
  return g_current_gl_context_tls->Get();
}

void SetGLApi(GLApi* api) {
  g_current_gl_context_tls->Set(api);
}

void SetGLToRealGLApi() {
  SetGLApi(g_gl);
}

void InitializeGLExtensionBindingsGL(GLContext* context) {
  g_driver_gl.InitializeExtensions(context);
}

void InitializeDebugGLBindingsGL() {
  g_driver_gl.InitializeDebugBindings();
}

void ClearGLBindingsGL() {
  if (g_real_gl) {
    delete g_real_gl;
    g_real_gl = NULL;
  }
  if (g_trace_gl) {
    delete g_trace_gl;
    g_trace_gl = NULL;
  }
  g_gl = NULL;
  g_driver_gl.ClearBindings();
  if (g_current_gl_context_tls) {
    delete g_current_gl_context_tls;
    g_current_gl_context_tls = NULL;
  }
}

GLApi::GLApi() {
}

GLApi::~GLApi() {
  if (GetCurrentGLApi() == this)
    SetGLApi(NULL);
}

GLApiBase::GLApiBase()
    : driver_(NULL) {
}

GLApiBase::~GLApiBase() {
}

void GLApiBase::InitializeBase(DriverGL* driver) {
  driver_ = driver;
}

RealGLApi::RealGLApi() {
}

RealGLApi::~RealGLApi() {
}

void RealGLApi::Initialize(DriverGL* driver) {
  InitializeBase(driver);
}

TraceGLApi::~TraceGLApi() {
}

VirtualGLApi::VirtualGLApi()
    : real_context_(NULL),
      current_context_(NULL) {
}

VirtualGLApi::~VirtualGLApi() {
}

void VirtualGLApi::Initialize(DriverGL* driver, GLContext* real_context) {
  InitializeBase(driver);
  real_context_ = real_context;

  DCHECK(real_context->IsCurrent(NULL));
  std::string ext_string(
      reinterpret_cast<const char*>(driver_->fn.glGetStringFn(GL_EXTENSIONS)));
  std::vector<std::string> ext;
  Tokenize(ext_string, " ", &ext);

  std::vector<std::string>::iterator it;
  // We can't support GL_EXT_occlusion_query_boolean which is
  // based on GL_ARB_occlusion_query without a lot of work virtualizing
  // queries.
  it = std::find(ext.begin(), ext.end(), "GL_EXT_occlusion_query_boolean");
  if (it != ext.end())
    ext.erase(it);

  extensions_ = JoinString(ext, " ");
}

bool VirtualGLApi::MakeCurrent(GLContext* virtual_context, GLSurface* surface) {
  bool switched_contexts = g_current_gl_context_tls->Get() != this;
  GLSurface* current_surface = GLSurface::GetCurrent();
  if (switched_contexts || surface != current_surface) {
    // MakeCurrent 'lite' path that avoids potentially expensive MakeCurrent()
    // calls if the GLSurface uses the same underlying surface or renders to
    // an FBO.
    if (switched_contexts || !current_surface ||
        !virtual_context->IsCurrent(surface)) {
      if (!real_context_->MakeCurrent(surface)) {
        return false;
      }
    }
  }

  DCHECK_EQ(real_context_, GLContext::GetRealCurrent());
  DCHECK(real_context_->IsCurrent(NULL));
  DCHECK(virtual_context->IsCurrent(surface));

  if (switched_contexts || virtual_context != current_context_) {
    // There should be no errors from the previous context leaking into the
    // new context.
    DCHECK_EQ(glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));

    current_context_ = virtual_context;
    // Set all state that is different from the real state
    // NOTE: !!! This is a temporary implementation that just restores all
    // state to let us test that it works.
    // TODO: ASAP, change this to something that only restores the state
    // needed for individual GL calls.
    GLApi* temp = GetCurrentGLApi();
    SetGLToRealGLApi();
    if (virtual_context->GetGLStateRestorer()->IsInitialized())
      virtual_context->GetGLStateRestorer()->RestoreState();
    SetGLApi(temp);
  }
  SetGLApi(this);

  virtual_context->SetCurrent(surface);
  if (!surface->OnMakeCurrent(virtual_context)) {
    LOG(ERROR) << "Could not make GLSurface current.";
    return false;
  }
  return true;
}

void VirtualGLApi::OnReleaseVirtuallyCurrent(GLContext* virtual_context) {
  if (current_context_ == virtual_context)
    current_context_ = NULL;
}

const GLubyte* VirtualGLApi::glGetStringFn(GLenum name) {
  switch (name) {
    case GL_EXTENSIONS:
      return reinterpret_cast<const GLubyte*>(extensions_.c_str());
    default:
      return driver_->fn.glGetStringFn(name);
  }
}

}  // namespace gfx
