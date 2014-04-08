// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ozone/dri/dri_surface.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <xf86drm.h>

#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/ozone/dri/dri_buffer.h"
#include "ui/gfx/ozone/dri/dri_wrapper.h"
#include "ui/gfx/skia_util.h"

namespace gfx {

namespace {

// TODO(dnicoara) Remove this once Skia implements this between 2 SkCanvases.
void CopyRect(DriBuffer* dst, DriBuffer* src, const gfx::Rect& damage) {
  SkImageInfo src_info, dst_info;
  size_t src_stride, dst_stride;
  uint8_t* src_pixels = static_cast<uint8_t*>(
      const_cast<void*>(src->canvas()->peekPixels(&src_info, &src_stride)));
  uint8_t* dst_pixels = static_cast<uint8_t*>(
      const_cast<void*>(dst->canvas()->peekPixels(&dst_info, &dst_stride)));

  // The 2 buffers should have the same properties.
  DCHECK(src_info == dst_info);
  DCHECK(src_stride == dst_stride);

  int bpp = src_info.bytesPerPixel();
  for (int height = damage.y(); height < damage.y() + damage.height(); ++height)
    memcpy(dst_pixels + height * dst_stride + damage.x() * bpp,
           src_pixels + height * src_stride + damage.x() * bpp,
           damage.width() * bpp);
}

}  // namespace

DriSurface::DriSurface(
    DriWrapper* dri, const gfx::Size& size)
    : dri_(dri),
      bitmaps_(),
      front_buffer_(0),
      size_(size) {
}

DriSurface::~DriSurface() {
}

bool DriSurface::Initialize() {
  for (size_t i = 0; i < arraysize(bitmaps_); ++i) {
    bitmaps_[i].reset(CreateBuffer());
    // TODO(dnicoara) Should select the configuration based on what the
    // underlying system supports.
    SkImageInfo info = SkImageInfo::Make(size_.width(),
                                         size_.height(),
                                         kPMColor_SkColorType,
                                         kPremul_SkAlphaType);
    if (!bitmaps_[i]->Initialize(info)) {
      return false;
    }
  }

  return true;
}

uint32_t DriSurface::GetFramebufferId() const {
  CHECK(backbuffer());
  return backbuffer()->framebuffer();
}

uint32_t DriSurface::GetHandle() const {
  CHECK(backbuffer());
  return backbuffer()->handle();
}

// This call is made after the hardware just started displaying our back buffer.
// We need to update our pointer reference and synchronize the two buffers.
void DriSurface::SwapBuffers() {
  CHECK(frontbuffer());
  CHECK(backbuffer());

  // Update our front buffer pointer.
  front_buffer_ ^= 1;

  SkIRect device_damage;
  frontbuffer()->canvas()->getClipDeviceBounds(&device_damage);
  CopyRect(backbuffer(), frontbuffer(), SkIRectToRect(device_damage));
}

SkCanvas* DriSurface::GetDrawableForWidget() {
  CHECK(backbuffer());
  return backbuffer()->canvas();
}

DriBuffer* DriSurface::CreateBuffer() { return new DriBuffer(dri_); }

}  // namespace gfx
