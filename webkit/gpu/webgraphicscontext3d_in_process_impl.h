// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GPU_WEBGRAPHICSCONTEXT3D_IN_PROCESS_IMPL_H_
#define WEBKIT_GPU_WEBGRAPHICSCONTEXT3D_IN_PROCESS_IMPL_H_

#include <list>
#include <set>

#include "base/hash_tables.h"
#include "base/scoped_ptr.h"
#include "third_party/angle/include/GLSLANG/ShaderLang.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

#if !defined(OS_MACOSX)
#define FLIP_FRAMEBUFFER_VERTICALLY
#endif
namespace gfx {
class GLContext;
}

using WebKit::WGC3Dchar;
using WebKit::WGC3Denum;
using WebKit::WGC3Dboolean;
using WebKit::WGC3Dbitfield;
using WebKit::WGC3Dint;
using WebKit::WGC3Dsizei;
using WebKit::WGC3Duint;
using WebKit::WGC3Dfloat;
using WebKit::WGC3Dclampf;
using WebKit::WGC3Dintptr;
using WebKit::WGC3Dsizeiptr;

using WebKit::WebGLId;

using WebKit::WebString;
using WebKit::WebView;

using WebKit::WebGraphicsContext3D;

namespace webkit {
namespace gpu {

// The default implementation of WebGL. In Chromium, using this class
// requires the sandbox to be disabled, which is strongly discouraged.
// It is provided for support of test_shell and any Chromium ports
// where an in-renderer WebGL implementation would be helpful.

class WebGraphicsContext3DInProcessImpl : public WebGraphicsContext3D {
 public:
  WebGraphicsContext3DInProcessImpl();
  virtual ~WebGraphicsContext3DInProcessImpl();

  //----------------------------------------------------------------------
  // WebGraphicsContext3D methods
  virtual bool initialize(
      WebGraphicsContext3D::Attributes attributes, WebView*, bool);
  virtual bool makeContextCurrent();

  virtual int width();
  virtual int height();

  virtual bool isGLES2Compliant();

  virtual void reshape(int width, int height);

  virtual bool readBackFramebuffer(unsigned char* pixels, size_t bufferSize);

  virtual WebGLId getPlatformTextureId();
  virtual void prepareTexture();

  virtual void synthesizeGLError(WGC3Denum error);
  virtual void* mapBufferSubDataCHROMIUM(WGC3Denum target, WGC3Dintptr offset,
                                         WGC3Dsizeiptr size, WGC3Denum access);
  virtual void unmapBufferSubDataCHROMIUM(const void*);
  virtual void* mapTexSubImage2DCHROMIUM(
      WGC3Denum target,
      WGC3Dint level,
      WGC3Dint xoffset,
      WGC3Dint yoffset,
      WGC3Dsizei width,
      WGC3Dsizei height,
      WGC3Denum format,
      WGC3Denum type,
      WGC3Denum access);
  virtual void unmapTexSubImage2DCHROMIUM(const void*);
  virtual void copyTextureToParentTextureCHROMIUM(
      WebGLId texture, WebGLId parentTexture);

  virtual WebString getRequestableExtensionsCHROMIUM();
  virtual void requestExtensionCHROMIUM(const char*);

  virtual void blitFramebufferCHROMIUM(
      WGC3Dint srcX0, WGC3Dint srcY0, WGC3Dint srcX1, WGC3Dint srcY1,
      WGC3Dint dstX0, WGC3Dint dstY0, WGC3Dint dstX1, WGC3Dint dstY1,
      WGC3Dbitfield mask, WGC3Denum filter);
  virtual void renderbufferStorageMultisampleCHROMIUM(
      WGC3Denum target, WGC3Dsizei samples, WGC3Denum internalformat,
      WGC3Dsizei width, WGC3Dsizei height);

