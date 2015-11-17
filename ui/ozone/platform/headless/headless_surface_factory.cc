// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/headless_surface_factory.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/threading/worker_pool.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/platform/headless/headless_window.h"
#include "ui/ozone/platform/headless/headless_window_manager.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

namespace {

void WriteDataToFile(const base::FilePath& location, const SkBitmap& bitmap) {
  DCHECK(!location.empty());
  std::vector<unsigned char> png_data;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, true, &png_data);
  base::WriteFile(location,
                  reinterpret_cast<const char*>(vector_as_array(&png_data)),
                  png_data.size());
}

// TODO(altimin): Find a proper way to capture rendering output.
class FileSurface : public SurfaceOzoneCanvas {
 public:
  FileSurface(const base::FilePath& location) : location_(location) {}
  ~FileSurface() override {}

  // SurfaceOzoneCanvas overrides:
  void ResizeCanvas(const gfx::Size& viewport_size) override {
    surface_ = skia::AdoptRef(SkSurface::NewRaster(SkImageInfo::MakeN32Premul(
        viewport_size.width(), viewport_size.height())));
  }
  skia::RefPtr<SkSurface> GetSurface() override { return surface_; }
  void PresentCanvas(const gfx::Rect& damage) override {
    if (location_.empty())
      return;
    SkBitmap bitmap;
    bitmap.setInfo(surface_->getCanvas()->imageInfo());

    // TODO(dnicoara) Use SkImage instead to potentially avoid a copy.
    // See crbug.com/361605 for details.
    if (surface_->getCanvas()->readPixels(&bitmap, 0, 0)) {
      base::WorkerPool::PostTask(
          FROM_HERE, base::Bind(&WriteDataToFile, location_, bitmap), true);
    }
  }
  scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() override {
    return nullptr;
  }

 private:
  base::FilePath location_;
  skia::RefPtr<SkSurface> surface_;
};

}  // namespace

HeadlessSurfaceFactory::HeadlessSurfaceFactory()
    : HeadlessSurfaceFactory(nullptr) {}

HeadlessSurfaceFactory::HeadlessSurfaceFactory(
    HeadlessWindowManager* window_manager)
    : window_manager_(window_manager) {}

HeadlessSurfaceFactory::~HeadlessSurfaceFactory() {}

scoped_ptr<SurfaceOzoneCanvas> HeadlessSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  HeadlessWindow* window = window_manager_->GetWindow(widget);
  return make_scoped_ptr<SurfaceOzoneCanvas>(new FileSurface(window->path()));
}

bool HeadlessSurfaceFactory::LoadEGLGLES2Bindings(
    AddGLLibraryCallback add_gl_library,
    SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return false;
}

}  // namespace ui
