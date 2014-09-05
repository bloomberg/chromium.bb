// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_EGL_H_
#define UI_GL_GL_IMAGE_EGL_H_

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"

namespace gfx {

class GL_EXPORT GLImageEGL : public GLImage {
 public:
  explicit GLImageEGL(const gfx::Size& size);

  bool Initialize(EGLenum target, EGLClientBuffer buffer, const EGLint* attrs);

  // Overridden from GLImage:
  virtual void Destroy(bool have_context) OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool BindTexImage(unsigned target) OVERRIDE;
  virtual void ReleaseTexImage(unsigned target) OVERRIDE {}
  virtual bool CopyTexImage(unsigned target) OVERRIDE;
  virtual void WillUseTexImage() OVERRIDE {}
  virtual void DidUseTexImage() OVERRIDE {}
  virtual void WillModifyTexImage() OVERRIDE {}
  virtual void DidModifyTexImage() OVERRIDE {}
  virtual bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                    int z_order,
                                    OverlayTransform transform,
                                    const Rect& bounds_rect,
                                    const RectF& crop_rect) OVERRIDE;

 protected:
  virtual ~GLImageEGL();

  EGLImageKHR egl_image_;
  const gfx::Size size_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLImageEGL);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_EGL_H_
