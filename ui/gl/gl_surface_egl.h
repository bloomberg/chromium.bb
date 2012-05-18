// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_H_
#define UI_GL_GL_SURFACE_EGL_H_
#pragma once

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/compiler_specific.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_surface.h"

typedef void* EGLConfig;
typedef void* EGLDisplay;
typedef void* EGLSurface;

#if defined(OS_ANDROID)
typedef void* EGLNativeDisplayType;
#elif defined(OS_WIN)
typedef HDC EGLNativeDisplayType;
#else
typedef struct _XDisplay* EGLNativeDisplayType;
#endif

namespace gfx {

// Interface for EGL surface.
class GL_EXPORT GLSurfaceEGL : public GLSurface {
 public:
  GLSurfaceEGL();

  // Implement GLSurface.
  virtual EGLDisplay GetDisplay() OVERRIDE;

  static bool InitializeOneOff();
  static EGLDisplay GetHardwareDisplay();
  static EGLDisplay GetSoftwareDisplay();
  static EGLNativeDisplayType GetNativeDisplay();

 protected:
  virtual ~GLSurfaceEGL();

  bool software_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceEGL);
};

// Encapsulates an EGL surface bound to a view.
class NativeViewGLSurfaceEGL : public GLSurfaceEGL {
 public:
  NativeViewGLSurfaceEGL(bool software, gfx::AcceleratedWidget window);

  // Implement GLSurface.
  virtual EGLConfig GetConfig() OVERRIDE;
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual EGLSurface GetHandle() OVERRIDE;
  virtual std::string GetExtensions() OVERRIDE;
  virtual bool PostSubBuffer(int x, int y, int width, int height) OVERRIDE;

 protected:
  virtual ~NativeViewGLSurfaceEGL();
  void SetHandle(EGLSurface surface);

 private:
  gfx::AcceleratedWidget window_;
  EGLSurface surface_;
  bool supports_post_sub_buffer_;
  EGLConfig config_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceEGL);
};

// Encapsulates a pbuffer EGL surface.
class GL_EXPORT PbufferGLSurfaceEGL : public GLSurfaceEGL {
 public:
  PbufferGLSurfaceEGL(bool software, const gfx::Size& size);

  // Implement GLSurface.
  virtual EGLConfig GetConfig() OVERRIDE;
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool Resize(const gfx::Size& size) OVERRIDE;
  virtual EGLSurface GetHandle() OVERRIDE;
  virtual void* GetShareHandle() OVERRIDE;

 protected:
  virtual ~PbufferGLSurfaceEGL();

 private:
  gfx::Size size_;
  EGLSurface surface_;

  DISALLOW_COPY_AND_ASSIGN(PbufferGLSurfaceEGL);
};

}  // namespace gfx

#endif  // UI_GL_GL_SURFACE_EGL_H_