  virtual void activeTexture(WGC3Denum texture);
  virtual void attachShader(WebGLId program, WebGLId shader);
  virtual void bindAttribLocation(
      WebGLId program, WGC3Duint index, const WGC3Dchar* name);
  virtual void bindBuffer(WGC3Denum target, WebGLId buffer);
  virtual void bindFramebuffer(WGC3Denum target, WebGLId framebuffer);
  virtual void bindRenderbuffer(
      WGC3Denum target, WebGLId renderbuffer);
  virtual void bindTexture(WGC3Denum target, WebGLId texture);
  virtual void blendColor(
      WGC3Dclampf red, WGC3Dclampf green, WGC3Dclampf blue, WGC3Dclampf alpha);
  virtual void blendEquation(WGC3Denum mode);
  virtual void blendEquationSeparate(WGC3Denum modeRGB, WGC3Denum modeAlpha);
  virtual void blendFunc(WGC3Denum sfactor, WGC3Denum dfactor);
  virtual void blendFuncSeparate(WGC3Denum srcRGB, WGC3Denum dstRGB,
                                 WGC3Denum srcAlpha, WGC3Denum dstAlpha);

  virtual void bufferData(
      WGC3Denum target, WGC3Dsizeiptr size, const void* data, WGC3Denum usage);
  virtual void bufferSubData(WGC3Denum target, WGC3Dintptr offset,
                             WGC3Dsizeiptr size, const void* data);

  virtual WGC3Denum checkFramebufferStatus(WGC3Denum target);
  virtual void clear(WGC3Dbitfield mask);
  virtual void clearColor(
      WGC3Dclampf red, WGC3Dclampf green, WGC3Dclampf blue, WGC3Dclampf alpha);
  virtual void clearDepth(WGC3Dclampf depth);
  virtual void clearStencil(WGC3Dint s);
  virtual void colorMask(WGC3Dboolean red, WGC3Dboolean green,
                         WGC3Dboolean blue, WGC3Dboolean alpha);
  virtual void compileShader(WebGLId shader);

  virtual void copyTexImage2D(
      WGC3Denum target,
      WGC3Dint level,
      WGC3Denum internalformat,
      WGC3Dint x,
      WGC3Dint y,
      WGC3Dsizei width,
      WGC3Dsizei height,
      WGC3Dint border);
  virtual void copyTexSubImage2D(
      WGC3Denum target,
      WGC3Dint level,
      WGC3Dint xoffset,
      WGC3Dint yoffset,
      WGC3Dint x,
      WGC3Dint y,
      WGC3Dsizei width,
      WGC3Dsizei height);
  virtual void cullFace(WGC3Denum mode);
  virtual void depthFunc(WGC3Denum func);
  virtual void depthMask(WGC3Dboolean flag);
  virtual void depthRange(WGC3Dclampf zNear, WGC3Dclampf zFar);
  virtual void detachShader(WebGLId program, WebGLId shader);
  virtual void disable(WGC3Denum cap);
  virtual void disableVertexAttribArray(WGC3Duint index);
  virtual void drawArrays(WGC3Denum mode, WGC3Dint first, WGC3Dsizei count);
  virtual void drawElements(
      WGC3Denum mode,
      WGC3Dsizei count,
      WGC3Denum type,
      WGC3Dintptr offset);

  virtual void enable(WGC3Denum cap);
  virtual void enableVertexAttribArray(WGC3Duint index);
  virtual void finish();
  virtual void flush();
  virtual void framebufferRenderbuffer(
      WGC3Denum target,
      WGC3Denum attachment,
      WGC3Denum renderbuffertarget,
      WebGLId renderbuffer);
  virtual void framebufferTexture2D(
      WGC3Denum target,
      WGC3Denum attachment,
      WGC3Denum textarget,
      WebGLId texture,
      WGC3Dint level);
  virtual void frontFace(WGC3Denum mode);
  virtual void generateMipmap(WGC3Denum target);

  virtual bool getActiveAttrib(WebGLId program, WGC3Duint index, ActiveInfo&);
  virtual bool getActiveUniform(WebGLId program, WGC3Duint index, ActiveInfo&);

  virtual void getAttachedShaders(WebGLId program, WGC3Dsizei maxCount,
                                  WGC3Dsizei* count, WebGLId* shaders);

  virtual WGC3Dint getAttribLocation(WebGLId program, const WGC3Dchar* name);

