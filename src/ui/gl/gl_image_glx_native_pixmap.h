// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_GLX_NATIVE_PIXMAP_H_
#define UI_GL_GL_IMAGE_GLX_NATIVE_PIXMAP_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image_glx.h"

namespace gl {

class GL_EXPORT GLImageGLXNativePixmap : public GLImageGLX {
 public:
  GLImageGLXNativePixmap(const gfx::Size& size, gfx::BufferFormat format);
  bool Initialize(scoped_refptr<gfx::NativePixmap> pixmap);

 protected:
  ~GLImageGLXNativePixmap() override;

 private:
  scoped_refptr<gfx::NativePixmap> native_pixmap_;

  DISALLOW_COPY_AND_ASSIGN(GLImageGLXNativePixmap);
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_GLX_NATIVE_PIXMAP_H_
