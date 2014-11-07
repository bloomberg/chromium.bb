// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/gpu/gpu_memory_buffer_factory_ozone_native_buffer.h"

#include "base/logging.h"
#include "ui/gl/gl_image_egl.h"
#include "ui/gl/gl_image_linux_dma_buffer.h"
#include "ui/ozone/public/native_pixmap.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_egl.h"

namespace ui {
namespace {
class GLImageOzoneNativePixmap : public gfx::GLImageEGL {
 public:
  explicit GLImageOzoneNativePixmap(const gfx::Size& size) : GLImageEGL(size) {}

  bool Initialize(NativePixmap* pixmap) {
    EGLint attrs[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    if (!Initialize(EGL_NATIVE_PIXMAP_KHR, pixmap->GetEGLClientBuffer(), attrs))
      return false;
    pixmap_ = pixmap;
    return true;
  }

  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override {
    return SurfaceFactoryOzone::GetInstance()->ScheduleOverlayPlane(
        widget, z_order, transform, pixmap_, bounds_rect, crop_rect);
  }

 protected:
  ~GLImageOzoneNativePixmap() override {}

 private:
  using gfx::GLImageEGL::Initialize;
  scoped_refptr<NativePixmap> pixmap_;
};

class GLImageOzoneNativePixmapDmaBuf : public gfx::GLImageLinuxDMABuffer {
 public:
  explicit GLImageOzoneNativePixmapDmaBuf(const gfx::Size& size,
                                          unsigned internalformat)
      : GLImageLinuxDMABuffer(size, internalformat) {}

  bool Initialize(NativePixmap* pixmap, gfx::GpuMemoryBuffer::Format format) {
    base::FileDescriptor handle(pixmap->GetDmaBufFd(), false);
    if (!GLImageLinuxDMABuffer::Initialize(handle, format,
                                           pixmap->GetDmaBufPitch()))
      return false;
    pixmap_ = pixmap;
    return true;
  }

  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override {
    return SurfaceFactoryOzone::GetInstance()->ScheduleOverlayPlane(
        widget, z_order, transform, pixmap_, bounds_rect, crop_rect);
  }

 protected:
  ~GLImageOzoneNativePixmapDmaBuf() override {}

 private:
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

std::pair<uint32_t, uint32_t> GetIndex(gfx::GpuMemoryBufferId id,
                                       int client_id) {
  return std::pair<uint32_t, uint32_t>(id, client_id);
}
}  // namespace

GpuMemoryBufferFactoryOzoneNativeBuffer::
    GpuMemoryBufferFactoryOzoneNativeBuffer() {
}

GpuMemoryBufferFactoryOzoneNativeBuffer::
    ~GpuMemoryBufferFactoryOzoneNativeBuffer() {
}

bool GpuMemoryBufferFactoryOzoneNativeBuffer::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    gfx::GpuMemoryBuffer::Usage usage,
    int client_id) {
  scoped_refptr<NativePixmap> pixmap =
      SurfaceFactoryOzone::GetInstance()->CreateNativePixmap(
          size, GetOzoneFormatFor(format));
  if (!pixmap.get()) {
    LOG(ERROR) << "Failed to create pixmap " << size.width() << "x"
               << size.height() << " format " << format << ", usage " << usage;
    return false;
  }
  native_pixmap_map_[GetIndex(id, client_id)] = pixmap;
  return true;
}

void GpuMemoryBufferFactoryOzoneNativeBuffer::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    int client_id) {
  native_pixmap_map_.erase(GetIndex(id, client_id));
}

scoped_refptr<gfx::GLImage>
GpuMemoryBufferFactoryOzoneNativeBuffer::CreateImageForGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::GpuMemoryBuffer::Format format,
    unsigned internalformat,
    int client_id) {
  BufferToPixmapMap::iterator it =
      native_pixmap_map_.find(GetIndex(id, client_id));
  if (it == native_pixmap_map_.end()) {
    return scoped_refptr<gfx::GLImage>();
  }
  NativePixmap* pixmap = it->second.get();
  if (pixmap->GetEGLClientBuffer()) {
    DCHECK_EQ(-1, pixmap->GetDmaBufFd());
    scoped_refptr<GLImageOzoneNativePixmap> image =
        new GLImageOzoneNativePixmap(size);
    if (!image->Initialize(pixmap)) {
      return scoped_refptr<gfx::GLImage>();
    }
    return image;
  }
  if (pixmap->GetDmaBufFd() > 0) {
    scoped_refptr<GLImageOzoneNativePixmapDmaBuf> image =
        new GLImageOzoneNativePixmapDmaBuf(size, internalformat);
    if (!image->Initialize(pixmap, format)) {
      return scoped_refptr<gfx::GLImage>();
    }
    return image;
  }
  return scoped_refptr<gfx::GLImage>();
}

}  // namespace ui
