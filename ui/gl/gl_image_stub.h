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
  void Destroy(bool have_context) override {}
  Size GetSize() override;
  unsigned GetInternalFormat() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override {}
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const Point& offset,
                       const Rect& rect) override;
  bool ScheduleOverlayPlane(AcceleratedWidget widget,
                            int z_order,
                            OverlayTransform transform,
                            const Rect& bounds_rect,
                            const RectF& crop_rect) override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override {}

 protected:
  ~GLImageStub() override;
};

}  // namespace gfx

#endif  // UI_GL_GL_IMAGE_STUB_H_
