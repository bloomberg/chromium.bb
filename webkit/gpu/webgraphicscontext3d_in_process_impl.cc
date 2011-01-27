// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"

#include <string.h>

#include <algorithm>
#include <string>

#include "app/gfx/gl/gl_bindings.h"
#include "app/gfx/gl/gl_context.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebGLId;
using WebKit::WebGraphicsContext3D;
using WebKit::WebString;
using WebKit::WebView;

namespace webkit {
namespace gpu {

enum {
  MAX_VERTEX_UNIFORM_VECTORS = 0x8DFB,
  MAX_VARYING_VECTORS = 0x8DFC,
  MAX_FRAGMENT_UNIFORM_VECTORS = 0x8DFD
};

WebGraphicsContext3DInProcessImpl::
    VertexAttribPointerState::VertexAttribPointerState()
    : enabled(false),
      buffer(0),
      indx(0),
      size(0),
      type(0),
      normalized(false),
      stride(0),
      offset(0) {
}

WebGraphicsContext3DInProcessImpl::WebGraphicsContext3DInProcessImpl()
    : initialized_(false),
      render_directly_to_web_view_(false),
      is_gles2_(false),
      have_ext_framebuffer_object_(false),
      have_ext_framebuffer_multisample_(false),
      have_angle_framebuffer_multisample_(false),
      texture_(0),
      fbo_(0),
      depth_stencil_buffer_(0),
      cached_width_(0),
      cached_height_(0),
      multisample_fbo_(0),
      multisample_depth_stencil_buffer_(0),
      multisample_color_buffer_(0),
      bound_fbo_(0),
      bound_texture_(0),
      copy_texture_to_parent_texture_fbo_(0),
      bound_array_buffer_(0),
#ifdef FLIP_FRAMEBUFFER_VERTICALLY
      scanline_(0),
#endif
      fragment_compiler_(0),
      vertex_compiler_(0) {
}

WebGraphicsContext3DInProcessImpl::~WebGraphicsContext3DInProcessImpl() {
  if (!initialized_)
    return;

  makeContextCurrent();

  if (attributes_.antialias) {
    glDeleteRenderbuffersEXT(1, &multisample_color_buffer_);
    if (attributes_.depth || attributes_.stencil)
      glDeleteRenderbuffersEXT(1, &multisample_depth_stencil_buffer_);
    glDeleteFramebuffersEXT(1, &multisample_fbo_);
  } else {
    if (attributes_.depth || attributes_.stencil)
      glDeleteRenderbuffersEXT(1, &depth_stencil_buffer_);
  }
  glDeleteTextures(1, &texture_);
  glDeleteFramebuffersEXT(1, &copy_texture_to_parent_texture_fbo_);
#ifdef FLIP_FRAMEBUFFER_VERTICALLY
  if (scanline_)
    delete[] scanline_;
#endif
  glDeleteFramebuffersEXT(1, &fbo_);

  gl_context_->Destroy();

  for (ShaderSourceMap::iterator ii = shader_source_map_.begin();
       ii != shader_source_map_.end(); ++ii) {
    if (ii->second)
      delete ii->second;
  }
  AngleDestroyCompilers();
}

bool WebGraphicsContext3DInProcessImpl::initialize(
    WebGraphicsContext3D::Attributes attributes,
    WebView* webView,
    bool render_directly_to_web_view) {
  if (!gfx::GLContext::InitializeOneOff())
    return false;

  render_directly_to_web_view_ = render_directly_to_web_view;
  gfx::GLContext* share_context = 0;

  if (!render_directly_to_web_view) {
    // Pick up the compositor's context to share resources with.
    WebGraphicsContext3D* view_context = webView->graphicsContext3D();
    if (view_context) {
      WebGraphicsContext3DInProcessImpl* contextImpl =
          static_cast<WebGraphicsContext3DInProcessImpl*>(view_context);
      share_context = contextImpl->gl_context_.get();
    } else {
      // The compositor's context didn't get created
      // successfully, so conceptually there is no way we can
      // render successfully to the WebView.
      render_directly_to_web_view_ = false;
    }
  }

  // This implementation always renders offscreen regardless of
  // whether render_directly_to_web_view is true. Both DumpRenderTree
  // and test_shell paint first to an intermediate offscreen buffer
  // and from there to the window, and WebViewImpl::paint already
  // correctly handles the case where the compositor is active but
  // the output needs to go to a WebCanvas.
  gl_context_.reset(gfx::GLContext::CreateOffscreenGLContext(share_context));
  if (!gl_context_.get())
    return false;

  attributes_ = attributes;

  // FIXME: for the moment we disable multisampling for the compositor.
  // It actually works in this implementation, but there are a few
  // considerations. First, we likely want to reduce the fuzziness in
  // these tests as much as possible because we want to run pixel tests.
  // Second, Mesa's multisampling doesn't seem to antialias straight
  // edges in some CSS 3D samples. Third, we don't have multisampling
  // support for the compositor in the normal case at the time of this
  // writing.
  if (render_directly_to_web_view)
    attributes_.antialias = false;

  is_gles2_ = gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2;
  const char* extensions =
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
  have_ext_framebuffer_object_ =
      strstr(extensions, "GL_EXT_framebuffer_object") != NULL;
  have_ext_framebuffer_multisample_ =
      strstr(extensions, "GL_EXT_framebuffer_multisample") != NULL;
  have_angle_framebuffer_multisample_ =
      strstr(extensions, "GL_ANGLE_framebuffer_multisample") != NULL;

  ValidateAttributes();

  if (!is_gles2_) {
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);
  }

  if (!AngleCreateCompilers()) {
    AngleDestroyCompilers();
    return false;
  }

  glGenFramebuffersEXT(1, &copy_texture_to_parent_texture_fbo_);

  initialized_ = true;
  return true;
}

void WebGraphicsContext3DInProcessImpl::ValidateAttributes() {
  const char* extensions =
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));

  if (attributes_.stencil) {
    if (strstr(extensions, "GL_OES_packed_depth_stencil") ||
        strstr(extensions, "GL_EXT_packed_depth_stencil")) {
      if (!attributes_.depth) {
        attributes_.depth = true;
      }
    } else {
      attributes_.stencil = false;
    }
  }
  if (attributes_.antialias) {
    bool isValidVendor = true;
#if defined(OS_MACOSX)
    // Currently in Mac we only turn on antialias if vendor is NVIDIA.
    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    if (!strstr(vendor, "NVIDIA"))
      isValidVendor = false;
#endif
    if (!(isValidVendor &&
          (have_ext_framebuffer_multisample_ ||
           (have_angle_framebuffer_multisample_ &&
            strstr(extensions, "GL_OES_rgb8_rgba8")))))
      attributes_.antialias = false;

    // Don't antialias when using Mesa to ensure more reliable testing and
    // because it doesn't appear to multisample straight lines correctly.
    const char* renderer =
        reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    if (!strncmp(renderer, "Mesa", 4)) {
      attributes_.antialias = false;
    }
  }
  // FIXME: instead of enforcing premultipliedAlpha = true, implement the
  // correct behavior when premultipliedAlpha = false is requested.
  attributes_.premultipliedAlpha = true;
}

