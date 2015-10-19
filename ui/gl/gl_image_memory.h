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

#include "base/numerics/safe_math.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gfx {

class GL_EXPORT GLImageMemory : public GLImage {
 public:
  GLImageMemory(const Size& size, unsigned internalformat);

  static bool StrideInBytes(size_t width,
                            BufferFormat format,
                            size_t* stride_in_bytes);

  bool Initialize(const unsigned char* memory, BufferFormat format);

  // Overridden from GLImage:
  void Destroy(bool have_context) override;
  Size GetSize() override;
  unsigned GetInternalFormat() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override {}
  bool CopyTexSubImage(unsigned target,
                       const Point& offset,
                       const Rect& rect) override;
  void WillUseTexImage() override;
  void DidUseTexImage() override;
  void WillModifyTexImage() override {}
  void DidModifyTexImage() override {}
  bool ScheduleOverlayPlane(AcceleratedWidget widget,
                            int z_order,
                            OverlayTransform transform,
                            const Rect& bounds_rect,
                            const RectF& crop_rect) override;

  // Only dumps the GLTexture portion of the memory usage. Subclasses are
  // responsible for dumping the CPU-side memory.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;

 protected:
  ~GLImageMemory() override;

  BufferFormat format() const { return format_; }

 private:
  void DoBindTexImage(unsigned target);

  const Size size_;
  const unsigned internalformat_;
  const unsigned char* memory_;
  BufferFormat format_;
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
