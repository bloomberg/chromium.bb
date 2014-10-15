// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/gpu/gpu_memory_buffer_factory_ozone_native_buffer.h"

#include "base/logging.h"
#include "ui/gl/gl_image_egl.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {
namespace {
class GLImageOzoneNativePixmap : public gfx::GLImageEGL {
 public:
  explicit GLImageOzoneNativePixmap(const gfx::Size& size) : GLImageEGL(size) {}

  bool Initialize(scoped_refptr<NativePixmap> pixmap) {
    EGLint attrs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    if (!Initialize(EGL_NATIVE_PIXMAP_KHR, pixmap->GetEGLClientBuffer(), attrs))
      return false;
    pixmap_ = pixmap;
    return true;
  }

  virtual bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                    int z_order,
                                    gfx::OverlayTransform transform,
                                    const gfx::Rect& bounds_rect,
                                    const gfx::RectF& crop_rect) override {
    return SurfaceFactoryOzone::GetInstance()->ScheduleOverlayPlane(
        widget, z_order, transform, pixmap_, bounds_rect, crop_rect);
  }

 protected:
  virtual ~GLImageOzoneNativePixmap() {}

 private:
  using gfx::GLImageEGL::Initialize;
  scoped_refptr<NativePixmap> pixmap_;
};

SurfaceFactoryOzone::BufferFormat GetOzoneFormatFor(
    gfx::GpuMemoryBuffer::Format format) {
  switch (format) {
    case gfx::GpuMemoryBuffer::RGBA_8888:
      return SurfaceFactoryOzone::RGBA_8888;
    case gfx::GpuMemoryBuffer::RGBX_8888:
      return SurfaceFactoryOzone::RGBX_8888;
    case gfx::GpuMemoryBuffer::BGRA_8888:
      NOTREACHED();
      return SurfaceFactoryOzone::RGBA_8888;
  }

  NOTREACHED();
  return SurfaceFactoryOzone::RGBA_8888;
}

std::pair<uint32_t, uint32_t> GetIndex(const gfx::GpuMemoryBufferId& id) {
  return std::pair<uint32_t, uint32_t>(id.primary_id, id.secondary_id);
}
}  // namespace

GpuMemoryBufferFactoryOzoneNativeBuffer::
    GpuMemoryBufferFactoryOzoneNativeBuffer() {
}

GpuMemoryBufferFactoryOzoneNativeBuffer::
    ~GpuMemoryBufferFactoryOzoneNativeBuffer() {
}

bool GpuMemoryBufferFactoryOzoneNativeBuffer::CreateGpuMemoryBuffer(
    const gfx::GpuMemoryBufferId& id,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage) {
  scoped_refptr<NativePixmap> pixmap =
      SurfaceFactoryOzone::GetInstance()->CreateNativePixmap(
          size, GetOzoneFormatFor(format));
  if (!pixmap.get()) {
    LOG(ERROR) << "Failed to create pixmap " << size.width() << "x"
               << size.height() << " format " << format << ", usage " << usage;
    return false;
  }
  native_pixmap_map_[GetIndex(id)] = pixmap;
  return true;
}

void GpuMemoryBufferFactoryOzoneNativeBuffer::DestroyGpuMemoryBuffer(
    const gfx::GpuMemoryBufferId& id) {
  native_pixmap_map_.erase(GetIndex(id));
}

scoped_refptr<gfx::GLImage>
GpuMemoryBufferFactoryOzoneNativeBuffer::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferId& id,
    const gfx::Size& size,
    unsigned internalformat) {
  BufferToPixmapMap::iterator it = native_pixmap_map_.find(GetIndex(id));
  if (it == native_pixmap_map_.end()) {
    return scoped_refptr<gfx::GLImage>();
  }
  scoped_refptr<GLImageOzoneNativePixmap> image =
      new GLImageOzoneNativePixmap(size);
  if (!image->Initialize(it->second)) {
    return scoped_refptr<gfx::GLImage>();
  }
  return image;
}

}  // namespace ui