void WebGraphicsContext3DInProcessImpl::ResolveMultisampledFramebuffer(
    unsigned x, unsigned y, unsigned width, unsigned height) {
  if (attributes_.antialias) {
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, multisample_fbo_);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, fbo_);
    if (have_ext_framebuffer_multisample_) {
      glBlitFramebufferEXT(x, y,
                           x + width, y + height,
                           x, y,
                           x + width, y + height,
                           GL_COLOR_BUFFER_BIT, GL_NEAREST);
    } else {
      DCHECK(have_angle_framebuffer_multisample_);
      glBlitFramebufferANGLE(x, y,
                             x + width, y + height,
                             x, y,
                             x + width, y + height,
                             GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, bound_fbo_);
  }
}

bool WebGraphicsContext3DInProcessImpl::makeContextCurrent() {
  return gl_context_->MakeCurrent();
}

int WebGraphicsContext3DInProcessImpl::width() {
  return cached_width_;
}

int WebGraphicsContext3DInProcessImpl::height() {
  return cached_height_;
}

int WebGraphicsContext3DInProcessImpl::sizeInBytes(int type) {
  switch (type) {
  case GL_BYTE:
    return sizeof(GLbyte);
  case GL_UNSIGNED_BYTE:
    return sizeof(GLubyte);
  case GL_SHORT:
    return sizeof(GLshort);
  case GL_UNSIGNED_SHORT:
    return sizeof(GLushort);
  case GL_INT:
    return sizeof(GLint);
  case GL_UNSIGNED_INT:
    return sizeof(GLuint);
  case GL_FLOAT:
    return sizeof(GLfloat);
  }
  return 0;
}

bool WebGraphicsContext3DInProcessImpl::isGLES2Compliant() {
  return is_gles2_;
}

unsigned int WebGraphicsContext3DInProcessImpl::getPlatformTextureId() {
  return texture_;
}

void WebGraphicsContext3DInProcessImpl::prepareTexture() {
  if (!render_directly_to_web_view_) {
    // We need to prepare our rendering results for the compositor.
    makeContextCurrent();
    ResolveMultisampledFramebuffer(0, 0, cached_width_, cached_height_);
  }
}

namespace {

int CreateTextureObject(GLenum target) {
  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(target, texture);
  glTexParameterf(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  return texture;
}

}  // anonymous namespace

void WebGraphicsContext3DInProcessImpl::reshape(int width, int height) {
  cached_width_ = width;
  cached_height_ = height;
  makeContextCurrent();

  GLenum target = GL_TEXTURE_2D;

  if (!texture_) {
    // Generate the texture object
    texture_ = CreateTextureObject(target);
    // Generate the framebuffer object
    glGenFramebuffersEXT(1, &fbo_);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);
    bound_fbo_ = fbo_;
    if (attributes_.depth || attributes_.stencil)
      glGenRenderbuffersEXT(1, &depth_stencil_buffer_);
    // Generate the multisample framebuffer object
    if (attributes_.antialias) {
      glGenFramebuffersEXT(1, &multisample_fbo_);
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, multisample_fbo_);
      bound_fbo_ = multisample_fbo_;
      glGenRenderbuffersEXT(1, &multisample_color_buffer_);
      if (attributes_.depth || attributes_.stencil)
        glGenRenderbuffersEXT(1, &multisample_depth_stencil_buffer_);
    }
  }

  GLint internal_multisampled_color_format = 0;
  GLint internal_color_format = 0;
  GLint color_format = 0;
  GLint internal_depth_stencil_format = 0;
  if (attributes_.alpha) {
    // GL_RGBA8_OES == GL_RGBA8
    internal_multisampled_color_format = GL_RGBA8;
    internal_color_format = is_gles2_ ? GL_RGBA : GL_RGBA8;
    color_format = GL_RGBA;
  } else {
    // GL_RGB8_OES == GL_RGB8
    internal_multisampled_color_format = GL_RGB8;
    internal_color_format = is_gles2_ ? GL_RGB : GL_RGB8;
    color_format = GL_RGB;
  }
  if (attributes_.stencil || attributes_.depth) {
    // We don't allow the logic where stencil is required and depth is not.
    // See GraphicsContext3DInternal constructor.
    if (attributes_.stencil && attributes_.depth) {
      internal_depth_stencil_format = GL_DEPTH24_STENCIL8_EXT;
    } else {
      if (is_gles2_)
        internal_depth_stencil_format = GL_DEPTH_COMPONENT16;
      else
        internal_depth_stencil_format = GL_DEPTH_COMPONENT;
    }
  }

  bool must_restore_fbo = false;

  // Resize multisampling FBO
  if (attributes_.antialias) {
    GLint max_sample_count;
    glGetIntegerv(GL_MAX_SAMPLES_EXT, &max_sample_count);
    GLint sample_count = std::min(8, max_sample_count);
    if (bound_fbo_ != multisample_fbo_) {
      must_restore_fbo = true;
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, multisample_fbo_);
    }
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, multisample_color_buffer_);
    if (have_ext_framebuffer_multisample_) {
      glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT,
                                          sample_count,
                                          internal_multisampled_color_format,
                                          width,
                                          height);
    } else {
      DCHECK(have_angle_framebuffer_multisample_);
      glRenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER_EXT,
                                            sample_count,
                                            internal_multisampled_color_format,
                                            width,
                                            height);
    }
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                 GL_COLOR_ATTACHMENT0_EXT,
                                 GL_RENDERBUFFER_EXT,
                                 multisample_color_buffer_);
    if (attributes_.stencil || attributes_.depth) {
      glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,
                            multisample_depth_stencil_buffer_);
      if (have_ext_framebuffer_multisample_) {
        glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT,
                                            sample_count,
                                            internal_depth_stencil_format,
                                            width,
                                            height);
      } else {
        DCHECK(have_angle_framebuffer_multisample_);
        glRenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER_EXT,
                                              sample_count,
                                              internal_depth_stencil_format,
                                              width,
                                              height);
      }
      if (attributes_.stencil)
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                     GL_STENCIL_ATTACHMENT_EXT,
                                     GL_RENDERBUFFER_EXT,
                                     multisample_depth_stencil_buffer_);
      if (attributes_.depth)
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                     GL_DEPTH_ATTACHMENT_EXT,
                                     GL_RENDERBUFFER_EXT,
                                     multisample_depth_stencil_buffer_);
    }
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
    if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
      LOG(ERROR) << "Multisampling framebuffer was incomplete";

      // FIXME: cleanup.
      NOTIMPLEMENTED();
    }
  }

  // Resize regular FBO
  if (bound_fbo_ != fbo_) {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);
    must_restore_fbo = true;
  }
  glBindTexture(target, texture_);
  glTexImage2D(target, 0, internal_color_format,
               width, height,
               0, color_format, GL_UNSIGNED_BYTE, 0);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                            GL_COLOR_ATTACHMENT0_EXT,
                            target,
                            texture_,
                            0);
  glBindTexture(target, 0);
  if (!attributes_.antialias && (attributes_.stencil || attributes_.depth)) {
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth_stencil_buffer_);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,
                             internal_depth_stencil_format,
                             width, height);
    if (attributes_.stencil)
      glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                   GL_STENCIL_ATTACHMENT_EXT,
                                   GL_RENDERBUFFER_EXT,
                                   depth_stencil_buffer_);
    if (attributes_.depth)
      glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                   GL_DEPTH_ATTACHMENT_EXT,
                                   GL_RENDERBUFFER_EXT,
                                   depth_stencil_buffer_);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
  }
  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
    LOG(ERROR) << "Framebuffer was incomplete";

    // FIXME: cleanup.
    NOTIMPLEMENTED();
  }

  if (attributes_.antialias) {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, multisample_fbo_);
    if (bound_fbo_ == multisample_fbo_)
      must_restore_fbo = false;
  }

  // Initialize renderbuffers to 0.
  GLfloat clearColor[] = {0, 0, 0, 0}, clearDepth = 0;
  GLint clearStencil = 0;
  GLboolean colorMask[] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
  GLboolean depthMask = GL_TRUE;
  GLuint stencilMask = 0xffffffff;
  GLboolean isScissorEnabled = GL_FALSE;
  GLboolean isDitherEnabled = GL_FALSE;
  GLbitfield clearMask = GL_COLOR_BUFFER_BIT;
  glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
  glClearColor(0, 0, 0, 0);
  glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  if (attributes_.depth) {
    glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
    glClearDepth(1);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    glDepthMask(GL_TRUE);
    clearMask |= GL_DEPTH_BUFFER_BIT;
  }
  if (attributes_.stencil) {
    glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &clearStencil);
    glClearStencil(0);
    glGetIntegerv(GL_STENCIL_WRITEMASK,
                  reinterpret_cast<GLint*>(&stencilMask));
    glStencilMaskSeparate(GL_FRONT, 0xffffffff);
    clearMask |= GL_STENCIL_BUFFER_BIT;
  }
  isScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
  glDisable(GL_SCISSOR_TEST);
  isDitherEnabled = glIsEnabled(GL_DITHER);
  glDisable(GL_DITHER);

  glClear(clearMask);

  glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
  glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
  if (attributes_.depth) {
    glClearDepth(clearDepth);
    glDepthMask(depthMask);
  }
  if (attributes_.stencil) {
    glClearStencil(clearStencil);
    glStencilMaskSeparate(GL_FRONT, stencilMask);
  }
  if (isScissorEnabled)
    glEnable(GL_SCISSOR_TEST);
  else
    glDisable(GL_SCISSOR_TEST);
  if (isDitherEnabled)
    glEnable(GL_DITHER);
  else
    glDisable(GL_DITHER);

  if (must_restore_fbo)
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, bound_fbo_);

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
  if (scanline_) {
    delete[] scanline_;
    scanline_ = 0;
  }
  scanline_ = new unsigned char[width * 4];
