// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_MEMORY_H_
#define UI_GL_GL_IMAGE_MEMORY_H_

#include "ui/gl/gl_image.h"

#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

namespace gfx {

class GL_EXPORT GLImageMemory : public GLImage {
 public:
  GLImageMemory(const gfx::Size& size, unsigned internalformat);

  bool Initialize(const unsigned char* memory);

  // Overridden from GLImage:
  virtual void Destroy(bool have_context) OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual bool BindTexImage(unsigned target) OVERRIDE;
  virtual void ReleaseTexImage(unsigned target) OVERRIDE {}
  virtual bool CopyTexImage(unsigned target) OVERRIDE;
  virtual void WillUseTexImage() OVERRIDE;
  virtual void DidUseTexImage() OVERRIDE;
  virtual void WillModifyTexImage() OVERRIDE {}
  virtual void DidModifyTexImage() OVERRIDE {}
  virtual bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                    int z_order,
                                    OverlayTransform transform,
                                    const Rect& bounds_rect,
                                    const RectF& crop_rect) OVERRIDE;

 protected:
  virtual ~GLImageMemory();

  bool HasValidFormat() const;
  size_t Bytes() const;

 private:
  void DoBindTexImage(unsigned target);

  const unsigned char* memory_;
  const gfx::Size size_;
  const unsigned internalformat_;
  bool in_use_;
  unsigned target_;
  bool need_do_bind_tex_image_;
#if defined(OS_WIN) || defined(USE_X11) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
  unsigned egl_texture_id_;
  EGLImageKHR egl_image_;
#endif

  DISALLOW_COPY_AND_ASSIGN(GLImageMemory);
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_MEMORY_H_
