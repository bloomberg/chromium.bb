// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_CONTEXT_OSMESA_H_
#define UI_GL_GL_CONTEXT_OSMESA_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_export.h"

typedef struct osmesa_context* OSMesaContext;

namespace gl {

class GLShareGroup;
class GLSurface;

// Encapsulates an OSMesa OpenGL context that uses software rendering.
class GL_EXPORT GLContextOSMesa : public GLContextReal {
 public:
  explicit GLContextOSMesa(GLShareGroup* share_group);

  // Implement GLContext.
  bool Initialize(GLSurface* compatible_surface,
                  const GLContextAttribs& attribs) override;
  bool MakeCurrent(GLSurface* surface) override;
  void ReleaseCurrent(GLSurface* surface) override;
  bool IsCurrent(GLSurface* surface) override;
  void* GetHandle() override;

 protected:
  ~GLContextOSMesa() override;

 private:
  void Destroy();

  OSMesaContext context_;
  bool is_released_;

  DISALLOW_COPY_AND_ASSIGN(GLContextOSMesa);
};

}  // namespace gl

#endif  // UI_GL_GL_CONTEXT_OSMESA_H_
