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

namespace {

void WriteDataToFile(const base::FilePath& location,
                     const SkBitmap& bitmap) {
  std::vector<unsigned char> png_data;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, true, &png_data);
  file_util::WriteFile(location,
                       (char*)vector_as_array(&png_data),
                       png_data.size());
}

}

namespace gfx {

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

AcceleratedWidget FileSurfaceFactory::RealizeAcceleratedWidget(
    AcceleratedWidget widget) {
  return 1;
}

bool FileSurfaceFactory::LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return false;
}

bool FileSurfaceFactory::AttemptToResizeAcceleratedWidget(
    AcceleratedWidget widget,
    const Rect& bounds) {
  device_ = skia::AdoptRef(new SkBitmapDevice(SkBitmap::kARGB_8888_Config,
                                              bounds.width(),
                                              bounds.height()));
  canvas_ = skia::AdoptRef(new SkCanvas(device_.get()));
  return true;
}

bool FileSurfaceFactory::SchedulePageFlip(AcceleratedWidget widget) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                   device_->width(),
                   device_->height());

  if (canvas_->readPixels(&bitmap, 0, 0)) {
    base::WorkerPool::PostTask(FROM_HERE,
        base::Bind(&WriteDataToFile, location_, bitmap),
        true);
  }
  return true;
}

SkCanvas* FileSurfaceFactory::GetCanvasForWidget(AcceleratedWidget w) {
  return canvas_.get();
}

VSyncProvider* FileSurfaceFactory::GetVSyncProvider(AcceleratedWidget w) {
  return NULL;
}

}  // namespace gfx
