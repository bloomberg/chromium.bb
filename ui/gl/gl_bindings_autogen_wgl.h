// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// ui/gl/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef UI_GL_GL_BINDINGS_AUTOGEN_WGL_H_
#define UI_GL_GL_BINDINGS_AUTOGEN_WGL_H_

#include <string>

namespace gl {

class GLContext;

typedef BOOL(GL_BINDING_CALL* wglChoosePixelFormatARBProc)(
    HDC dc,
    const int* int_attrib_list,
    const float* float_attrib_list,
    UINT max_formats,
    int* formats,
    UINT* num_formats);
typedef BOOL(GL_BINDING_CALL* wglCopyContextProc)(HGLRC hglrcSrc,
                                                  HGLRC hglrcDst,
                                                  UINT mask);
typedef HGLRC(GL_BINDING_CALL* wglCreateContextProc)(HDC hdc);
typedef HGLRC(GL_BINDING_CALL* wglCreateContextAttribsARBProc)(
    HDC hDC,
    HGLRC hShareContext,
    const int* attribList);
typedef HGLRC(GL_BINDING_CALL* wglCreateLayerContextProc)(HDC hdc,
                                                          int iLayerPlane);
typedef HPBUFFERARB(GL_BINDING_CALL* wglCreatePbufferARBProc)(
    HDC hDC,
    int iPixelFormat,
    int iWidth,
    int iHeight,
    const int* piAttribList);
typedef BOOL(GL_BINDING_CALL* wglDeleteContextProc)(HGLRC hglrc);
typedef BOOL(GL_BINDING_CALL* wglDestroyPbufferARBProc)(HPBUFFERARB hPbuffer);
typedef HGLRC(GL_BINDING_CALL* wglGetCurrentContextProc)();
typedef HDC(GL_BINDING_CALL* wglGetCurrentDCProc)();
typedef const char*(GL_BINDING_CALL* wglGetExtensionsStringARBProc)(HDC hDC);
typedef const char*(GL_BINDING_CALL* wglGetExtensionsStringEXTProc)();
typedef HDC(GL_BINDING_CALL* wglGetPbufferDCARBProc)(HPBUFFERARB hPbuffer);
typedef BOOL(GL_BINDING_CALL* wglMakeCurrentProc)(HDC hdc, HGLRC hglrc);
typedef BOOL(GL_BINDING_CALL* wglQueryPbufferARBProc)(HPBUFFERARB hPbuffer,
                                                      int iAttribute,
                                                      int* piValue);
typedef int(GL_BINDING_CALL* wglReleasePbufferDCARBProc)(HPBUFFERARB hPbuffer,
                                                         HDC hDC);
typedef BOOL(GL_BINDING_CALL* wglShareListsProc)(HGLRC hglrc1, HGLRC hglrc2);
typedef BOOL(GL_BINDING_CALL* wglSwapIntervalEXTProc)(int interval);
typedef BOOL(GL_BINDING_CALL* wglSwapLayerBuffersProc)(HDC hdc, UINT fuPlanes);

struct ExtensionsWGL {
  bool b_WGL_ARB_create_context;
  bool b_WGL_ARB_extensions_string;
  bool b_WGL_ARB_pbuffer;
  bool b_WGL_ARB_pixel_format;
  bool b_WGL_EXT_extensions_string;
  bool b_WGL_EXT_swap_control;
};

struct ProcsWGL {
  wglChoosePixelFormatARBProc wglChoosePixelFormatARBFn;
  wglCopyContextProc wglCopyContextFn;
  wglCreateContextProc wglCreateContextFn;
  wglCreateContextAttribsARBProc wglCreateContextAttribsARBFn;
  wglCreateLayerContextProc wglCreateLayerContextFn;
  wglCreatePbufferARBProc wglCreatePbufferARBFn;
  wglDeleteContextProc wglDeleteContextFn;
  wglDestroyPbufferARBProc wglDestroyPbufferARBFn;
  wglGetCurrentContextProc wglGetCurrentContextFn;
  wglGetCurrentDCProc wglGetCurrentDCFn;
  wglGetExtensionsStringARBProc wglGetExtensionsStringARBFn;
  wglGetExtensionsStringEXTProc wglGetExtensionsStringEXTFn;
  wglGetPbufferDCARBProc wglGetPbufferDCARBFn;
  wglMakeCurrentProc wglMakeCurrentFn;
  wglQueryPbufferARBProc wglQueryPbufferARBFn;
  wglReleasePbufferDCARBProc wglReleasePbufferDCARBFn;
  wglShareListsProc wglShareListsFn;
  wglSwapIntervalEXTProc wglSwapIntervalEXTFn;
  wglSwapLayerBuffersProc wglSwapLayerBuffersFn;
};

class GL_EXPORT WGLApi {
 public:
  WGLApi();
  virtual ~WGLApi();

