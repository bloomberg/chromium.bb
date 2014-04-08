// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ozone/impl/file_surface_factory.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/threading/worker_pool.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/ozone/surface_ozone_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"

namespace gfx {

namespace {

void WriteDataToFile(const base::FilePath& location,
                     const SkBitmap& bitmap) {
  std::vector<unsigned char> png_data;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, true, &png_data);
  base::WriteFile(location,
                  reinterpret_cast<const char*>(vector_as_array(&png_data)),
                  png_data.size());
}

class FileSurface : public SurfaceOzoneCanvas {
 public:
  FileSurface(const base::FilePath& location) : location_(location) {}
  virtual ~FileSurface() {}

  virtual bool InitializeCanvas() OVERRIDE { return true; }

  virtual bool ResizeCanvas(const Size& viewport_size) OVERRIDE {
    SkImageInfo info = SkImageInfo::MakeN32Premul(viewport_size.width(),
                                                  viewport_size.height());
    device_ = skia::AdoptRef(SkBitmapDevice::Create(info));
    canvas_ = skia::AdoptRef(new SkCanvas(device_.get()));
    return true;
  }

  virtual skia::RefPtr<SkCanvas> GetCanvas() OVERRIDE { return canvas_; }

  virtual bool PresentCanvas() OVERRIDE {
    SkBitmap bitmap;
    bitmap.setConfig(
        SkBitmap::kARGB_8888_Config, device_->width(), device_->height());

    if (canvas_->readPixels(&bitmap, 0, 0)) {
      base::WorkerPool::PostTask(
          FROM_HERE, base::Bind(&WriteDataToFile, location_, bitmap), true);
    }
    return true;
  }

  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() OVERRIDE {
    return scoped_ptr<gfx::VSyncProvider>();
  }

 private:
  base::FilePath location_;
  skia::RefPtr<SkBitmapDevice> device_;
  skia::RefPtr<SkCanvas> canvas_;
};

}  // namespace

FileSurfaceFactory::FileSurfaceFactory(
    const base::FilePath& dump_location)
    : location_(dump_location) {
  CHECK(!base::DirectoryExists(location_))
      << "Location cannot be a directory (" << location_.value() << ")";
  CHECK(!base::PathExists(location_) || base::PathIsWritable(location_));
}

FileSurfaceFactory::~FileSurfaceFactory() {}

SurfaceFactoryOzone::HardwareState
FileSurfaceFactory::InitializeHardware() {
  return INITIALIZED;
}

void FileSurfaceFactory::ShutdownHardware() {
}

AcceleratedWidget FileSurfaceFactory::GetAcceleratedWidget() {
  return 1;
}

scoped_ptr<SurfaceOzoneCanvas> FileSurfaceFactory::CreateCanvasForWidget(
      gfx::AcceleratedWidget w) {
  return make_scoped_ptr<SurfaceOzoneCanvas>(new FileSurface(location_));
}

bool FileSurfaceFactory::LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return false;
}

}  // namespace gfx