#endif  // FLIP_FRAMEBUFFER_VERTICALLY
}

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
void WebGraphicsContext3DInProcessImpl::FlipVertically(
    unsigned char* framebuffer, unsigned int width, unsigned int height) {
  unsigned char* scanline = scanline_;
  if (!scanline)
    return;
  unsigned int row_bytes = width * 4;
  unsigned int count = height / 2;
  for (unsigned int i = 0; i < count; i++) {
    unsigned char* row_a = framebuffer + i * row_bytes;
    unsigned char* row_b = framebuffer + (height - i - 1) * row_bytes;
    // FIXME: this is where the multiplication of the alpha
    // channel into the color buffer will need to occur if the
    // user specifies the "premultiplyAlpha" flag in the context
    // creation attributes.
    memcpy(scanline, row_b, row_bytes);
    memcpy(row_b, row_a, row_bytes);
    memcpy(row_a, scanline, row_bytes);
  }
}
#endif

bool WebGraphicsContext3DInProcessImpl::readBackFramebuffer(
    unsigned char* pixels, size_t bufferSize) {
  if (bufferSize != static_cast<size_t>(4 * width() * height()))
    return false;

  makeContextCurrent();

  // Earlier versions of this code used the GPU to flip the
  // framebuffer vertically before reading it back for compositing
  // via software. This code was quite complicated, used a lot of
  // GPU memory, and didn't provide an obvious speedup. Since this
  // vertical flip is only a temporary solution anyway until Chrome
  // is fully GPU composited, it wasn't worth the complexity.

  ResolveMultisampledFramebuffer(0, 0, cached_width_, cached_height_);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);

  GLint pack_alignment = 4;
  bool must_restore_pack_alignment = false;
  glGetIntegerv(GL_PACK_ALIGNMENT, &pack_alignment);
  if (pack_alignment > 4) {
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    must_restore_pack_alignment = true;
  }

  if (is_gles2_) {
    // FIXME: consider testing for presence of GL_OES_read_format
    // and GL_EXT_read_format_bgra, and using GL_BGRA_EXT here
    // directly.
    glReadPixels(0, 0, cached_width_, cached_height_,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    for (size_t i = 0; i < bufferSize; i += 4) {
      std::swap(pixels[i], pixels[i + 2]);
    }
  } else {
    glReadPixels(0, 0, cached_width_, cached_height_,
                 GL_BGRA, GL_UNSIGNED_BYTE, pixels);
  }

  if (must_restore_pack_alignment)
    glPixelStorei(GL_PACK_ALIGNMENT, pack_alignment);

  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, bound_fbo_);

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
  if (pixels)
    FlipVertically(pixels, cached_width_, cached_height_);
#endif

  return true;
}

void WebGraphicsContext3DInProcessImpl::synthesizeGLError(unsigned long error) {
  if (synthetic_errors_set_.find(error) == synthetic_errors_set_.end()) {
    synthetic_errors_set_.insert(error);
    synthetic_errors_list_.push_back(error);
  }
}

void* WebGraphicsContext3DInProcessImpl::mapBufferSubDataCHROMIUM(
    unsigned target, int offset, int size, unsigned access) {
  return 0;
}

void WebGraphicsContext3DInProcessImpl::unmapBufferSubDataCHROMIUM(
    const void* mem) {
}

void* WebGraphicsContext3DInProcessImpl::mapTexSubImage2DCHROMIUM(
    unsigned target, int level, int xoffset, int yoffset,
    int width, int height, unsigned format, unsigned type, unsigned access) {
  return 0;
}

void WebGraphicsContext3DInProcessImpl::unmapTexSubImage2DCHROMIUM(
    const void* mem) {
}

void WebGraphicsContext3DInProcessImpl::copyTextureToParentTextureCHROMIUM(
    unsigned id, unsigned id2) {
  if (!glGetTexLevelParameteriv)
    return;

  makeContextCurrent();
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, copy_texture_to_parent_texture_fbo_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
                            GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_2D,
                            id,
                            0);  // level
  glBindTexture(GL_TEXTURE_2D, id2);
  GLsizei width, height;
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
  glCopyTexImage2D(GL_TEXTURE_2D,
                   0,  // level
                   GL_RGBA,
                   0, 0,  // x, y
                   width,
                   height,
                   0);  // border
  glBindTexture(GL_TEXTURE_2D, bound_texture_);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, bound_fbo_);
}

WebString WebGraphicsContext3DInProcessImpl::
    getRequestableExtensionsCHROMIUM() {
  return WebString();
}

void WebGraphicsContext3DInProcessImpl::requestExtensionCHROMIUM(const char*) {
}

void WebGraphicsContext3DInProcessImpl::blitFramebufferCHROMIUM(
    int srcX0, int srcY0, int srcX1, int srcY1,
    int dstX0, int dstY0, int dstX1, int dstY1,
    unsigned mask, unsigned filter) {
}

void WebGraphicsContext3DInProcessImpl::renderbufferStorageMultisampleCHROMIUM(
    unsigned long target, int samples, unsigned internalformat,
    unsigned width, unsigned height) {
}

// Helper macros to reduce the amount of code.

