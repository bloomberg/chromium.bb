// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_surface.h"

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/ozone/platform/drm/gpu/drm_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_vsync_provider.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

namespace ui {

namespace {

scoped_refptr<DrmBuffer> AllocateBuffer(const scoped_refptr<DrmDevice>& drm,
                                        const gfx::Size& size) {
  scoped_refptr<DrmBuffer> buffer(new DrmBuffer(drm));
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());

  bool initialized =
      buffer->Initialize(info, true /* should_register_framebuffer */);
  DCHECK(initialized) << "Failed to create drm buffer.";

  return buffer;
}

}  // namespace

DrmSurface::DrmSurface(DrmWindow* window_delegate)
    : window_delegate_(window_delegate), buffers_(), front_buffer_(0) {
}

DrmSurface::~DrmSurface() {
}

skia::RefPtr<SkSurface> DrmSurface::GetSurface() {
  return surface_;
}

void DrmSurface::ResizeCanvas(const gfx::Size& viewport_size) {
  SkImageInfo info = SkImageInfo::MakeN32(
      viewport_size.width(), viewport_size.height(), kOpaque_SkAlphaType);
  surface_ = skia::AdoptRef(SkSurface::NewRaster(info));

  HardwareDisplayController* controller = window_delegate_->GetController();
  if (!controller)
    return;

  // For the display buffers use the mode size since a |viewport_size| smaller
  // than the display size will not scanout.
  for (size_t i = 0; i < arraysize(buffers_); ++i)
    buffers_[i] = AllocateBuffer(controller->GetAllocationDrmDevice(),
                                 controller->GetModeSize());
}

void DrmSurface::PresentCanvas(const gfx::Rect& damage) {
  DCHECK(base::MessageLoopForUI::IsCurrent());

  HardwareDisplayController* controller = window_delegate_->GetController();
  if (!controller)
    return;

  DCHECK(buffers_[front_buffer_ ^ 1].get());
  controller->QueueOverlayPlane(OverlayPlane(buffers_[front_buffer_ ^ 1]));

  UpdateNativeSurface(damage);
  controller->SchedulePageFlip(false /* is_sync */,
                               base::Bind(&base::DoNothing));

  // Update our front buffer pointer.
  front_buffer_ ^= 1;
}

scoped_ptr<gfx::VSyncProvider> DrmSurface::CreateVSyncProvider() {
  return make_scoped_ptr(new DrmVSyncProvider(window_delegate_));
}

void DrmSurface::UpdateNativeSurface(const gfx::Rect& damage) {
  SkCanvas* canvas = buffers_[front_buffer_ ^ 1]->GetCanvas();

  // The DrmSurface is double buffered, so the current back buffer is
  // missing the previous update. Expand damage region.
  SkRect real_damage = RectToSkRect(UnionRects(damage, last_damage_));

  // Copy damage region.
  skia::RefPtr<SkImage> image = skia::AdoptRef(surface_->newImageSnapshot());
  canvas->drawImageRect(image.get(), &real_damage, real_damage, NULL);

  last_damage_ = damage;
}

}  // namespace ui
