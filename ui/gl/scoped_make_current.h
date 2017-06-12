// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SCOPED_MAKE_CURRENT_H_
#define UI_GL_SCOPED_MAKE_CURRENT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "ui/gl/gl_export.h"

namespace gl {
class GLContext;
class GLSurface;
}

namespace ui {

class GL_EXPORT ScopedMakeCurrent {
 public:
  ScopedMakeCurrent(gl::GLContext* context, gl::GLSurface* surface);
  ~ScopedMakeCurrent();

  bool Succeeded() const;

 private:
  scoped_refptr<gl::GLContext> previous_context_;
  scoped_refptr<gl::GLSurface> previous_surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;
  bool succeeded_;

  DISALLOW_COPY_AND_ASSIGN(ScopedMakeCurrent);
};

// This class is used to make sure a specified surface isn't current, and upon
// destruction it will make the surface current again if it had been before.
class GL_EXPORT ScopedReleaseCurrent {
 public:
  explicit ScopedReleaseCurrent(gl::GLSurface* this_surface);

  ~ScopedReleaseCurrent();

 private:
  base::Optional<ScopedMakeCurrent> make_current_;

  DISALLOW_COPY_AND_ASSIGN(ScopedReleaseCurrent);
};

}  // namespace ui

#endif  // UI_GL_SCOPED_MAKE_CURRENT_H_
