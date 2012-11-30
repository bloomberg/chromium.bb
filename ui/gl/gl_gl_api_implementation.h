// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_API_IMPLEMENTATION_H_
#define UI_GL_GL_API_IMPLEMENTATION_H_

#include "base/compiler_specific.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"

namespace gpu {
namespace gles2 {
class GLES2Decoder;
}
}
namespace gfx {

class GLContext;
class GLSurface;

void InitializeGLBindingsGL();
void InitializeGLExtensionBindingsGL(GLContext* context);
void InitializeDebugGLBindingsGL();
void ClearGLBindingsGL();
void SetGLToRealGLApi();
void SetGLApi(GLApi* api);

// Implemenents the GL API by calling directly into the driver.
class GL_EXPORT RealGLApi : public GLApi {
 public:
  RealGLApi();
  virtual ~RealGLApi();
  void Initialize(DriverGL* driver);

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_gl.h"

 private:
  DriverGL* driver_;
};

// Implementents the GL API using co-operative state restoring.
// Assumes there is only one real GL context and that multiple virtual contexts
// are implemented above it. Restores the needed state from the current context.
class GL_EXPORT VirtualGLApi : public GLApi {
 public:
  VirtualGLApi();
  virtual ~VirtualGLApi();
  void Initialize(DriverGL* driver, GLContext* real_context);

  // Include the auto-generated part of this class. We split this because
  // it means we can easily edit the non-auto generated parts right here in
  // this file instead of having to edit some template or the code generator.
  #include "gl_bindings_api_autogen_gl.h"

  // Sets the current virutal context.
  bool MakeCurrent(GLContext* virtual_context, GLSurface* surface);

 private:
  DriverGL* driver_;

  // The real context we're running on.
  GLContext* real_context_;

  // The current virtual context.
  GLContext* current_context_;
};

}  // namespace gfx

#endif  // UI_GL_GL_API_IMPLEMENTATION_H_