  virtual void getBooleanv(WGC3Denum pname, WGC3Dboolean* value);

  virtual void getBufferParameteriv(
      WGC3Denum target, WGC3Denum pname, WGC3Dint* value);

  virtual Attributes getContextAttributes();

  virtual WGC3Denum getError();

  virtual bool isContextLost();

  virtual void getFloatv(WGC3Denum pname, WGC3Dfloat* value);

  virtual void getFramebufferAttachmentParameteriv(
      WGC3Denum target,
      WGC3Denum attachment,
      WGC3Denum pname,
      WGC3Dint* value);

  virtual void getIntegerv(WGC3Denum pname, WGC3Dint* value);

  virtual void getProgramiv(
      WebGLId program, WGC3Denum pname, WGC3Dint* value);

  virtual WebString getProgramInfoLog(WebGLId program);

  virtual void getRenderbufferParameteriv(
      WGC3Denum target, WGC3Denum pname, WGC3Dint* value);

  virtual void getShaderiv(
      WebGLId shader, WGC3Denum pname, WGC3Dint* value);

  virtual WebString getShaderInfoLog(WebGLId shader);

  // TBD
  // void glGetShaderPrecisionFormat(
  //     GLenum shadertype, GLenum precisiontype,
  //     GLint* range, GLint* precision);

  virtual WebString getShaderSource(WebGLId shader);
  virtual WebString getString(WGC3Denum name);

  virtual void getTexParameterfv(
      WGC3Denum target, WGC3Denum pname, WGC3Dfloat* value);
  virtual void getTexParameteriv(
      WGC3Denum target, WGC3Denum pname, WGC3Dint* value);

  virtual void getUniformfv(
      WebGLId program, WGC3Dint location, WGC3Dfloat* value);
  virtual void getUniformiv(
      WebGLId program, WGC3Dint location, WGC3Dint* value);

  virtual WGC3Dint getUniformLocation(WebGLId program, const WGC3Dchar* name);

  virtual void getVertexAttribfv(
      WGC3Duint index, WGC3Denum pname, WGC3Dfloat* value);
  virtual void getVertexAttribiv(
      WGC3Duint index, WGC3Denum pname, WGC3Dint* value);

  virtual WGC3Dsizeiptr getVertexAttribOffset(
      WGC3Duint index, WGC3Denum pname);

  virtual void hint(WGC3Denum target, WGC3Denum mode);
  virtual WGC3Dboolean isBuffer(WebGLId buffer);
  virtual WGC3Dboolean isEnabled(WGC3Denum cap);
  virtual WGC3Dboolean isFramebuffer(WebGLId framebuffer);
  virtual WGC3Dboolean isProgram(WebGLId program);
  virtual WGC3Dboolean isRenderbuffer(WebGLId renderbuffer);
  virtual WGC3Dboolean isShader(WebGLId shader);
  virtual WGC3Dboolean isTexture(WebGLId texture);
  virtual void lineWidth(WGC3Dfloat width);
  virtual void linkProgram(WebGLId program);
  virtual void pixelStorei(WGC3Denum pname, WGC3Dint param);
  virtual void polygonOffset(WGC3Dfloat factor, WGC3Dfloat units);

  virtual void readPixels(
      WGC3Dint x, WGC3Dint y,
      WGC3Dsizei width, WGC3Dsizei height,
      WGC3Denum format,
      WGC3Denum type,
      void* pixels);

  virtual void releaseShaderCompiler();
  virtual void renderbufferStorage(
      WGC3Denum target,
      WGC3Denum internalformat,
      WGC3Dsizei width,
      WGC3Dsizei height);
  virtual void sampleCoverage(WGC3Dclampf value, WGC3Dboolean invert);
  virtual void scissor(
      WGC3Dint x, WGC3Dint y, WGC3Dsizei width, WGC3Dsizei height);
  virtual void shaderSource(WebGLId shader, const WGC3Dchar* source);
  virtual void stencilFunc(WGC3Denum func, WGC3Dint ref, WGC3Duint mask);
  virtual void stencilFuncSeparate(
      WGC3Denum face, WGC3Denum func, WGC3Dint ref, WGC3Duint mask);
  virtual void stencilMask(WGC3Duint mask);
  virtual void stencilMaskSeparate(WGC3Denum face, WGC3Duint mask);
  virtual void stencilOp(WGC3Denum fail, WGC3Denum zfail, WGC3Denum zpass);
  virtual void stencilOpSeparate(
      WGC3Denum face,
      WGC3Denum fail,
      WGC3Denum zfail,
      WGC3Denum zpass);

