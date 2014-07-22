// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/test/test_window_manager.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/threading/worker_pool.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

namespace {

void WriteDataToFile(const base::FilePath& location, const SkBitmap& bitmap) {
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

  // SurfaceOzoneCanvas overrides:
  virtual void ResizeCanvas(const gfx::Size& viewport_size) OVERRIDE {
    surface_ = skia::AdoptRef(SkSurface::NewRaster(SkImageInfo::MakeN32Premul(
        viewport_size.width(), viewport_size.height())));
  }
  virtual skia::RefPtr<SkCanvas> GetCanvas() OVERRIDE {
    return skia::SharePtr(surface_->getCanvas());
  }
  virtual void PresentCanvas(const gfx::Rect& damage) OVERRIDE {
    SkBitmap bitmap;
    bitmap.setInfo(surface_->getCanvas()->imageInfo());

    // TODO(dnicoara) Use SkImage instead to potentially avoid a copy.
    // See crbug.com/361605 for details.
    if (surface_->getCanvas()->readPixels(&bitmap, 0, 0)) {
      base::WorkerPool::PostTask(
          FROM_HERE, base::Bind(&WriteDataToFile, location_, bitmap), true);
    }
  }
  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() OVERRIDE {
    return scoped_ptr<gfx::VSyncProvider>();
  }

 private:
  base::FilePath location_;
  skia::RefPtr<SkSurface> surface_;
};

}  // namespace

TestWindowManager::TestWindowManager(const base::FilePath& dump_location)
    : location_(dump_location) {
}

TestWindowManager::~TestWindowManager() {
}

void TestWindowManager::Initialize() {
  if (!DirectoryExists(location_) && !base::CreateDirectory(location_) &&
      location_ != base::FilePath("/dev/null"))
    PLOG(FATAL) << "unable to create output directory";
  if (!base::PathIsWritable(location_))
    PLOG(FATAL) << "unable to write to output location";
}

int32_t TestWindowManager::AddWindow(TestWindow* window) {
  return windows_.Add(window);
}

void TestWindowManager::RemoveWindow(int32_t window_id, TestWindow* window) {
  DCHECK_EQ(window, windows_.Lookup(window_id));
  windows_.Remove(window_id);
}

base::FilePath TestWindowManager::base_path() const {
  return location_;
}

scoped_ptr<SurfaceOzoneCanvas> TestWindowManager::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  TestWindow* window = windows_.Lookup(widget);
  DCHECK(window);
  return make_scoped_ptr<SurfaceOzoneCanvas>(new FileSurface(window->path()));
}

bool TestWindowManager::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return false;
}

}  // namespace ui