#define DELEGATE_TO_GL(name, glname)                                           \
void WebGraphicsContext3DInProcessImpl::name() {                               \
  makeContextCurrent();                                                        \
  gl##glname();                                                                \
}

#define DELEGATE_TO_GL_1(name, glname, t1)                                     \
void WebGraphicsContext3DInProcessImpl::name(t1 a1) {                          \
  makeContextCurrent();                                                        \
  gl##glname(a1);                                                              \
}

#define DELEGATE_TO_GL_1_C(name, glname, t1)                                   \
void WebGraphicsContext3DInProcessImpl::name(t1 a1) {                          \
  makeContextCurrent();                                                        \
  gl##glname(static_cast<GLclampf>(a1));                                       \
}

#define DELEGATE_TO_GL_1R(name, glname, t1, rt)                                \
rt WebGraphicsContext3DInProcessImpl::name(t1 a1) {                            \
  makeContextCurrent();                                                        \
  return gl##glname(a1);                                                       \
}

#define DELEGATE_TO_GL_1RB(name, glname, t1, rt)                               \
rt WebGraphicsContext3DInProcessImpl::name(t1 a1) {                            \
  makeContextCurrent();                                                        \
  return gl##glname(a1) ? true : false;                                        \
}

#define DELEGATE_TO_GL_2(name, glname, t1, t2)                                 \
void WebGraphicsContext3DInProcessImpl::name(t1 a1, t2 a2) {                   \
  makeContextCurrent();                                                        \
  gl##glname(a1, a2);                                                          \
}

#define DELEGATE_TO_GL_2_C1(name, glname, t1, t2)                              \
void WebGraphicsContext3DInProcessImpl::name(t1 a1, t2 a2) {                   \
  makeContextCurrent();                                                        \
  gl##glname(static_cast<GLclampf>(a1), a2);                                   \
}

#define DELEGATE_TO_GL_2_C12(name, glname, t1, t2)                             \
void WebGraphicsContext3DInProcessImpl::name(t1 a1, t2 a2) {                   \
  makeContextCurrent();                                                        \
  gl##glname(static_cast<GLclampf>(a1), static_cast<GLclampf>(a2));            \
}

#define DELEGATE_TO_GL_2R(name, glname, t1, t2, rt)                            \
rt WebGraphicsContext3DInProcessImpl::name(t1 a1, t2 a2) {                     \
  makeContextCurrent();                                                        \
  return gl##glname(a1, a2);                                                   \
}

#define DELEGATE_TO_GL_3(name, glname, t1, t2, t3)                             \
void WebGraphicsContext3DInProcessImpl::name(t1 a1, t2 a2, t3 a3) {            \
  makeContextCurrent();                                                        \
  gl##glname(a1, a2, a3);                                                      \
}

#define DELEGATE_TO_GL_4(name, glname, t1, t2, t3, t4)                         \
void WebGraphicsContext3DInProcessImpl::name(t1 a1, t2 a2, t3 a3, t4 a4) {     \
  makeContextCurrent();                                                        \
  gl##glname(a1, a2, a3, a4);                                                  \
}

#define DELEGATE_TO_GL_4_C1234(name, glname, t1, t2, t3, t4)                   \
void WebGraphicsContext3DInProcessImpl::name(t1 a1, t2 a2, t3 a3, t4 a4) {     \
  makeContextCurrent();                                                        \
  gl##glname(static_cast<GLclampf>(a1), static_cast<GLclampf>(a2),             \
             static_cast<GLclampf>(a3), static_cast<GLclampf>(a4));            \
}

#define DELEGATE_TO_GL_5(name, glname, t1, t2, t3, t4, t5)                     \
void WebGraphicsContext3DInProcessImpl::name(t1 a1, t2 a2, t3 a3, t4 a4,       \
                                             t5 a5) {                          \
  makeContextCurrent();                                                        \
  gl##glname(a1, a2, a3, a4, a5);                                              \
}

#define DELEGATE_TO_GL_6(name, glname, t1, t2, t3, t4, t5, t6)                 \
void WebGraphicsContext3DInProcessImpl::name(t1 a1, t2 a2, t3 a3, t4 a4,       \
                                             t5 a5, t6 a6) {                   \
  makeContextCurrent();                                                        \
  gl##glname(a1, a2, a3, a4, a5, a6);                                          \
}

#define DELEGATE_TO_GL_7(name, glname, t1, t2, t3, t4, t5, t6, t7)             \
void WebGraphicsContext3DInProcessImpl::name(t1 a1, t2 a2, t3 a3, t4 a4,       \
                                             t5 a5, t6 a6, t7 a7) {            \
  makeContextCurrent();                                                        \
  gl##glname(a1, a2, a3, a4, a5, a6, a7);                                      \
}

#define DELEGATE_TO_GL_8(name, glname, t1, t2, t3, t4, t5, t6, t7, t8)         \
void WebGraphicsContext3DInProcessImpl::name(t1 a1, t2 a2, t3 a3, t4 a4,       \
                                             t5 a5, t6 a6, t7 a7, t8 a8) {     \
  makeContextCurrent();                                                        \
  gl##glname(a1, a2, a3, a4, a5, a6, a7, a8);                                  \
}

#define DELEGATE_TO_GL_9(name, glname, t1, t2, t3, t4, t5, t6, t7, t8, t9)     \
void WebGraphicsContext3DInProcessImpl::name(t1 a1, t2 a2, t3 a3, t4 a4,       \
                                             t5 a5, t6 a6, t7 a7, t8 a8,       \
                                             t9 a9) {                          \
  makeContextCurrent();                                                        \
  gl##glname(a1, a2, a3, a4, a5, a6, a7, a8, a9);                              \
}

void WebGraphicsContext3DInProcessImpl::activeTexture(unsigned long texture) {
  // FIXME: query number of textures available.
  if (texture < GL_TEXTURE0 || texture > GL_TEXTURE0+32)
    // FIXME: raise exception.
    return;

  makeContextCurrent();
  glActiveTexture(texture);
}

