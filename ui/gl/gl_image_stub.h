// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_STUB_H_
#define UI_GL_GL_IMAGE_STUB_H_

#include "ui/gl/gl_image.h"

namespace gfx {

// A GLImage that does nothing for unit tests.
class GL_EXPORT GLImageStub : public GLImage {
 public:
  GLImageStub();

  // Overridden from GLImage:
  virtual void Destroy(bool have_context) override {}
  virtual gfx::Size GetSize() override;
  virtual bool BindTexImage(unsigned target) override;
  virtual void ReleaseTexImage(unsigned target) override {}
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
  virtual ~GLImageStub();
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_STUB_H_
