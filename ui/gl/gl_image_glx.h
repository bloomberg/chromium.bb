// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_GLX_H_
#define UI_GL_GL_IMAGE_GLX_H_

#include "ui/gfx/size.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

namespace gfx {

class GL_EXPORT GLImageGLX : public GLImage {
 public:
  GLImageGLX(const gfx::Size& size, unsigned internalformat);

  bool Initialize(XID pixmap);

  // Overridden from GLImage:
  virtual void Destroy(bool have_context) OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool BindTexImage(unsigned target) OVERRIDE;
  virtual void ReleaseTexImage(unsigned target) OVERRIDE;
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
  virtual ~GLImageGLX();

 private:
  XID glx_pixmap_;
  const gfx::Size size_;
  unsigned internalformat_;

  DISALLOW_COPY_AND_ASSIGN(GLImageGLX);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_GLX_H_