DELEGATE_TO_GL_2(attachShader, AttachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_3(bindAttribLocation, BindAttribLocation,
                 WebGLId, unsigned long, const char*)

void WebGraphicsContext3DInProcessImpl::bindBuffer(
    unsigned long target, WebGLId buffer) {
  makeContextCurrent();
  if (target == GL_ARRAY_BUFFER)
    bound_array_buffer_ = buffer;
  glBindBuffer(target, buffer);
}

void WebGraphicsContext3DInProcessImpl::bindFramebuffer(
    unsigned long target, WebGLId framebuffer) {
  makeContextCurrent();
  if (!framebuffer)
    framebuffer = (attributes_.antialias ? multisample_fbo_ : fbo_);
  if (framebuffer != bound_fbo_) {
    glBindFramebufferEXT(target, framebuffer);
    bound_fbo_ = framebuffer;
  }
}

DELEGATE_TO_GL_2(bindRenderbuffer, BindRenderbufferEXT, unsigned long, WebGLId)

void WebGraphicsContext3DInProcessImpl::bindTexture(
    unsigned long target, WebGLId texture) {
  makeContextCurrent();
  glBindTexture(target, texture);
  bound_texture_ = texture;
}

DELEGATE_TO_GL_4_C1234(blendColor, BlendColor, double, double, double, double)

DELEGATE_TO_GL_1(blendEquation, BlendEquation, unsigned long)

DELEGATE_TO_GL_2(blendEquationSeparate, BlendEquationSeparate,
                 unsigned long, unsigned long)

DELEGATE_TO_GL_2(blendFunc, BlendFunc, unsigned long, unsigned long)

DELEGATE_TO_GL_4(blendFuncSeparate, BlendFuncSeparate,
                 unsigned long, unsigned long, unsigned long, unsigned long)

DELEGATE_TO_GL_4(bufferData, BufferData,
                 unsigned long, int, const void*, unsigned long)

DELEGATE_TO_GL_4(bufferSubData, BufferSubData,
                 unsigned long, long, int, const void*)

DELEGATE_TO_GL_1R(checkFramebufferStatus, CheckFramebufferStatusEXT,
                  unsigned long, unsigned long)

DELEGATE_TO_GL_1(clear, Clear, unsigned long)

DELEGATE_TO_GL_4_C1234(clearColor, ClearColor, double, double, double, double)

DELEGATE_TO_GL_1(clearDepth, ClearDepth, double)

DELEGATE_TO_GL_1(clearStencil, ClearStencil, long)

DELEGATE_TO_GL_4(colorMask, ColorMask, bool, bool, bool, bool)

void WebGraphicsContext3DInProcessImpl::compileShader(WebGLId shader) {
  makeContextCurrent();

  ShaderSourceMap::iterator result = shader_source_map_.find(shader);
  if (result == shader_source_map_.end()) {
    // Passing down to gl driver to generate the correct error; or the case
    // where the shader deletion is delayed when it's attached to a program.
    glCompileShader(shader);
    return;
  }
  ShaderSourceEntry* entry = result->second;
  DCHECK(entry);

  if (!AngleValidateShaderSource(entry)) {
    // Shader didn't validate; don't move forward with compiling
    // translated source.
    return;
  }

  const char* translated_source = entry->translated_source.get();
  int shader_length = translated_source ? strlen(translated_source) : 0;
  glShaderSource(
      shader, 1, const_cast<const char**>(&translated_source), &shader_length);
  glCompileShader(shader);

#ifndef NDEBUG
  int compileStatus;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
  // DCHECK that ANGLE generated GLSL will be accepted by OpenGL
  DCHECK(compileStatus == GL_TRUE);
#endif
}

void WebGraphicsContext3DInProcessImpl::copyTexImage2D(
    unsigned long target, long level, unsigned long internalformat,
    long x, long y, unsigned long width, unsigned long height, long border) {
  makeContextCurrent();

  bool needsResolve = (attributes_.antialias && bound_fbo_ == multisample_fbo_);
  if (needsResolve) {
    ResolveMultisampledFramebuffer(x, y, width, height);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);
  }

  glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);

  if (needsResolve)
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, bound_fbo_);
}

void WebGraphicsContext3DInProcessImpl::copyTexSubImage2D(
    unsigned long target, long level, long xoffset, long yoffset,
    long x, long y, unsigned long width, unsigned long height) {
  makeContextCurrent();

  bool needsResolve = (attributes_.antialias && bound_fbo_ == multisample_fbo_);
  if (needsResolve) {
    ResolveMultisampledFramebuffer(x, y, width, height);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);
  }

  glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);

  if (needsResolve)
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, bound_fbo_);
}

DELEGATE_TO_GL_1(cullFace, CullFace, unsigned long)

DELEGATE_TO_GL_1(depthFunc, DepthFunc, unsigned long)

DELEGATE_TO_GL_1(depthMask, DepthMask, bool)

DELEGATE_TO_GL_2(depthRange, DepthRange, double, double)

DELEGATE_TO_GL_2(detachShader, DetachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_1(disable, Disable, unsigned long)

void WebGraphicsContext3DInProcessImpl::disableVertexAttribArray(
    unsigned long index) {
  makeContextCurrent();
  if (index < kNumTrackedPointerStates)
    vertex_attrib_pointer_state_[index].enabled = false;
  glDisableVertexAttribArray(index);
}

DELEGATE_TO_GL_3(drawArrays, DrawArrays, unsigned long, long, long)

void WebGraphicsContext3DInProcessImpl::drawElements(
    unsigned long mode, unsigned long count, unsigned long type, long offset) {
  makeContextCurrent();
  glDrawElements(mode, count, type,
                 reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_1(enable, Enable, unsigned long)

void WebGraphicsContext3DInProcessImpl::enableVertexAttribArray(
    unsigned long index) {
  makeContextCurrent();
  if (index < kNumTrackedPointerStates)
    vertex_attrib_pointer_state_[index].enabled = true;
  glEnableVertexAttribArray(index);
}

DELEGATE_TO_GL(finish, Finish)

DELEGATE_TO_GL(flush, Flush)

DELEGATE_TO_GL_4(framebufferRenderbuffer, FramebufferRenderbufferEXT,
                 unsigned long, unsigned long, unsigned long, WebGLId)

DELEGATE_TO_GL_5(framebufferTexture2D, FramebufferTexture2DEXT,
                 unsigned long, unsigned long, unsigned long, WebGLId, long)

DELEGATE_TO_GL_1(frontFace, FrontFace, unsigned long)

void WebGraphicsContext3DInProcessImpl::generateMipmap(unsigned long target) {
  makeContextCurrent();
  if (is_gles2_ || have_ext_framebuffer_object_)
    glGenerateMipmapEXT(target);
  // FIXME: provide alternative code path? This will be unpleasant
  // to implement if glGenerateMipmapEXT is not available -- it will
  // require a texture readback and re-upload.
}

bool WebGraphicsContext3DInProcessImpl::getActiveAttrib(
    WebGLId program, unsigned long index, ActiveInfo& info) {
  makeContextCurrent();
  if (!program) {
    synthesizeGLError(GL_INVALID_VALUE);
    return false;
  }
  GLint max_name_length = -1;
  glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_name_length);
  if (max_name_length < 0)
    return false;
  scoped_array<GLchar> name(new GLchar[max_name_length]);
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  glGetActiveAttrib(program, index, max_name_length,
                    &length, &size, &type, name.get());
  if (size < 0) {
    return false;
  }
  info.name = WebString::fromUTF8(name.get(), length);
  info.type = type;
  info.size = size;
  return true;
}

bool WebGraphicsContext3DInProcessImpl::getActiveUniform(
    WebGLId program, unsigned long index, ActiveInfo& info) {
  makeContextCurrent();
  GLint max_name_length = -1;
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_length);
  if (max_name_length < 0)
    return false;
  scoped_array<GLchar> name(new GLchar[max_name_length]);
  GLsizei length = 0;
  GLint size = -1;
  GLenum type = 0;
  glGetActiveUniform(program, index, max_name_length,
                     &length, &size, &type, name.get());
  if (size < 0) {
    return false;
  }
  info.name = WebString::fromUTF8(name.get(), length);
  info.type = type;
  info.size = size;
  return true;
}

DELEGATE_TO_GL_4(getAttachedShaders, GetAttachedShaders,
                 WebGLId, int, int*, unsigned int*)

DELEGATE_TO_GL_2R(getAttribLocation, GetAttribLocation,
                  WebGLId, const char*, int)

DELEGATE_TO_GL_2(getBooleanv, GetBooleanv,
                 unsigned long, unsigned char*)

DELEGATE_TO_GL_3(getBufferParameteriv, GetBufferParameteriv,
                 unsigned long, unsigned long, int*)

WebGraphicsContext3D::Attributes WebGraphicsContext3DInProcessImpl::
    getContextAttributes() {
  return attributes_;
}

unsigned long WebGraphicsContext3DInProcessImpl::getError() {
  DCHECK(synthetic_errors_list_.size() == synthetic_errors_set_.size());
  if (synthetic_errors_set_.size() > 0) {
    unsigned long error = synthetic_errors_list_.front();
    synthetic_errors_list_.pop_front();
    synthetic_errors_set_.erase(error);
    return error;
  }

  makeContextCurrent();
  return glGetError();
}

bool WebGraphicsContext3DInProcessImpl::isContextLost() {
  return false;
}

DELEGATE_TO_GL_2(getFloatv, GetFloatv, unsigned long, float*)

void WebGraphicsContext3DInProcessImpl::getFramebufferAttachmentParameteriv(
    unsigned long target, unsigned long attachment,
    unsigned long pname, int* value) {
  makeContextCurrent();
  if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
    attachment = GL_DEPTH_ATTACHMENT;  // Or GL_STENCIL_ATTACHMENT;
                                       // either works.
  glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, value);
}

