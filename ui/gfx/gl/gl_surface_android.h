// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_SURFACE_ANDROID_H_
#define UI_GFX_GL_GL_SURFACE_ANDROID_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "ui/gfx/gl/gl_surface_egl.h"

namespace gfx {

// A view surface. This can be created in the GPU process (default case), or
// browser process (in-process-gpu), or render process (in-process-webgl). When
// it is initialized, it always uses a pbuffer EGL surface until the native view
// is set. The native view is in charge of sharing the GL texture with UI thread
// in the browser process through SurfaceTexture.
class AndroidViewSurface : public NativeViewGLSurfaceEGL {
 public:
  AndroidViewSurface();
  virtual ~AndroidViewSurface();

  // Implement GLSurface.
  virtual bool Initialize() OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual bool Resize(const gfx::Size& size) OVERRIDE;
  virtual bool IsOffscreen() OVERRIDE;
  virtual bool SwapBuffers() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual EGLSurface GetHandle() OVERRIDE;
  virtual void SetNativeWindow(AndroidNativeWindow* window) OVERRIDE;

 private:
  bool CreateWindowSurface(AndroidNativeWindow* window);
  scoped_refptr<PbufferGLSurfaceEGL>  pbuffer_surface_;
  AndroidNativeWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(AndroidViewSurface);
};

}  // namespace gfx

#endif /* UI_GFX_GL_GL_SURFACE_ANDROID_H_ */
