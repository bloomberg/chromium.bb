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
  virtual void Destroy(bool have_context) override;
  virtual gfx::Size GetSize() override;
  virtual bool BindTexImage(unsigned target) override;
  virtual void ReleaseTexImage(unsigned target) override;
  virtual bool CopyTexImage(unsigned target) override;
  virtual void WillUseTexImage() override {}
  virtual void DidUseTexImage() override {}
  virtual void WillModifyTexImage() override {}
  virtual void DidModifyTexImage() override {}
  virtual bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                    int z_order,
                                    OverlayTransform transform,
                                    const Rect& bounds_rect,
                                    const RectF& crop_rect) override;

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