void WebGraphicsContext3DInProcessImpl::getIntegerv(
    unsigned long pname, int* value) {
  makeContextCurrent();
  if (is_gles2_) {
    glGetIntegerv(pname, value);
    return;
  }
  // Need to emulate MAX_FRAGMENT/VERTEX_UNIFORM_VECTORS and
  // MAX_VARYING_VECTORS because desktop GL's corresponding queries
  // return the number of components whereas GLES2 return the number
  // of vectors (each vector has 4 components).  Therefore, the value
  // returned by desktop GL needs to be divided by 4.
  switch (pname) {
  case MAX_FRAGMENT_UNIFORM_VECTORS:
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, value);
    *value /= 4;
    break;
  case MAX_VERTEX_UNIFORM_VECTORS:
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, value);
    *value /= 4;
    break;
  case MAX_VARYING_VECTORS:
    glGetIntegerv(GL_MAX_VARYING_FLOATS, value);
    *value /= 4;
    break;
  default:
    glGetIntegerv(pname, value);
  }
}

DELEGATE_TO_GL_3(getProgramiv, GetProgramiv, WebGLId, unsigned long, int*)

WebString WebGraphicsContext3DInProcessImpl::getProgramInfoLog(
    WebGLId program) {
  makeContextCurrent();
  GLint log_length;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
  if (!log_length)
    return WebString();
  scoped_array<GLchar> log(new GLchar[log_length]);
  GLsizei returned_log_length;
  glGetProgramInfoLog(program, log_length, &returned_log_length, log.get());
  DCHECK(log_length == returned_log_length + 1);
  WebString res = WebString::fromUTF8(log.get(), returned_log_length);
  return res;
}

DELEGATE_TO_GL_3(getRenderbufferParameteriv, GetRenderbufferParameterivEXT,
                 unsigned long, unsigned long, int*)

void WebGraphicsContext3DInProcessImpl::getShaderiv(
    WebGLId shader, unsigned long pname, int* value) {
  makeContextCurrent();

  ShaderSourceMap::iterator result = shader_source_map_.find(shader);
  if (result != shader_source_map_.end()) {
    ShaderSourceEntry* entry = result->second;
    DCHECK(entry);
    switch (pname) {
      case GL_COMPILE_STATUS:
        if (!entry->is_valid) {
          *value = 0;
          return;
        }
        break;
      case GL_INFO_LOG_LENGTH:
        if (!entry->is_valid) {
          *value = entry->log.get() ? strlen(entry->log.get()) : 0;
          if (*value)
            (*value)++;
          return;
        }
        break;
      case GL_SHADER_SOURCE_LENGTH:
        *value = entry->source.get() ? strlen(entry->source.get()) : 0;
        if (*value)
          (*value)++;
        return;
    }
  }

  glGetShaderiv(shader, pname, value);
}

WebString WebGraphicsContext3DInProcessImpl::getShaderInfoLog(WebGLId shader) {
  makeContextCurrent();

  ShaderSourceMap::iterator result = shader_source_map_.find(shader);
  if (result != shader_source_map_.end()) {
    ShaderSourceEntry* entry = result->second;
    DCHECK(entry);
    if (!entry->is_valid) {
      if (!entry->log.get())
        return WebString();
      WebString res = WebString::fromUTF8(
          entry->log.get(), strlen(entry->log.get()));
      return res;
    }
  }

  GLint log_length = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
  if (log_length <= 1)
    return WebString();
  scoped_array<GLchar> log(new GLchar[log_length]);
  GLsizei returned_log_length;
  glGetShaderInfoLog(shader, log_length, &returned_log_length, log.get());
  DCHECK(log_length == returned_log_length + 1);
  WebString res = WebString::fromUTF8(log.get(), returned_log_length);
  return res;
}

WebString WebGraphicsContext3DInProcessImpl::getShaderSource(WebGLId shader) {
  makeContextCurrent();

  ShaderSourceMap::iterator result = shader_source_map_.find(shader);
  if (result != shader_source_map_.end()) {
    ShaderSourceEntry* entry = result->second;
    DCHECK(entry);
    if (!entry->source.get())
      return WebString();
    WebString res = WebString::fromUTF8(
        entry->source.get(), strlen(entry->source.get()));
    return res;
  }

  GLint log_length = 0;
  glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &log_length);
  if (log_length <= 1)
    return WebString();
  scoped_array<GLchar> log(new GLchar[log_length]);
  GLsizei returned_log_length;
  glGetShaderSource(shader, log_length, &returned_log_length, log.get());
  DCHECK(log_length == returned_log_length + 1);
  WebString res = WebString::fromUTF8(log.get(), returned_log_length);
  return res;
}

WebString WebGraphicsContext3DInProcessImpl::getString(unsigned long name) {
  makeContextCurrent();
  std::string result(reinterpret_cast<const char*>(glGetString(name)));
  if (name == GL_EXTENSIONS) {
    // GL_CHROMIUM_copy_texture_to_parent_texture requires the
    // desktopGL-only function glGetTexLevelParameteriv (GLES2
    // doesn't support it).
    if (!is_gles2_)
      result += " GL_CHROMIUM_copy_texture_to_parent_texture";
  }
  return WebString::fromUTF8(result.c_str());
}

DELEGATE_TO_GL_3(getTexParameterfv, GetTexParameterfv,
                 unsigned long, unsigned long, float*)

DELEGATE_TO_GL_3(getTexParameteriv, GetTexParameteriv,
                 unsigned long, unsigned long, int*)

DELEGATE_TO_GL_3(getUniformfv, GetUniformfv, WebGLId, long, float*)

DELEGATE_TO_GL_3(getUniformiv, GetUniformiv, WebGLId, long, int*)

DELEGATE_TO_GL_2R(getUniformLocation, GetUniformLocation,
                  WebGLId, const char*, long)

DELEGATE_TO_GL_3(getVertexAttribfv, GetVertexAttribfv,
                 unsigned long, unsigned long, float*)

DELEGATE_TO_GL_3(getVertexAttribiv, GetVertexAttribiv,
                 unsigned long, unsigned long, int*)

long WebGraphicsContext3DInProcessImpl::getVertexAttribOffset(
    unsigned long index, unsigned long pname) {
  makeContextCurrent();
  void* pointer;
  glGetVertexAttribPointerv(index, pname, &pointer);
  return reinterpret_cast<long>(pointer);
}

DELEGATE_TO_GL_2(hint, Hint, unsigned long, unsigned long)

