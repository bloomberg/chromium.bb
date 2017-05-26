// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3d11.h>

#include "base/win/scoped_comptr.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

typedef void* EGLStreamKHR;

namespace gl {

class GL_EXPORT GLImageDXGI : public GLImage {
 public:
  GLImageDXGI(const gfx::Size& size, EGLStreamKHR stream);

  // Safe downcast. Returns nullptr on failure.
  static GLImageDXGI* FromGLImage(GLImage* image);

  // GLImage implementation.
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override;
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  void Flush() override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override;
  Type GetType() const override;

  void SetTexture(const base::win::ScopedComPtr<ID3D11Texture2D>& texture,
                  size_t level);

  base::win::ScopedComPtr<ID3D11Texture2D> texture() { return texture_; }
  size_t level() const { return level_; }

 protected:
  ~GLImageDXGI() override;

  gfx::Size size_;

  EGLStreamKHR stream_;

  base::win::ScopedComPtr<ID3D11Texture2D> texture_;
  size_t level_ = 0;
};

// This copies to a new texture on bind.
class GL_EXPORT CopyingGLImageDXGI : public GLImageDXGI {
 public:
  CopyingGLImageDXGI(const base::win::ScopedComPtr<ID3D11Device>& d3d11_device,
                     const gfx::Size& size,
                     EGLStreamKHR stream);

  bool Initialize();
  bool InitializeVideoProcessor(
      const base::win::ScopedComPtr<ID3D11VideoProcessor>& video_processor,
      const base::win::ScopedComPtr<ID3D11VideoProcessorEnumerator>&
          enumerator);
  void UnbindFromTexture();

  // GLImage implementation.
  bool BindTexImage(unsigned target) override;

 private:
  ~CopyingGLImageDXGI() override;

  bool copied_ = false;

  base::win::ScopedComPtr<ID3D11VideoDevice> video_device_;
  base::win::ScopedComPtr<ID3D11VideoContext> video_context_;
  base::win::ScopedComPtr<ID3D11VideoProcessor> d3d11_processor_;
  base::win::ScopedComPtr<ID3D11VideoProcessorEnumerator> enumerator_;
  base::win::ScopedComPtr<ID3D11Device> d3d11_device_;
  base::win::ScopedComPtr<ID3D11Texture2D> decoder_copy_texture_;
  base::win::ScopedComPtr<ID3D11VideoProcessorOutputView> output_view_;
};
}
