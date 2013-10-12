// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ozone/impl/software_surface_ozone.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <xf86drm.h>

#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/ozone/impl/drm_skbitmap_ozone.h"
#include "ui/gfx/ozone/impl/hardware_display_controller_ozone.h"
#include "ui/gfx/skia_util.h"

namespace gfx {

namespace {

// Extends the SkBitmapDevice to allow setting the SkPixelRef. We use the setter
// to change the SkPixelRef such that the device always points to the
// backbuffer.
class CustomSkBitmapDevice : public SkBitmapDevice {
 public:
  CustomSkBitmapDevice(const SkBitmap& bitmap) : SkBitmapDevice(bitmap) {}
  virtual ~CustomSkBitmapDevice() {}

  void SetPixelRef(SkPixelRef* pixel_ref) { setPixelRef(pixel_ref, 0); }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomSkBitmapDevice);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SoftwareSurfaceOzone implementation

SoftwareSurfaceOzone::SoftwareSurfaceOzone(
    HardwareDisplayControllerOzone* controller)
    : controller_(controller),
      bitmaps_(),
      front_buffer_(0) {
}

SoftwareSurfaceOzone::~SoftwareSurfaceOzone() {
}

bool SoftwareSurfaceOzone::Initialize() {
  for (int i = 0; i < 2; ++i) {
    bitmaps_[i].reset(CreateBuffer());
    // TODO(dnicoara) Should select the configuration based on what the
    // underlying system supports.
    bitmaps_[i]->setConfig(SkBitmap::kARGB_8888_Config,
                           controller_->get_mode().hdisplay,
                           controller_->get_mode().vdisplay);

    if (!bitmaps_[i]->Initialize()) {
      return false;
    }
  }

  skia_device_ = skia::AdoptRef(
      new SkBitmapDevice(*bitmaps_[front_buffer_ ^ 1].get()));
  skia_canvas_ = skia::AdoptRef(new SkCanvas(skia_device_.get()));

  return true;
}

uint32_t SoftwareSurfaceOzone::GetFramebufferId() const {
  CHECK(bitmaps_[0].get() && bitmaps_[1].get());
  return bitmaps_[front_buffer_ ^ 1]->get_framebuffer();
}

// This call is made after the hardware just started displaying our back buffer.
// We need to update our pointer reference and synchronize the two buffers.
void SoftwareSurfaceOzone::SwapBuffers() {
  CHECK(bitmaps_[0].get() && bitmaps_[1].get());

  // Update our front buffer pointer.
  front_buffer_ ^= 1;

  // Unlocking will unset the pixel pointer, so it won't be pointing to the old
  // PixelRef.
  skia_device_->accessBitmap(false).unlockPixels();
  // Update the backing pixels for the bitmap device.
  static_cast<CustomSkBitmapDevice*>(skia_device_.get())->SetPixelRef(
      bitmaps_[front_buffer_ ^ 1]->pixelRef());
  // Locking the pixels will set the pixel pointer based on the PixelRef value.
  skia_device_->accessBitmap(false).lockPixels();

  SkIRect device_damage;
  skia_canvas_->getClipDeviceBounds(&device_damage);
  SkRect damage = SkRect::Make(device_damage);

  skia_canvas_->drawBitmapRectToRect(*bitmaps_[front_buffer_].get(),
                                     &damage,
                                     damage);
}

SkCanvas* SoftwareSurfaceOzone::GetDrawableForWidget() {
  return skia_canvas_.get();
}

DrmSkBitmapOzone* SoftwareSurfaceOzone::CreateBuffer() {
  return new DrmSkBitmapOzone(controller_->get_fd());
}

}  // namespace gfx