  virtual void SetDisabledExtensions(const std::string& disabled_extensions) {}

  virtual BOOL wglChoosePixelFormatARBFn(HDC dc,
                                         const int* int_attrib_list,
                                         const float* float_attrib_list,
                                         UINT max_formats,
                                         int* formats,
                                         UINT* num_formats) = 0;
  virtual BOOL wglCopyContextFn(HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask) = 0;
  virtual HGLRC wglCreateContextFn(HDC hdc) = 0;
  virtual HGLRC wglCreateContextAttribsARBFn(HDC hDC,
                                             HGLRC hShareContext,
                                             const int* attribList) = 0;
  virtual HGLRC wglCreateLayerContextFn(HDC hdc, int iLayerPlane) = 0;
  virtual HPBUFFERARB wglCreatePbufferARBFn(HDC hDC,
                                            int iPixelFormat,
                                            int iWidth,
                                            int iHeight,
                                            const int* piAttribList) = 0;
  virtual BOOL wglDeleteContextFn(HGLRC hglrc) = 0;
  virtual BOOL wglDestroyPbufferARBFn(HPBUFFERARB hPbuffer) = 0;
  virtual HGLRC wglGetCurrentContextFn() = 0;
  virtual HDC wglGetCurrentDCFn() = 0;
  virtual const char* wglGetExtensionsStringARBFn(HDC hDC) = 0;
  virtual const char* wglGetExtensionsStringEXTFn() = 0;
  virtual HDC wglGetPbufferDCARBFn(HPBUFFERARB hPbuffer) = 0;
  virtual BOOL wglMakeCurrentFn(HDC hdc, HGLRC hglrc) = 0;
  virtual BOOL wglQueryPbufferARBFn(HPBUFFERARB hPbuffer,
                                    int iAttribute,
                                    int* piValue) = 0;
  virtual int wglReleasePbufferDCARBFn(HPBUFFERARB hPbuffer, HDC hDC) = 0;
  virtual BOOL wglShareListsFn(HGLRC hglrc1, HGLRC hglrc2) = 0;
  virtual BOOL wglSwapIntervalEXTFn(int interval) = 0;
  virtual BOOL wglSwapLayerBuffersFn(HDC hdc, UINT fuPlanes) = 0;
};

}  // namespace gl

#define wglChoosePixelFormatARB \
  ::gl::g_current_wgl_context->wglChoosePixelFormatARBFn
#define wglCopyContext ::gl::g_current_wgl_context->wglCopyContextFn
#define wglCreateContext ::gl::g_current_wgl_context->wglCreateContextFn
#define wglCreateContextAttribsARB \
  ::gl::g_current_wgl_context->wglCreateContextAttribsARBFn
#define wglCreateLayerContext \
  ::gl::g_current_wgl_context->wglCreateLayerContextFn
#define wglCreatePbufferARB ::gl::g_current_wgl_context->wglCreatePbufferARBFn
#define wglDeleteContext ::gl::g_current_wgl_context->wglDeleteContextFn
#define wglDestroyPbufferARB ::gl::g_current_wgl_context->wglDestroyPbufferARBFn
#define wglGetCurrentContext ::gl::g_current_wgl_context->wglGetCurrentContextFn
#define wglGetCurrentDC ::gl::g_current_wgl_context->wglGetCurrentDCFn
#define wglGetExtensionsStringARB \
  ::gl::g_current_wgl_context->wglGetExtensionsStringARBFn
#define wglGetExtensionsStringEXT \
  ::gl::g_current_wgl_context->wglGetExtensionsStringEXTFn
#define wglGetPbufferDCARB ::gl::g_current_wgl_context->wglGetPbufferDCARBFn
#define wglMakeCurrent ::gl::g_current_wgl_context->wglMakeCurrentFn
#define wglQueryPbufferARB ::gl::g_current_wgl_context->wglQueryPbufferARBFn
#define wglReleasePbufferDCARB \
  ::gl::g_current_wgl_context->wglReleasePbufferDCARBFn
#define wglShareLists ::gl::g_current_wgl_context->wglShareListsFn
#define wglSwapIntervalEXT ::gl::g_current_wgl_context->wglSwapIntervalEXTFn
#define wglSwapLayerBuffers ::gl::g_current_wgl_context->wglSwapLayerBuffersFn

#endif  //  UI_GL_GL_BINDINGS_AUTOGEN_WGL_H_
