// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/caca/caca_surface_factory.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/ozone/surface_ozone_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/platform/caca/caca_connection.h"

namespace ui {

namespace {

const gfx::AcceleratedWidget kDefaultWidgetHandle = 1;

class CacaSurface : public gfx::SurfaceOzoneCanvas {
 public:
  CacaSurface(CacaConnection* connection);
  virtual ~CacaSurface();

  // gfx::SurfaceOzoneCanvas overrides:
  virtual skia::RefPtr<SkCanvas> GetCanvas() OVERRIDE;
  virtual bool ResizeCanvas(const gfx::Size& viewport_size) OVERRIDE;
  virtual bool PresentCanvas() OVERRIDE;
  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() OVERRIDE;

 private:
  CacaConnection* connection_;  // Not owned.

  caca_dither_t* dither_;

  skia::RefPtr<SkSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(CacaSurface);
};

CacaSurface::CacaSurface(CacaConnection* connection)
  : connection_(connection) {}

CacaSurface::~CacaSurface() {
  caca_free_dither(dither_);
}

bool CacaSurface::InitializeCanvas() {
  SkImageInfo info = SkImageInfo::Make(connection_->bitmap_size().width(),
                                       connection_->bitmap_size().height(),
                                       kPMColor_SkColorType,
                                       kPremul_SkAlphaType);

  surface_ = skia::AdoptRef(SkSurface::NewRaster(info));
  if (!surface_) {
    LOG(ERROR) << "Failed to create SkCanvas";
    return false;
  }

  dither_ = caca_create_dither(
      info.bytesPerPixel() * 8,
      info.width(),
      info.height(),
      SkBitmap::ComputeRowBytes(SkBitmap::kARGB_8888_Config, info.width()),
      0x00ff0000,
      0x0000ff00,
      0x000000ff,
      0xff000000);

  return true;
}

skia::RefPtr<SkCanvas> CacaSurface::GetCanvas() {
  return skia::SharePtr<SkCanvas>(surface_->getCanvas());
}

bool CacaSurface::ResizeCanvas(const gfx::Size& viewport_size) {
  NOTIMPLEMENTED();
  return false;
}

bool CacaSurface::PresentCanvas() {
  SkImageInfo info;
  size_t row_bytes;
  const void* pixels = surface_->peekPixels(&info, &row_bytes);

  caca_canvas_t* canvas = caca_get_canvas(connection_->display());
  caca_dither_bitmap(canvas, 0, 0,
                     caca_get_canvas_width(canvas),
                     caca_get_canvas_height(canvas),
                     dither_,
                     static_cast<const uint8_t*>(pixels));
  caca_refresh_display(connection_->display());

  return true;
}

scoped_ptr<gfx::VSyncProvider> CacaSurface::CreateVSyncProvider() {
  return scoped_ptr<gfx::VSyncProvider>();
}

}  // namespace

CacaSurfaceFactory::CacaSurfaceFactory(CacaConnection* connection)
    : state_(UNINITIALIZED),
      connection_(connection) {
}

CacaSurfaceFactory::~CacaSurfaceFactory() {
  if (state_ == INITIALIZED)
    ShutdownHardware();
}

gfx::SurfaceFactoryOzone::HardwareState
CacaSurfaceFactory::InitializeHardware() {
  connection_->Initialize();
  state_ = INITIALIZED;
  return state_;
}

void CacaSurfaceFactory::ShutdownHardware() {
  CHECK_EQ(INITIALIZED, state_);
  state_ = UNINITIALIZED;
}

gfx::AcceleratedWidget CacaSurfaceFactory::GetAcceleratedWidget() {
  return kDefaultWidgetHandle;
}

bool CacaSurfaceFactory::LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  NOTREACHED();
  return false;
}

scoped_ptr<gfx::SurfaceOzoneCanvas> CacaSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  CHECK_EQ(INITIALIZED, state_);
  CHECK_EQ(kDefaultWidgetHandle, widget);

  return make_scoped_ptr<gfx::SurfaceOzoneCanvas>(new CacaSurface(connection_));
}

}  // namespace ui
