// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_gl_api_implementation.h"

#include <vector>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_state_restorer.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_version_info.h"

namespace gl {

// The GL state for when no context is bound
static CurrentGL* g_no_context_current_gl = nullptr;

// If the null draw bindings are currently enabled.
// TODO: Consider adding a new GLApi that no-ops these functions
static bool g_null_draw_bindings_enabled = false;

// If the GL debug bindings are enabled.
static bool g_debug_bindings_enabled = false;

namespace {

static inline GLenum GetInternalFormat(const GLVersionInfo* version,
                                       GLenum internal_format) {
  if (!version->is_es) {
    if (internal_format == GL_BGRA_EXT || internal_format == GL_BGRA8_EXT)
      return GL_RGBA8;
  }
  if (version->is_es3 && version->is_mesa) {
    // Mesa bug workaround: Mipmapping does not work when using GL_BGRA_EXT
    if (internal_format == GL_BGRA_EXT)
      return GL_RGBA;
  }
  return internal_format;
}

// TODO(epenner): Could the above function be merged into this and removed?
static inline GLenum GetTexInternalFormat(const GLVersionInfo* version,
                                          GLenum internal_format,
                                          GLenum format,
                                          GLenum type) {
  GLenum gl_internal_format = GetInternalFormat(version, internal_format);

  // g_version_info must be initialized when this function is bound.
  if (version->is_es3) {
    if (internal_format == GL_RED_EXT) {
      // GL_EXT_texture_rg case in ES2.
      switch (type) {
        case GL_UNSIGNED_BYTE:
          gl_internal_format = GL_R8_EXT;
          break;
        case GL_HALF_FLOAT_OES:
          gl_internal_format = GL_R16F_EXT;
          break;
        case GL_FLOAT:
          gl_internal_format = GL_R32F_EXT;
          break;
        default:
          NOTREACHED();
          break;
      }
      return gl_internal_format;
    } else if (internal_format == GL_RG_EXT) {
      // GL_EXT_texture_rg case in ES2.
      switch (type) {
        case GL_UNSIGNED_BYTE:
          gl_internal_format = GL_RG8_EXT;
          break;
        case GL_HALF_FLOAT_OES:
          gl_internal_format = GL_RG16F_EXT;
          break;
        case GL_FLOAT:
          gl_internal_format = GL_RG32F_EXT;
          break;
        default:
          NOTREACHED();
          break;
      }
      return gl_internal_format;
    }
  }

  if (type == GL_FLOAT && version->is_angle && version->is_es &&
      version->major_version == 2) {
    // It's possible that the texture is using a sized internal format, and
    // ANGLE exposing GLES2 API doesn't support those.
    // TODO(oetuaho@nvidia.com): Remove these conversions once ANGLE has the
    // support.
    // http://code.google.com/p/angleproject/issues/detail?id=556
    switch (format) {
      case GL_RGBA:
        gl_internal_format = GL_RGBA;
        break;
      case GL_RGB:
        gl_internal_format = GL_RGB;
        break;
      default:
        break;
    }
  }

  if (version->IsAtLeastGL(2, 1) || version->IsAtLeastGLES(3, 0)) {
    switch (internal_format) {
      case GL_SRGB_EXT:
        gl_internal_format = GL_SRGB8;
        break;
      case GL_SRGB_ALPHA_EXT:
        gl_internal_format = GL_SRGB8_ALPHA8;
        break;
      default:
        break;
    }
  }

  if (version->is_es)
    return gl_internal_format;

  if (type == GL_FLOAT) {
    switch (internal_format) {
      // We need to map all the unsized internal formats from ES2 clients.
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
      // RED and RG are reached here because on Desktop GL core profile,
      // LUMINANCE/ALPHA formats are emulated through RED and RG in Chrome.
      case GL_RED:
        gl_internal_format = GL_R32F;
        break;
      case GL_RG:
        gl_internal_format = GL_RG32F;
        break;
      default:
        // We can't assert here because if the client context is ES3,
        // all sized internal_format will reach here.
        break;
    }
  } else if (type == GL_HALF_FLOAT_OES) {
    switch (internal_format) {
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
      // RED and RG are reached here because on Desktop GL core profile,
      // LUMINANCE/ALPHA formats are emulated through RED and RG in Chrome.
      case GL_RED:
        gl_internal_format = GL_R16F;
        break;
      case GL_RG:
        gl_internal_format = GL_RG16F;
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  return gl_internal_format;
}

static inline GLenum GetTexFormat(const GLVersionInfo* version, GLenum format) {
  GLenum gl_format = format;

  if (version->IsAtLeastGL(2, 1) || version->IsAtLeastGLES(3, 0)) {
    switch (format) {
      case GL_SRGB_EXT:
        gl_format = GL_RGB;
        break;
      case GL_SRGB_ALPHA_EXT:
        gl_format = GL_RGBA;
        break;
      default:
        break;
    }
  }

  return gl_format;
}

static inline GLenum GetTexType(const GLVersionInfo* version, GLenum type) {
  if (!version->is_es) {
    if (type == GL_HALF_FLOAT_OES)
      return GL_HALF_FLOAT_ARB;
  }
  return type;
}

}  // anonymous namespace

void InitializeStaticGLBindingsGL() {
  g_current_gl_context_tls = new base::ThreadLocalPointer<CurrentGL>;
  g_no_context_current_gl = new CurrentGL;
  g_no_context_current_gl->Api = new NoContextGLApi;
}

void ClearBindingsGL() {
  if (g_no_context_current_gl) {
    delete g_no_context_current_gl->Api;
    delete g_no_context_current_gl->Driver;
    delete g_no_context_current_gl->Version;
    delete g_no_context_current_gl;
    g_no_context_current_gl = nullptr;
  }

  if (g_current_gl_context_tls) {
    delete g_current_gl_context_tls;
    g_current_gl_context_tls = nullptr;
  }
}

void InitializeDebugGLBindingsGL() {
  g_debug_bindings_enabled = true;
}

bool GetDebugGLBindingsInitializedGL() {
  return g_debug_bindings_enabled;
}

bool SetNullDrawGLBindingsEnabled(bool enabled) {
  bool old_value = g_null_draw_bindings_enabled;
  g_null_draw_bindings_enabled = enabled;
  return old_value;
}

bool GetNullDrawBindingsEnabled() {
  return g_null_draw_bindings_enabled;
}

void SetCurrentGL(CurrentGL* current) {
  CurrentGL* new_current = current ? current : g_no_context_current_gl;
  g_current_gl_context_tls->Set(new_current);
}

GLApi::GLApi() {
}

GLApi::~GLApi() {
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
#if DCHECK_IS_ON()
  filtered_exts_initialized_ = false;
#endif
}

RealGLApi::~RealGLApi() {
}

void RealGLApi::Initialize(DriverGL* driver) {
  InitializeWithCommandLine(driver, base::CommandLine::ForCurrentProcess());
}

void RealGLApi::InitializeWithCommandLine(DriverGL* driver,
                                          base::CommandLine* command_line) {
  DCHECK(command_line);
  InitializeBase(driver);

  const std::string disabled_extensions = command_line->GetSwitchValueASCII(
      switches::kDisableGLExtensions);
  if (!disabled_extensions.empty()) {
    disabled_exts_ = base::SplitString(
        disabled_extensions, ", ;",
        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }
}

void RealGLApi::glGetIntegervFn(GLenum pname, GLint* params) {
  if (pname == GL_NUM_EXTENSIONS && disabled_exts_.size()) {
#if DCHECK_IS_ON()
    DCHECK(filtered_exts_initialized_);
#endif
    *params = static_cast<GLint>(filtered_exts_.size());
  } else {
    GLApiBase::glGetIntegervFn(pname, params);
  }
}

const GLubyte* RealGLApi::glGetStringFn(GLenum name) {
  if (name == GL_EXTENSIONS && disabled_exts_.size()) {
#if DCHECK_IS_ON()
    DCHECK(filtered_exts_initialized_);
#endif
    return reinterpret_cast<const GLubyte*>(filtered_exts_str_.c_str());
  }
  return GLApiBase::glGetStringFn(name);
}

const GLubyte* RealGLApi::glGetStringiFn(GLenum name, GLuint index) {
  if (name == GL_EXTENSIONS && disabled_exts_.size()) {
#if DCHECK_IS_ON()
    DCHECK(filtered_exts_initialized_);
#endif
    if (index >= filtered_exts_.size()) {
      return NULL;
    }
    return reinterpret_cast<const GLubyte*>(filtered_exts_[index].c_str());
  }
  return GLApiBase::glGetStringiFn(name, index);
}

void RealGLApi::glTexImage2DFn(GLenum target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               const void* pixels) {
  GLenum gl_internal_format =
      GetTexInternalFormat(version_.get(), internalformat, format, type);
  GLenum gl_format = GetTexFormat(version_.get(), format);
  GLenum gl_type = GetTexType(version_.get(), type);
  GLApiBase::glTexImage2DFn(target, level, gl_internal_format, width, height,
                            border, gl_format, gl_type, pixels);
}

void RealGLApi::glTexSubImage2DFn(GLenum target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLenum format,
                                  GLenum type,
                                  const void* pixels) {
  GLenum gl_format = GetTexFormat(version_.get(), format);
  GLenum gl_type = GetTexType(version_.get(), type);
  GLApiBase::glTexSubImage2DFn(target, level, xoffset, yoffset, width, height,
                               gl_format, gl_type, pixels);
}

void RealGLApi::glTexStorage2DEXTFn(GLenum target,
                                    GLsizei levels,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height) {
  GLenum gl_internal_format = GetInternalFormat(version_.get(), internalformat);
  GLApiBase::glTexStorage2DEXTFn(target, levels, gl_internal_format, width,
                                 height);
}

void RealGLApi::glRenderbufferStorageEXTFn(GLenum target,
                                           GLenum internalformat,
                                           GLsizei width,
                                           GLsizei height) {
  GLenum gl_internal_format = GetInternalFormat(version_.get(), internalformat);
  GLApiBase::glRenderbufferStorageEXTFn(target, gl_internal_format, width,
                                        height);
}

// The ANGLE and IMG variants of glRenderbufferStorageMultisample currently do
// not support BGRA render buffers so only the EXT one is customized. If
// GL_CHROMIUM_renderbuffer_format_BGRA8888 support is added to ANGLE then the
// ANGLE version should also be customized.
void RealGLApi::glRenderbufferStorageMultisampleEXTFn(GLenum target,
                                                      GLsizei samples,
                                                      GLenum internalformat,
                                                      GLsizei width,
                                                      GLsizei height) {
  GLenum gl_internal_format = GetInternalFormat(version_.get(), internalformat);
  GLApiBase::glRenderbufferStorageMultisampleEXTFn(
      target, samples, gl_internal_format, width, height);
}

void RealGLApi::glRenderbufferStorageMultisampleFn(GLenum target,
                                                   GLsizei samples,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height) {
  GLenum gl_internal_format = GetInternalFormat(version_.get(), internalformat);
  GLApiBase::glRenderbufferStorageMultisampleFn(
      target, samples, gl_internal_format, width, height);
}

void RealGLApi::glClearFn(GLbitfield mask) {
  if (!g_null_draw_bindings_enabled)
    GLApiBase::glClearFn(mask);
}

void RealGLApi::glDrawArraysFn(GLenum mode, GLint first, GLsizei count) {
  if (!g_null_draw_bindings_enabled)
    GLApiBase::glDrawArraysFn(mode, first, count);
}

void RealGLApi::glDrawElementsFn(GLenum mode,
                                 GLsizei count,
                                 GLenum type,
                                 const void* indices) {
  if (!g_null_draw_bindings_enabled)
    GLApiBase::glDrawElementsFn(mode, count, type, indices);
}

void RealGLApi::glClearDepthFn(GLclampd depth) {
  // OpenGL ES only has glClearDepthf, forward the parameters from glClearDepth.
  // Many mock tests expect only glClearDepth is called so don't make the
  // interception when testing with mocks.
  if (version_->is_es && GetGLImplementation() != kGLImplementationMockGL) {
    DCHECK(driver_->fn.glClearDepthfFn);
    GLApiBase::glClearDepthfFn(static_cast<GLclampf>(depth));
  } else {
    DCHECK(driver_->fn.glClearDepthFn);
    GLApiBase::glClearDepthFn(depth);
  }
}

void RealGLApi::glDepthRangeFn(GLclampd z_near, GLclampd z_far) {
  // OpenGL ES only has glDepthRangef, forward the parameters from glDepthRange.
  // Many mock tests expect only glDepthRange is called so don't make the
  // interception when testing with mocks.
  if (version_->is_es && GetGLImplementation() != kGLImplementationMockGL) {
    DCHECK(driver_->fn.glDepthRangefFn);
    GLApiBase::glDepthRangefFn(static_cast<GLclampf>(z_near),
                               static_cast<GLclampf>(z_far));
  } else {
    DCHECK(driver_->fn.glDepthRangeFn);
    GLApiBase::glDepthRangeFn(z_near, z_far);
  }
}

void RealGLApi::InitializeFilteredExtensions() {
  if (disabled_exts_.size()) {
    filtered_exts_.clear();
    if (WillUseGLGetStringForExtensions(this)) {
      filtered_exts_str_ =
          FilterGLExtensionList(reinterpret_cast<const char*>(
                                    GLApiBase::glGetStringFn(GL_EXTENSIONS)),
                                disabled_exts_);
      filtered_exts_ = base::SplitString(
          filtered_exts_str_, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    } else {
      GLint num_extensions = 0;
      GLApiBase::glGetIntegervFn(GL_NUM_EXTENSIONS, &num_extensions);
      for (GLint i = 0; i < num_extensions; ++i) {
        const char* gl_extension = reinterpret_cast<const char*>(
            GLApiBase::glGetStringiFn(GL_EXTENSIONS, i));
        DCHECK(gl_extension != NULL);
        if (!base::ContainsValue(disabled_exts_, gl_extension))
          filtered_exts_.push_back(gl_extension);
      }
      filtered_exts_str_ = base::JoinString(filtered_exts_, " ");
    }
#if DCHECK_IS_ON()
    filtered_exts_initialized_ = true;
#endif
  }
}

void RealGLApi::set_version(std::unique_ptr<GLVersionInfo> version) {
  version_ = std::move(version);
}

TraceGLApi::~TraceGLApi() {
}

DebugGLApi::DebugGLApi(GLApi* gl_api) : gl_api_(gl_api) {}

DebugGLApi::~DebugGLApi() {}

NoContextGLApi::NoContextGLApi() {
}

NoContextGLApi::~NoContextGLApi() {
}

}  // namespace gl