DELEGATE_TO_GL_1RB(isBuffer, IsBuffer, WebGLId, bool)

DELEGATE_TO_GL_1RB(isEnabled, IsEnabled, unsigned long, bool)

DELEGATE_TO_GL_1RB(isFramebuffer, IsFramebufferEXT, WebGLId, bool)

DELEGATE_TO_GL_1RB(isProgram, IsProgram, WebGLId, bool)

DELEGATE_TO_GL_1RB(isRenderbuffer, IsRenderbufferEXT, WebGLId, bool)

DELEGATE_TO_GL_1RB(isShader, IsShader, WebGLId, bool)

DELEGATE_TO_GL_1RB(isTexture, IsTexture, WebGLId, bool)

DELEGATE_TO_GL_1_C(lineWidth, LineWidth, double)

DELEGATE_TO_GL_1(linkProgram, LinkProgram, WebGLId)

DELEGATE_TO_GL_2(pixelStorei, PixelStorei, unsigned long, long)

DELEGATE_TO_GL_2_C12(polygonOffset, PolygonOffset, double, double)

void WebGraphicsContext3DInProcessImpl::readPixels(
    long x, long y, unsigned long width, unsigned long height,
    unsigned long format, unsigned long type, void* pixels) {
  makeContextCurrent();
  // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
  // all previous rendering calls should be done before reading pixels.
  glFlush();
  bool needs_resolve =
      (attributes_.antialias && bound_fbo_ == multisample_fbo_);
  if (needs_resolve) {
    ResolveMultisampledFramebuffer(x, y, width, height);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_);
    glFlush();
  }

  glReadPixels(x, y, width, height, format, type, pixels);

  if (needs_resolve)
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, bound_fbo_);
}

void WebGraphicsContext3DInProcessImpl::releaseShaderCompiler() {
}

void WebGraphicsContext3DInProcessImpl::renderbufferStorage(
    unsigned long target,
    unsigned long internalformat,
    unsigned long width,
    unsigned long height) {
  makeContextCurrent();
  if (!is_gles2_) {
    switch (internalformat) {
    case GL_DEPTH_STENCIL:
      internalformat = GL_DEPTH24_STENCIL8_EXT;
      break;
    case GL_DEPTH_COMPONENT16:
      internalformat = GL_DEPTH_COMPONENT;
      break;
    case GL_RGBA4:
    case GL_RGB5_A1:
      internalformat = GL_RGBA;
      break;
    case 0x8D62:  // GL_RGB565
      internalformat = GL_RGB;
      break;
    }
  }
  glRenderbufferStorageEXT(target, internalformat, width, height);
}

DELEGATE_TO_GL_2_C1(sampleCoverage, SampleCoverage, double, bool)

DELEGATE_TO_GL_4(scissor, Scissor, long, long, unsigned long, unsigned long)

void WebGraphicsContext3DInProcessImpl::texImage2D(
    unsigned target, unsigned level, unsigned internalFormat,
    unsigned width, unsigned height, unsigned border,
    unsigned format, unsigned type, const void* pixels) {
  if (width && height && !pixels) {
    synthesizeGLError(GL_INVALID_VALUE);
    return;
  }
  makeContextCurrent();
  glTexImage2D(target, level, internalFormat,
               width, height, border, format, type, pixels);
}

void WebGraphicsContext3DInProcessImpl::shaderSource(
    WebGLId shader, const char* source) {
  makeContextCurrent();
  GLint length = source ? strlen(source) : 0;
  ShaderSourceMap::iterator result = shader_source_map_.find(shader);
  if (result != shader_source_map_.end()) {
    ShaderSourceEntry* entry = result->second;
    DCHECK(entry);
    entry->source.reset(new char[length + 1]);
    memcpy(entry->source.get(), source, (length + 1) * sizeof(char));
  } else {
    glShaderSource(shader, 1, &source, &length);
  }
}

DELEGATE_TO_GL_3(stencilFunc, StencilFunc, unsigned long, long, unsigned long)

DELEGATE_TO_GL_4(stencilFuncSeparate, StencilFuncSeparate,
                 unsigned long, unsigned long, long, unsigned long)

DELEGATE_TO_GL_1(stencilMask, StencilMask, unsigned long)

DELEGATE_TO_GL_2(stencilMaskSeparate, StencilMaskSeparate,
                 unsigned long, unsigned long)

DELEGATE_TO_GL_3(stencilOp, StencilOp,
                 unsigned long, unsigned long, unsigned long)

DELEGATE_TO_GL_4(stencilOpSeparate, StencilOpSeparate,
                 unsigned long, unsigned long, unsigned long, unsigned long)

DELEGATE_TO_GL_3(texParameterf, TexParameterf, unsigned, unsigned, float);

DELEGATE_TO_GL_3(texParameteri, TexParameteri, unsigned, unsigned, int);

DELEGATE_TO_GL_9(texSubImage2D, TexSubImage2D,
                 unsigned, unsigned, unsigned, unsigned, unsigned,
                 unsigned, unsigned, unsigned, const void*)

DELEGATE_TO_GL_2(uniform1f, Uniform1f, long, float)

DELEGATE_TO_GL_3(uniform1fv, Uniform1fv, long, int, float*)

DELEGATE_TO_GL_2(uniform1i, Uniform1i, long, int)

DELEGATE_TO_GL_3(uniform1iv, Uniform1iv, long, int, int*)

DELEGATE_TO_GL_3(uniform2f, Uniform2f, long, float, float)

DELEGATE_TO_GL_3(uniform2fv, Uniform2fv, long, int, float*)

DELEGATE_TO_GL_3(uniform2i, Uniform2i, long, int, int)

DELEGATE_TO_GL_3(uniform2iv, Uniform2iv, long, int, int*)

DELEGATE_TO_GL_4(uniform3f, Uniform3f, long, float, float, float)

DELEGATE_TO_GL_3(uniform3fv, Uniform3fv, long, int, float*)

DELEGATE_TO_GL_4(uniform3i, Uniform3i, long, int, int, int)

DELEGATE_TO_GL_3(uniform3iv, Uniform3iv, long, int, int*)

DELEGATE_TO_GL_5(uniform4f, Uniform4f, long, float, float, float, float)

DELEGATE_TO_GL_3(uniform4fv, Uniform4fv, long, int, float*)

DELEGATE_TO_GL_5(uniform4i, Uniform4i, long, int, int, int, int)

DELEGATE_TO_GL_3(uniform4iv, Uniform4iv, long, int, int*)

DELEGATE_TO_GL_4(uniformMatrix2fv, UniformMatrix2fv,
                 long, int, bool, const float*)

DELEGATE_TO_GL_4(uniformMatrix3fv, UniformMatrix3fv,
                 long, int, bool, const float*)

DELEGATE_TO_GL_4(uniformMatrix4fv, UniformMatrix4fv,
                 long, int, bool, const float*)

DELEGATE_TO_GL_1(useProgram, UseProgram, WebGLId)

DELEGATE_TO_GL_1(validateProgram, ValidateProgram, WebGLId)

DELEGATE_TO_GL_2(vertexAttrib1f, VertexAttrib1f, unsigned long, float)

DELEGATE_TO_GL_2(vertexAttrib1fv, VertexAttrib1fv, unsigned long, const float*)

DELEGATE_TO_GL_3(vertexAttrib2f, VertexAttrib2f, unsigned long, float, float)

DELEGATE_TO_GL_2(vertexAttrib2fv, VertexAttrib2fv, unsigned long, const float*)