  virtual void texImage2D(
      WGC3Denum target,
      WGC3Dint level,
      WGC3Denum internalformat,
      WGC3Dsizei width,
      WGC3Dsizei height,
      WGC3Dint border,
      WGC3Denum format,
      WGC3Denum type,
      const void* pixels);

  virtual void texParameterf(
      WGC3Denum target, WGC3Denum pname, WGC3Dfloat param);
  virtual void texParameteri(
      WGC3Denum target, WGC3Denum pname, WGC3Dint param);

  virtual void texSubImage2D(
      WGC3Denum target,
      WGC3Dint level,
      WGC3Dint xoffset,
      WGC3Dint yoffset,
      WGC3Dsizei width,
      WGC3Dsizei height,
      WGC3Denum format,
      WGC3Denum type,
      const void* pixels);

  virtual void uniform1f(WGC3Dint location, WGC3Dfloat x);
  virtual void uniform1fv(WGC3Dint location, WGC3Dsizei count,
                          const WGC3Dfloat* v);
  virtual void uniform1i(WGC3Dint location, WGC3Dint x);
  virtual void uniform1iv(WGC3Dint location, WGC3Dsizei count,
                          const WGC3Dint* v);
  virtual void uniform2f(WGC3Dint location, WGC3Dfloat x, WGC3Dfloat y);
  virtual void uniform2fv(WGC3Dint location, WGC3Dsizei count,
                          const WGC3Dfloat* v);
  virtual void uniform2i(WGC3Dint location, WGC3Dint x, WGC3Dint y);
  virtual void uniform2iv(WGC3Dint location, WGC3Dsizei count,
                          const WGC3Dint* v);
  virtual void uniform3f(WGC3Dint location,
                         WGC3Dfloat x, WGC3Dfloat y, WGC3Dfloat z);
  virtual void uniform3fv(WGC3Dint location, WGC3Dsizei count,
                          const WGC3Dfloat* v);
  virtual void uniform3i(WGC3Dint location, WGC3Dint x, WGC3Dint y, WGC3Dint z);
  virtual void uniform3iv(WGC3Dint location, WGC3Dsizei count,
                          const WGC3Dint* v);
  virtual void uniform4f(WGC3Dint location, WGC3Dfloat x, WGC3Dfloat y,
                         WGC3Dfloat z, WGC3Dfloat w);
  virtual void uniform4fv(WGC3Dint location, WGC3Dsizei count,
                          const WGC3Dfloat* v);
  virtual void uniform4i(WGC3Dint location, WGC3Dint x, WGC3Dint y,
                         WGC3Dint z, WGC3Dint w);
  virtual void uniform4iv(WGC3Dint location, WGC3Dsizei count,
                          const WGC3Dint* v);
  virtual void uniformMatrix2fv(
      WGC3Dint location, WGC3Dsizei count,
      WGC3Dboolean transpose, const WGC3Dfloat* value);
  virtual void uniformMatrix3fv(
      WGC3Dint location, WGC3Dsizei count,
      WGC3Dboolean transpose, const WGC3Dfloat* value);
  virtual void uniformMatrix4fv(
      WGC3Dint location, WGC3Dsizei count,
      WGC3Dboolean transpose, const WGC3Dfloat* value);

  virtual void useProgram(WebGLId program);
  virtual void validateProgram(WebGLId program);

