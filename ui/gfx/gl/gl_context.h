// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GL_GL_CONTEXT_H_
#define UI_GFX_GL_GL_CONTEXT_H_
#pragma once

#include <string>

#include "base/basictypes.h"

namespace gfx {

class GLSurface;

// Encapsulates an OpenGL context, hiding platform specific management.
class GLContext {
 public:
  GLContext() {}
  virtual ~GLContext() {}

  // Initializes the GL context to be compatible with the given surface. The GL
  // context can be made with other surface's of the same type. The compatible
  // surface is only needed for certain platforms like WGL, OSMesa and GLX. It
  // should be specific for all platforms though.
  virtual bool Initialize(GLContext* shared_context,
                          GLSurface* compatible_surface) = 0;

  // Destroys the GL context.
  virtual void Destroy() = 0;

  // Makes the GL context and a surface current on the current thread.
  virtual bool MakeCurrent(GLSurface* surface) = 0;

  // Releases this GL context and surface as current on the current thread.
  virtual void ReleaseCurrent(GLSurface* surface) = 0;

  // Returns true if this context and surface is current. Pass a null surface
  // if the current surface is not important.
  virtual bool IsCurrent(GLSurface* surface) = 0;

  // Get the underlying platform specific GL context "handle".
  virtual void* GetHandle() = 0;

  // Set swap interval. This context must be current.
  virtual void SetSwapInterval(int interval) = 0;

  // Returns space separated list of extensions. The context must be current.
  virtual std::string GetExtensions();

  // Returns whether the current context supports the named extension. The
  // context must be current.
  bool HasExtension(const char* name);

  // Create a GL context that is compatible with the given surface.
  // |share_context|, if non-NULL, is a context which the
  // internally created OpenGL context shares textures and other resources.
  static GLContext* CreateGLContext(GLContext* shared_context,
                                    GLSurface* compatible_surface);

  static bool LosesAllContextsOnContextLost();

 private:
  DISALLOW_COPY_AND_ASSIGN(GLContext);
};

}  // namespace gfx

#endif  // UI_GFX_GL_GL_CONTEXT_H_
