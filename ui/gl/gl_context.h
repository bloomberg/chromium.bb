// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_H_
#define UI_GL_GL_CONTEXT_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gpu_preference.h"

namespace gfx {

class GLSurface;

// Encapsulates an OpenGL context, hiding platform specific management.
class GL_EXPORT GLContext : public base::RefCounted<GLContext> {
 public:
  explicit GLContext(GLShareGroup* share_group);

  // Initializes the GL context to be compatible with the given surface. The GL
  // context can be made with other surface's of the same type. The compatible
  // surface is only needed for certain platforms like WGL, OSMesa and GLX. It
  // should be specific for all platforms though.
  virtual bool Initialize(
      GLSurface* compatible_surface, GpuPreference gpu_preference) = 0;

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

  GLShareGroup* share_group();

  // Create a GL context that is compatible with the given surface.
  // |share_group|, if non-NULL, is a group of contexts which the
  // internally created OpenGL context shares textures and other resources.
  static scoped_refptr<GLContext> CreateGLContext(
      GLShareGroup* share_group,
      GLSurface* compatible_surface,
      GpuPreference gpu_preference);

  static bool LosesAllContextsOnContextLost();

  static bool SupportsDualGpus();

  static GLContext* GetCurrent();

  virtual bool WasAllocatedUsingARBRobustness();

 protected:
  virtual ~GLContext();
  static void SetCurrent(GLContext* context, GLSurface* surface);

  // Initialize function pointers to extension functions in the GL
  // implementation. Should be called immediately after this context is made
  // current.
  bool InitializeExtensionBindings();

 private:
  friend class base::RefCounted<GLContext>;

  scoped_refptr<GLShareGroup> share_group_;

  DISALLOW_COPY_AND_ASSIGN(GLContext);
};

}  // namespace gfx

#endif  // UI_GL_GL_CONTEXT_H_
