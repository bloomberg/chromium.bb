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

namespace webkit {
namespace gpu {

enum {
  MAX_VERTEX_UNIFORM_VECTORS = 0x8DFB,
  MAX_VARYING_VECTORS = 0x8DFC,
  MAX_FRAGMENT_UNIFORM_VECTORS = 0x8DFD
};

struct WebGraphicsContext3DInProcessImpl::ShaderSourceEntry {
  explicit ShaderSourceEntry(WGC3Denum shader_type)
      : type(shader_type),
        is_valid(false) {
  }

  WGC3Denum type;
  scoped_array<char> source;
  scoped_array<char> log;
  scoped_array<char> translated_source;
  bool is_valid;
};

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
    WGC3Dint x, WGC3Dint y, WGC3Dsizei width, WGC3Dsizei height) {
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

bool WebGraphicsContext3DInProcessImpl::isGLES2Compliant() {
  return is_gles2_;
}

WebGLId WebGraphicsContext3DInProcessImpl::getPlatformTextureId() {
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

void WebGraphicsContext3DInProcessImpl::synthesizeGLError(WGC3Denum error) {
  if (synthetic_errors_set_.find(error) == synthetic_errors_set_.end()) {
    synthetic_errors_set_.insert(error);
    synthetic_errors_list_.push_back(error);
  }
}

void* WebGraphicsContext3DInProcessImpl::mapBufferSubDataCHROMIUM(
    WGC3Denum target, WGC3Dintptr offset,
    WGC3Dsizeiptr size, WGC3Denum access) {
  return 0;
}

void WebGraphicsContext3DInProcessImpl::unmapBufferSubDataCHROMIUM(
    const void* mem) {
}

void* WebGraphicsContext3DInProcessImpl::mapTexSubImage2DCHROMIUM(
    WGC3Denum target, WGC3Dint level, WGC3Dint xoffset, WGC3Dint yoffset,
    WGC3Dsizei width, WGC3Dsizei height, WGC3Denum format, WGC3Denum type,
    WGC3Denum access) {
  return 0;
}

void WebGraphicsContext3DInProcessImpl::unmapTexSubImage2DCHROMIUM(
    const void* mem) {
}

void WebGraphicsContext3DInProcessImpl::copyTextureToParentTextureCHROMIUM(
    WebGLId id, WebGLId id2) {
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
    WGC3Dint srcX0, WGC3Dint srcY0, WGC3Dint srcX1, WGC3Dint srcY1,
    WGC3Dint dstX0, WGC3Dint dstY0, WGC3Dint dstX1, WGC3Dint dstY1,
    WGC3Dbitfield mask, WGC3Denum filter) {
}

void WebGraphicsContext3DInProcessImpl::renderbufferStorageMultisampleCHROMIUM(
    WGC3Denum target, WGC3Dsizei samples, WGC3Denum internalformat,
    WGC3Dsizei width, WGC3Dsizei height) {
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

void WebGraphicsContext3DInProcessImpl::activeTexture(WGC3Denum texture) {
  // FIXME: query number of textures available.
  if (texture < GL_TEXTURE0 || texture > GL_TEXTURE0+32)
    // FIXME: raise exception.
    return;

  makeContextCurrent();
  glActiveTexture(texture);
}

DELEGATE_TO_GL_2(attachShader, AttachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_3(bindAttribLocation, BindAttribLocation,
                 WebGLId, WGC3Duint, const WGC3Dchar*)

DELEGATE_TO_GL_2(bindBuffer, BindBuffer, WGC3Denum, WebGLId);

void WebGraphicsContext3DInProcessImpl::bindFramebuffer(
    WGC3Denum target, WebGLId framebuffer) {
  makeContextCurrent();
  if (!framebuffer)
    framebuffer = (attributes_.antialias ? multisample_fbo_ : fbo_);
  if (framebuffer != bound_fbo_) {
    glBindFramebufferEXT(target, framebuffer);
    bound_fbo_ = framebuffer;
  }
}

DELEGATE_TO_GL_2(bindRenderbuffer, BindRenderbufferEXT, WGC3Denum, WebGLId)

void WebGraphicsContext3DInProcessImpl::bindTexture(
    WGC3Denum target, WebGLId texture) {
  makeContextCurrent();
  glBindTexture(target, texture);
  bound_texture_ = texture;
}

DELEGATE_TO_GL_4(blendColor, BlendColor,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_1(blendEquation, BlendEquation, WGC3Denum)

DELEGATE_TO_GL_2(blendEquationSeparate, BlendEquationSeparate,
                 WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_2(blendFunc, BlendFunc, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_4(blendFuncSeparate, BlendFuncSeparate,
                 WGC3Denum, WGC3Denum, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_4(bufferData, BufferData,
                 WGC3Denum, WGC3Dsizeiptr, const void*, WGC3Denum)

DELEGATE_TO_GL_4(bufferSubData, BufferSubData,
                 WGC3Denum, WGC3Dintptr, WGC3Dsizeiptr, const void*)

DELEGATE_TO_GL_1R(checkFramebufferStatus, CheckFramebufferStatusEXT,
                  WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_1(clear, Clear, WGC3Dbitfield)

DELEGATE_TO_GL_4(clearColor, ClearColor,
                 WGC3Dclampf, WGC3Dclampf, WGC3Dclampf, WGC3Dclampf)

DELEGATE_TO_GL_1(clearDepth, ClearDepth, WGC3Dclampf)

DELEGATE_TO_GL_1(clearStencil, ClearStencil, WGC3Dint)

DELEGATE_TO_GL_4(colorMask, ColorMask,
                 WGC3Dboolean, WGC3Dboolean, WGC3Dboolean, WGC3Dboolean)

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
    WGC3Denum target, WGC3Dint level, WGC3Denum internalformat, WGC3Dint x,
    WGC3Dint y, WGC3Dsizei width, WGC3Dsizei height, WGC3Dint border) {
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
    WGC3Denum target, WGC3Dint level, WGC3Dint xoffset, WGC3Dint yoffset,
    WGC3Dint x, WGC3Dint y, WGC3Dsizei width, WGC3Dsizei height) {
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

DELEGATE_TO_GL_1(cullFace, CullFace, WGC3Denum)

DELEGATE_TO_GL_1(depthFunc, DepthFunc, WGC3Denum)

DELEGATE_TO_GL_1(depthMask, DepthMask, WGC3Dboolean)

DELEGATE_TO_GL_2(depthRange, DepthRange, WGC3Dclampf, WGC3Dclampf)

DELEGATE_TO_GL_2(detachShader, DetachShader, WebGLId, WebGLId)

DELEGATE_TO_GL_1(disable, Disable, WGC3Denum)

DELEGATE_TO_GL_1(disableVertexAttribArray, DisableVertexAttribArray, WGC3Duint)

DELEGATE_TO_GL_3(drawArrays, DrawArrays, WGC3Denum, WGC3Dint, WGC3Dsizei)

void WebGraphicsContext3DInProcessImpl::drawElements(
    WGC3Denum mode, WGC3Dsizei count, WGC3Denum type, WGC3Dintptr offset) {
  makeContextCurrent();
  glDrawElements(mode, count, type,
                 reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_1(enable, Enable, WGC3Denum)

DELEGATE_TO_GL_1(enableVertexAttribArray, EnableVertexAttribArray, WGC3Duint)

DELEGATE_TO_GL(finish, Finish)

DELEGATE_TO_GL(flush, Flush)

DELEGATE_TO_GL_4(framebufferRenderbuffer, FramebufferRenderbufferEXT,
                 WGC3Denum, WGC3Denum, WGC3Denum, WebGLId)

DELEGATE_TO_GL_5(framebufferTexture2D, FramebufferTexture2DEXT,
                 WGC3Denum, WGC3Denum, WGC3Denum, WebGLId, WGC3Dint)

DELEGATE_TO_GL_1(frontFace, FrontFace, WGC3Denum)

void WebGraphicsContext3DInProcessImpl::generateMipmap(WGC3Denum target) {
  makeContextCurrent();
  if (is_gles2_ || have_ext_framebuffer_object_)
    glGenerateMipmapEXT(target);
  // FIXME: provide alternative code path? This will be unpleasant
  // to implement if glGenerateMipmapEXT is not available -- it will
  // require a texture readback and re-upload.
}

bool WebGraphicsContext3DInProcessImpl::getActiveAttrib(
    WebGLId program, WGC3Duint index, ActiveInfo& info) {
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
    WebGLId program, WGC3Duint index, ActiveInfo& info) {
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
                 WebGLId, WGC3Dsizei, WGC3Dsizei*, WebGLId*)

DELEGATE_TO_GL_2R(getAttribLocation, GetAttribLocation,
                  WebGLId, const WGC3Dchar*, WGC3Dint)

DELEGATE_TO_GL_2(getBooleanv, GetBooleanv,
                 WGC3Denum, WGC3Dboolean*)

DELEGATE_TO_GL_3(getBufferParameteriv, GetBufferParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Dint*)

WebGraphicsContext3D::Attributes WebGraphicsContext3DInProcessImpl::
    getContextAttributes() {
  return attributes_;
}

WGC3Denum WebGraphicsContext3DInProcessImpl::getError() {
  DCHECK(synthetic_errors_list_.size() == synthetic_errors_set_.size());
  if (synthetic_errors_set_.size() > 0) {
    WGC3Denum error = synthetic_errors_list_.front();
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

DELEGATE_TO_GL_2(getFloatv, GetFloatv, WGC3Denum, WGC3Dfloat*)

void WebGraphicsContext3DInProcessImpl::getFramebufferAttachmentParameteriv(
    WGC3Denum target, WGC3Denum attachment,
    WGC3Denum pname, WGC3Dint* value) {
  makeContextCurrent();
  if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
    attachment = GL_DEPTH_ATTACHMENT;  // Or GL_STENCIL_ATTACHMENT;
                                       // either works.
  glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, value);
}

void WebGraphicsContext3DInProcessImpl::getIntegerv(
    WGC3Denum pname, WGC3Dint* value) {
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

DELEGATE_TO_GL_3(getProgramiv, GetProgramiv, WebGLId, WGC3Denum, WGC3Dint*)

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
                 WGC3Denum, WGC3Denum, WGC3Dint*)

void WebGraphicsContext3DInProcessImpl::getShaderiv(
    WebGLId shader, WGC3Denum pname, WGC3Dint* value) {
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

WebString WebGraphicsContext3DInProcessImpl::getString(WGC3Denum name) {
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
                 WGC3Denum, WGC3Denum, WGC3Dfloat*)

DELEGATE_TO_GL_3(getTexParameteriv, GetTexParameteriv,
                 WGC3Denum, WGC3Denum, WGC3Dint*)

DELEGATE_TO_GL_3(getUniformfv, GetUniformfv, WebGLId, WGC3Dint, WGC3Dfloat*)

DELEGATE_TO_GL_3(getUniformiv, GetUniformiv, WebGLId, WGC3Dint, WGC3Dint*)

DELEGATE_TO_GL_2R(getUniformLocation, GetUniformLocation,
                  WebGLId, const WGC3Dchar*, WGC3Dint)

DELEGATE_TO_GL_3(getVertexAttribfv, GetVertexAttribfv,
                 WGC3Duint, WGC3Denum, WGC3Dfloat*)

DELEGATE_TO_GL_3(getVertexAttribiv, GetVertexAttribiv,
                 WGC3Duint, WGC3Denum, WGC3Dint*)

WGC3Dsizeiptr WebGraphicsContext3DInProcessImpl::getVertexAttribOffset(
    WGC3Duint index, WGC3Denum pname) {
  makeContextCurrent();
  void* pointer;
  glGetVertexAttribPointerv(index, pname, &pointer);
  return static_cast<WGC3Dsizeiptr>(reinterpret_cast<intptr_t>(pointer));
}

DELEGATE_TO_GL_2(hint, Hint, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_1RB(isBuffer, IsBuffer, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isEnabled, IsEnabled, WGC3Denum, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isFramebuffer, IsFramebufferEXT, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isProgram, IsProgram, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isRenderbuffer, IsRenderbufferEXT, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isShader, IsShader, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1RB(isTexture, IsTexture, WebGLId, WGC3Dboolean)

DELEGATE_TO_GL_1(lineWidth, LineWidth, WGC3Dfloat)

DELEGATE_TO_GL_1(linkProgram, LinkProgram, WebGLId)

DELEGATE_TO_GL_2(pixelStorei, PixelStorei, WGC3Denum, WGC3Dint)

DELEGATE_TO_GL_2(polygonOffset, PolygonOffset, WGC3Dfloat, WGC3Dfloat)

void WebGraphicsContext3DInProcessImpl::readPixels(
    WGC3Dint x, WGC3Dint y, WGC3Dsizei width, WGC3Dsizei height,
    WGC3Denum format, WGC3Denum type, void* pixels) {
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
    WGC3Denum target,
    WGC3Denum internalformat,
    WGC3Dsizei width,
    WGC3Dsizei height) {
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

DELEGATE_TO_GL_2(sampleCoverage, SampleCoverage, WGC3Dclampf, WGC3Dboolean)

DELEGATE_TO_GL_4(scissor, Scissor, WGC3Dint, WGC3Dint, WGC3Dsizei, WGC3Dsizei)

void WebGraphicsContext3DInProcessImpl::texImage2D(
    WGC3Denum target, WGC3Dint level, WGC3Denum internalFormat,
    WGC3Dsizei width, WGC3Dsizei height, WGC3Dint border,
    WGC3Denum format, WGC3Denum type, const void* pixels) {
  if (width && height && !pixels) {
    synthesizeGLError(GL_INVALID_VALUE);
    return;
  }
  makeContextCurrent();
  glTexImage2D(target, level, internalFormat,
               width, height, border, format, type, pixels);
}

void WebGraphicsContext3DInProcessImpl::shaderSource(
    WebGLId shader, const WGC3Dchar* source) {
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

DELEGATE_TO_GL_3(stencilFunc, StencilFunc, WGC3Denum, WGC3Dint, WGC3Duint)

DELEGATE_TO_GL_4(stencilFuncSeparate, StencilFuncSeparate,
                 WGC3Denum, WGC3Denum, WGC3Dint, WGC3Duint)

DELEGATE_TO_GL_1(stencilMask, StencilMask, WGC3Duint)

DELEGATE_TO_GL_2(stencilMaskSeparate, StencilMaskSeparate,
                 WGC3Denum, WGC3Duint)

DELEGATE_TO_GL_3(stencilOp, StencilOp,
                 WGC3Denum, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_4(stencilOpSeparate, StencilOpSeparate,
                 WGC3Denum, WGC3Denum, WGC3Denum, WGC3Denum)

DELEGATE_TO_GL_3(texParameterf, TexParameterf, WGC3Denum, WGC3Denum, WGC3Dfloat)

DELEGATE_TO_GL_3(texParameteri, TexParameteri, WGC3Denum, WGC3Denum, WGC3Dint)

DELEGATE_TO_GL_9(texSubImage2D, TexSubImage2D,
                 WGC3Denum, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dsizei,
                 WGC3Dsizei, WGC3Denum, WGC3Denum, const void*)

DELEGATE_TO_GL_2(uniform1f, Uniform1f, WGC3Dint, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform1fv, Uniform1fv,
                 WGC3Dint, WGC3Dsizei, const WGC3Dfloat*)

DELEGATE_TO_GL_2(uniform1i, Uniform1i, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform1iv, Uniform1iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_3(uniform2f, Uniform2f, WGC3Dint, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform2fv, Uniform2fv,
                 WGC3Dint, WGC3Dsizei, const WGC3Dfloat*)

DELEGATE_TO_GL_3(uniform2i, Uniform2i, WGC3Dint, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform2iv, Uniform2iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_4(uniform3f, Uniform3f,
                 WGC3Dint, WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform3fv, Uniform3fv,
                 WGC3Dint, WGC3Dsizei, const WGC3Dfloat*)

DELEGATE_TO_GL_4(uniform3i, Uniform3i, WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform3iv, Uniform3iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_5(uniform4f, Uniform4f, WGC3Dint,
                 WGC3Dfloat, WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_3(uniform4fv, Uniform4fv,
                 WGC3Dint, WGC3Dsizei, const WGC3Dfloat*)

DELEGATE_TO_GL_5(uniform4i, Uniform4i, WGC3Dint,
                 WGC3Dint, WGC3Dint, WGC3Dint, WGC3Dint)

DELEGATE_TO_GL_3(uniform4iv, Uniform4iv, WGC3Dint, WGC3Dsizei, const WGC3Dint*)

DELEGATE_TO_GL_4(uniformMatrix2fv, UniformMatrix2fv,
                 WGC3Dint, WGC3Dsizei, WGC3Dboolean, const WGC3Dfloat*)

DELEGATE_TO_GL_4(uniformMatrix3fv, UniformMatrix3fv,
                 WGC3Dint, WGC3Dsizei, WGC3Dboolean, const WGC3Dfloat*)

DELEGATE_TO_GL_4(uniformMatrix4fv, UniformMatrix4fv,
                 WGC3Dint, WGC3Dsizei, WGC3Dboolean, const WGC3Dfloat*)

DELEGATE_TO_GL_1(useProgram, UseProgram, WebGLId)

DELEGATE_TO_GL_1(validateProgram, ValidateProgram, WebGLId)

DELEGATE_TO_GL_2(vertexAttrib1f, VertexAttrib1f, WGC3Duint, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib1fv, VertexAttrib1fv, WGC3Duint, const WGC3Dfloat*)

DELEGATE_TO_GL_3(vertexAttrib2f, VertexAttrib2f,
                 WGC3Duint, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib2fv, VertexAttrib2fv, WGC3Duint, const WGC3Dfloat*)

DELEGATE_TO_GL_4(vertexAttrib3f, VertexAttrib3f,
                 WGC3Duint, WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib3fv, VertexAttrib3fv, WGC3Duint, const WGC3Dfloat*)

DELEGATE_TO_GL_5(vertexAttrib4f, VertexAttrib4f,
                 WGC3Duint, WGC3Dfloat, WGC3Dfloat, WGC3Dfloat, WGC3Dfloat)

DELEGATE_TO_GL_2(vertexAttrib4fv, VertexAttrib4fv, WGC3Duint, const WGC3Dfloat*)

void WebGraphicsContext3DInProcessImpl::vertexAttribPointer(
    WGC3Duint index, WGC3Dint size, WGC3Denum type, WGC3Dboolean normalized,
    WGC3Dsizei stride, WGC3Dintptr offset) {
  makeContextCurrent();
  glVertexAttribPointer(index, size, type, normalized, stride,
                        reinterpret_cast<void*>(static_cast<intptr_t>(offset)));
}

DELEGATE_TO_GL_4(viewport, Viewport, WGC3Dint, WGC3Dint, WGC3Dsizei, WGC3Dsizei)

WebGLId WebGraphicsContext3DInProcessImpl::createBuffer() {
  makeContextCurrent();
  GLuint o;
  glGenBuffersARB(1, &o);
  return o;
}

WebGLId WebGraphicsContext3DInProcessImpl::createFramebuffer() {
  makeContextCurrent();
  GLuint o = 0;
  glGenFramebuffersEXT(1, &o);
  return o;
}

WebGLId WebGraphicsContext3DInProcessImpl::createProgram() {
  makeContextCurrent();
  return glCreateProgram();
}

WebGLId WebGraphicsContext3DInProcessImpl::createRenderbuffer() {
  makeContextCurrent();
  GLuint o;
  glGenRenderbuffersEXT(1, &o);
  return o;
}

WebGLId WebGraphicsContext3DInProcessImpl::createShader(
    WGC3Denum shaderType) {
  makeContextCurrent();
  DCHECK(shaderType == GL_VERTEX_SHADER || shaderType == GL_FRAGMENT_SHADER);
  GLuint shader = glCreateShader(shaderType);
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

WebGLId WebGraphicsContext3DInProcessImpl::createTexture() {
  makeContextCurrent();
  GLuint o;
  glGenTextures(1, &o);
  return o;
}

void WebGraphicsContext3DInProcessImpl::deleteBuffer(WebGLId buffer) {
  makeContextCurrent();
  glDeleteBuffersARB(1, &buffer);
}

void WebGraphicsContext3DInProcessImpl::deleteFramebuffer(
    WebGLId framebuffer) {
  makeContextCurrent();
  glDeleteFramebuffersEXT(1, &framebuffer);
}

void WebGraphicsContext3DInProcessImpl::deleteProgram(WebGLId program) {
  makeContextCurrent();
  glDeleteProgram(program);
}

void WebGraphicsContext3DInProcessImpl::deleteRenderbuffer(
    WebGLId renderbuffer) {
  makeContextCurrent();
  glDeleteRenderbuffersEXT(1, &renderbuffer);
}

void WebGraphicsContext3DInProcessImpl::deleteShader(WebGLId shader) {
  makeContextCurrent();

  ShaderSourceMap::iterator result = shader_source_map_.find(shader);
  if (result != shader_source_map_.end()) {
    delete result->second;
    shader_source_map_.erase(result);
  }
  glDeleteShader(shader);
}

void WebGraphicsContext3DInProcessImpl::deleteTexture(WebGLId texture) {
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

