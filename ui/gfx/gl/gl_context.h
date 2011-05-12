// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_CONTEXT_H_
#define UI_GFX_GL_GL_CONTEXT_H_
#pragma once

#include <string>

#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace gfx {

class GLSurface;

// Encapsulates an OpenGL context, hiding platform specific management.
class GLContext {
 public:
  GLContext() {}
  virtual ~GLContext() {}

  // Destroys the GL context.
  virtual void Destroy() = 0;

  // Makes the GL context current on the current thread.
  virtual bool MakeCurrent() = 0;

  // Releases this GL context as current on the current thread. TODO(apatrick):
  // implement this in the other GLContexts.
  virtual void ReleaseCurrent();

  // Returns true if this context is current.
  virtual bool IsCurrent() = 0;

  // Returns true if this context is offscreen.
  virtual bool IsOffscreen() = 0;

  // Swaps front and back buffers. This has no effect for off-screen
  // contexts.
  virtual bool SwapBuffers() = 0;

  // Get the size of the back buffer.
  virtual gfx::Size GetSize() = 0;

  // Get the surface. TODO(apatrick): remove this when contexts are split from
  // surfaces.
  virtual GLSurface* GetSurface();

  // Get the underlying platform specific GL context "handle".
  virtual void* GetHandle() = 0;

  // Set swap interval. This context must be current.
  virtual void SetSwapInterval(int interval) = 0;

  // Returns the internal frame buffer object name if the context is backed by
  // FBO. Otherwise returns 0.
  virtual unsigned int GetBackingFrameBufferObject();

  // Returns space separated list of extensions. The context must be current.
  virtual std::string GetExtensions();

  // Returns whether the current context supports the named extension. The
  // context must be current.
  bool HasExtension(const char* name);

  // Create a GL context that is compatible with the given surface.
  // |share_context|, if non-NULL, is a context which the
  // internally created OpenGL context shares textures and other resources.
  // TODO(apatrick): For the time being, the context will take ownership of the
  // surface and the surface will be made the current read and draw surface
  // when the context is made current.
  static GLContext* CreateGLContext(GLSurface* compatible_surface,
                                    GLContext* shared_context);

  static bool LosesAllContextsOnContextLost();

 protected:
  bool InitializeCommon();

 private:
  DISALLOW_COPY_AND_ASSIGN(GLContext);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_CONTEXT_H_
