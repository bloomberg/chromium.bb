// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SCOPED_MAKE_CURRENT_H_
#define UI_GL_SCOPED_MAKE_CURRENT_H_
#pragma once

#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace gfx {

class GL_EXPORT ScopedMakeCurrent {
 public:
  explicit ScopedMakeCurrent(GLContext* context, GLSurface* surface);
  ~ScopedMakeCurrent();

  bool Succeeded();

 private:
  scoped_refptr<GLContext> previous_context_;
  scoped_refptr<GLSurface> previous_surface_;
  scoped_refptr<GLContext> context_;
  scoped_refptr<GLSurface> surface_;
  bool succeeded_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMakeCurrent);
};

}  // namespace gfx

#endif  // UI_GL_SCOPED_MAKE_CURRENT_H_