  virtual void vertexAttrib1f(WGC3Duint index, WGC3Dfloat x);
  virtual void vertexAttrib1fv(WGC3Duint index, const WGC3Dfloat* values);
  virtual void vertexAttrib2f(WGC3Duint index, WGC3Dfloat x, WGC3Dfloat y);
  virtual void vertexAttrib2fv(WGC3Duint index, const WGC3Dfloat* values);
  virtual void vertexAttrib3f(
      WGC3Duint index, WGC3Dfloat x, WGC3Dfloat y, WGC3Dfloat z);
  virtual void vertexAttrib3fv(WGC3Duint index, const WGC3Dfloat* values);
  virtual void vertexAttrib4f(
      WGC3Duint index, WGC3Dfloat x, WGC3Dfloat y, WGC3Dfloat z, WGC3Dfloat w);
  virtual void vertexAttrib4fv(WGC3Duint index, const WGC3Dfloat* values);
  virtual void vertexAttribPointer(
      WGC3Duint index,
      WGC3Dint size,
      WGC3Denum type,
      WGC3Dboolean normalized,
      WGC3Dsizei stride,
      WGC3Dintptr offset);

  virtual void viewport(
      WGC3Dint x, WGC3Dint y, WGC3Dsizei width, WGC3Dsizei height);

  // Support for buffer creation and deletion
  virtual WebGLId createBuffer();
  virtual WebGLId createFramebuffer();
  virtual WebGLId createProgram();
  virtual WebGLId createRenderbuffer();
  virtual WebGLId createShader(WGC3Denum);
  virtual WebGLId createTexture();

  virtual void deleteBuffer(WebGLId);
  virtual void deleteFramebuffer(WebGLId);
  virtual void deleteProgram(WebGLId);
  virtual void deleteRenderbuffer(WebGLId);
  virtual void deleteShader(WebGLId);
  virtual void deleteTexture(WebGLId);

 private:
  // ANGLE related.
  struct ShaderSourceEntry {
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

  typedef base::hash_map<WebGLId, ShaderSourceEntry*> ShaderSourceMap;

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
  void FlipVertically(unsigned char* framebuffer,
                      unsigned int width,
                      unsigned int height);
#endif

  // Take into account the user's requested context creation attributes, in
  // particular stencil and antialias, and determine which could or could
  // not be honored based on the capabilities of the OpenGL implementation.
  void ValidateAttributes();

  // Resolve the given rectangle of the multisampled framebuffer if necessary.
  void ResolveMultisampledFramebuffer(
      WGC3Dint x, WGC3Dint y, WGC3Dsizei width, WGC3Dsizei height);

  bool AngleCreateCompilers();
  void AngleDestroyCompilers();
  bool AngleValidateShaderSource(ShaderSourceEntry* entry);

  WebGraphicsContext3D::Attributes attributes_;
  bool initialized_;
  bool render_directly_to_web_view_;
  bool is_gles2_;
  bool have_ext_framebuffer_object_;
  bool have_ext_framebuffer_multisample_;
  bool have_angle_framebuffer_multisample_;

  WebGLId texture_;
  WebGLId fbo_;
  WebGLId depth_stencil_buffer_;
  int cached_width_, cached_height_;

  // For multisampling
  WebGLId multisample_fbo_;
  WebGLId multisample_depth_stencil_buffer_;
  WebGLId multisample_color_buffer_;

  // For tracking which FBO is bound
  WebGLId bound_fbo_;

  // For tracking which texture is bound
  WebGLId bound_texture_;

  // FBO used for copying child texture to parent texture.
  WebGLId copy_texture_to_parent_texture_fbo_;

#ifdef FLIP_FRAMEBUFFER_VERTICALLY
  unsigned char* scanline_;
#endif

  // Errors raised by synthesizeGLError().
  std::list<WGC3Denum> synthetic_errors_list_;
  std::set<WGC3Denum> synthetic_errors_set_;

  scoped_ptr<gfx::GLContext> gl_context_;

  ShaderSourceMap shader_source_map_;

  ShHandle fragment_compiler_;
  ShHandle vertex_compiler_;
};

}  // namespace gpu
}  // namespace webkit

#endif  // WEBKIT_GPU_WEBGRAPHICSCONTEXT3D_IN_PROCESS_IMPL_H_