DELEGATE_TO_GL_4(vertexAttrib3f, VertexAttrib3f,
                 unsigned long, float, float, float)

DELEGATE_TO_GL_2(vertexAttrib3fv, VertexAttrib3fv, unsigned long, const float*)

DELEGATE_TO_GL_5(vertexAttrib4f, VertexAttrib4f,
                 unsigned long, float, float, float, float)

DELEGATE_TO_GL_2(vertexAttrib4fv, VertexAttrib4fv, unsigned long, const float*)

void WebGraphicsContext3DInProcessImpl::vertexAttribPointer(
    unsigned long indx, int size, int type, bool normalized,
    unsigned long stride, unsigned long offset) {
  makeContextCurrent();

  if (bound_array_buffer_ <= 0) {
    // FIXME: raise exception.
    // LogMessagef(("bufferData: no buffer bound"));
    return;
  }

  if (indx < kNumTrackedPointerStates) {
    VertexAttribPointerState& state = vertex_attrib_pointer_state_[indx];
    state.buffer = bound_array_buffer_;
    state.indx = indx;
    state.size = size;
    state.type = type;
    state.normalized = normalized;
    state.stride = stride;
    state.offset = offset;
  }

  glVertexAttribPointer(indx, size, type, normalized, stride,
                        reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_4(viewport, Viewport, long, long, unsigned long, unsigned long)

unsigned WebGraphicsContext3DInProcessImpl::createBuffer() {
  makeContextCurrent();
  GLuint o;
  glGenBuffersARB(1, &o);
  return o;
}

unsigned WebGraphicsContext3DInProcessImpl::createFramebuffer() {
  makeContextCurrent();
  GLuint o = 0;
  glGenFramebuffersEXT(1, &o);
  return o;
}

unsigned WebGraphicsContext3DInProcessImpl::createProgram() {
  makeContextCurrent();
  return glCreateProgram();
}

unsigned WebGraphicsContext3DInProcessImpl::createRenderbuffer() {
  makeContextCurrent();
  GLuint o;
  glGenRenderbuffersEXT(1, &o);
  return o;
}

unsigned WebGraphicsContext3DInProcessImpl::createShader(
    unsigned long shaderType) {
  makeContextCurrent();
  DCHECK(shaderType == GL_VERTEX_SHADER || shaderType == GL_FRAGMENT_SHADER);
  unsigned shader = glCreateShader(shaderType);
  if (shader) {
    ShaderSourceMap::iterator result = shader_source_map_.find(shader);
    if (result != shader_source_map_.end()) {
      delete result->second;
      shader_source_map_.erase(result);
    }
    shader_source_map_.insert(
        ShaderSourceMap::value_type(shader, new ShaderSourceEntry(shaderType)));
  }

  return shader;
}

unsigned WebGraphicsContext3DInProcessImpl::createTexture() {
  makeContextCurrent();
  GLuint o;
  glGenTextures(1, &o);
  return o;
}

void WebGraphicsContext3DInProcessImpl::deleteBuffer(unsigned buffer) {
  makeContextCurrent();
  glDeleteBuffersARB(1, &buffer);
}

void WebGraphicsContext3DInProcessImpl::deleteFramebuffer(
    unsigned framebuffer) {
  makeContextCurrent();
  glDeleteFramebuffersEXT(1, &framebuffer);
}

void WebGraphicsContext3DInProcessImpl::deleteProgram(unsigned program) {
  makeContextCurrent();
  glDeleteProgram(program);
}

void WebGraphicsContext3DInProcessImpl::deleteRenderbuffer(
    unsigned renderbuffer) {
  makeContextCurrent();
  glDeleteRenderbuffersEXT(1, &renderbuffer);
}

void WebGraphicsContext3DInProcessImpl::deleteShader(unsigned shader) {
  makeContextCurrent();

  ShaderSourceMap::iterator result = shader_source_map_.find(shader);
  if (result != shader_source_map_.end()) {
    delete result->second;
    shader_source_map_.erase(result);
  }
  glDeleteShader(shader);
}

void WebGraphicsContext3DInProcessImpl::deleteTexture(unsigned texture) {
  makeContextCurrent();
  glDeleteTextures(1, &texture);
}

bool WebGraphicsContext3DInProcessImpl::AngleCreateCompilers() {
  if (!ShInitialize())
    return false;

  ShBuiltInResources resources;
  ShInitBuiltInResources(&resources);
  getIntegerv(GL_MAX_VERTEX_ATTRIBS, &resources.MaxVertexAttribs);
  getIntegerv(MAX_VERTEX_UNIFORM_VECTORS, &resources.MaxVertexUniformVectors);
  getIntegerv(MAX_VARYING_VECTORS, &resources.MaxVaryingVectors);
  getIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,
              &resources.MaxVertexTextureImageUnits);
  getIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
              &resources.MaxCombinedTextureImageUnits);
  getIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &resources.MaxTextureImageUnits);
  getIntegerv(MAX_FRAGMENT_UNIFORM_VECTORS,
              &resources.MaxFragmentUniformVectors);
  // Always set to 1 for OpenGL ES.
  resources.MaxDrawBuffers = 1;

  fragment_compiler_ = ShConstructCompiler(
      SH_FRAGMENT_SHADER, SH_WEBGL_SPEC, &resources);
  vertex_compiler_ = ShConstructCompiler(
      SH_VERTEX_SHADER, SH_WEBGL_SPEC, &resources);
  return (fragment_compiler_ && vertex_compiler_);
}

void WebGraphicsContext3DInProcessImpl::AngleDestroyCompilers() {
  if (fragment_compiler_) {
    ShDestruct(fragment_compiler_);
    fragment_compiler_ = 0;
  }
  if (vertex_compiler_) {
    ShDestruct(vertex_compiler_);
    vertex_compiler_ = 0;
  }
}

bool WebGraphicsContext3DInProcessImpl::AngleValidateShaderSource(
    ShaderSourceEntry* entry) {
  entry->is_valid = false;
  entry->translated_source.reset();
  entry->log.reset();

  ShHandle compiler = 0;
  switch (entry->type) {
    case GL_FRAGMENT_SHADER:
      compiler = fragment_compiler_;
      break;
    case GL_VERTEX_SHADER:
      compiler = vertex_compiler_;
      break;
  }
  if (!compiler)
    return false;

  char* source = entry->source.get();
  if (!ShCompile(compiler, &source, 1, SH_OBJECT_CODE)) {
    int logSize = 0;
    ShGetInfo(compiler, SH_INFO_LOG_LENGTH, &logSize);
    if (logSize > 1) {
      entry->log.reset(new char[logSize]);
      ShGetInfoLog(compiler, entry->log.get());
    }
    return false;
  }

  int length = 0;
  if (is_gles2_) {
    // ANGLE does not yet have a GLSL ES backend. Therefore if the
    // compile succeeds we send the original source down.
    length = strlen(entry->source.get());
    if (length > 0)
      ++length;  // Add null terminator
  } else {
    ShGetInfo(compiler, SH_OBJECT_CODE_LENGTH, &length);
  }
  if (length > 1) {
    entry->translated_source.reset(new char[length]);
    if (is_gles2_)
      strncpy(entry->translated_source.get(), entry->source.get(), length);
    else
      ShGetObjectCode(compiler, entry->translated_source.get());
  }
  entry->is_valid = true;
  return true;
}

}  // namespace gpu
}  // namespace webkit

