// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_EGL_H_
#define MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_EGL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/gpu/vaapi/vaapi_picture_native_pixmap.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VaapiWrapper;

// Implementation of VaapiPictureNativePixmap for EGL backends, see
// https://crbug.com/785201.
class VaapiPictureNativePixmapEgl : public VaapiPictureNativePixmap {
 public:
  VaapiPictureNativePixmapEgl(
      const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb_,
      int32_t picture_buffer_id,
      const gfx::Size& size,
      uint32_t texture_id,
      uint32_t client_texture_id,
      uint32_t texture_target);

  ~VaapiPictureNativePixmapEgl() override;

  // VaapiPicture implementation.
  bool Allocate(gfx::BufferFormat format) override;
  bool ImportGpuMemoryBufferHandle(
      gfx::BufferFormat format,
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) override;

 private:
  bool Initialize();

  DISALLOW_COPY_AND_ASSIGN(VaapiPictureNativePixmapEgl);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_PICTURE_NATIVE_PIXMAP_EGL_H_
