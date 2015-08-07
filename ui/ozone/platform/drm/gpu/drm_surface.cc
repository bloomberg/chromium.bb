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

DrmSurface::DrmSurface(DrmWindow* window)
    : window_(window), weak_ptr_factory_(this) {
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

  HardwareDisplayController* controller = window_->GetController();
  if (!controller)
    return;

  // For the display buffers use the mode size since a |viewport_size| smaller
  // than the display size will not scanout.
  front_buffer_ = AllocateBuffer(controller->GetAllocationDrmDevice(),
                                 controller->GetModeSize());
  back_buffer_ = AllocateBuffer(controller->GetAllocationDrmDevice(),
                                controller->GetModeSize());
}

void DrmSurface::PresentCanvas(const gfx::Rect& damage) {
  DCHECK(base::MessageLoopForUI::IsCurrent());

  // Create a snapshot of the requested drawing. If we get here again before
  // presenting, just add the additional damage.
  pending_image_damage_.Union(damage);
  pending_image_ = skia::AdoptRef(surface_->newImageSnapshot());

  if (!pending_pageflip_)
    SchedulePageFlip();
}

scoped_ptr<gfx::VSyncProvider> DrmSurface::CreateVSyncProvider() {
  return make_scoped_ptr(new DrmVSyncProvider(window_));
}

void DrmSurface::SchedulePageFlip() {
  DCHECK(back_buffer_);
  SkCanvas* canvas = back_buffer_->GetCanvas();

  // The DrmSurface is double buffered, so the current back buffer is
  // missing the previous update. Expand damage region.
  SkRect real_damage =
      RectToSkRect(UnionRects(pending_image_damage_, last_damage_));

  // Copy damage region.
  canvas->drawImageRect(pending_image_.get(), real_damage, real_damage, NULL);
  last_damage_ = pending_image_damage_;

  pending_image_ = nullptr;
  pending_image_damage_ = gfx::Rect();

  window_->QueueOverlayPlane(OverlayPlane(back_buffer_));

  // Update our front buffer pointer.
  std::swap(front_buffer_, back_buffer_);
  pending_pageflip_ = window_->SchedulePageFlip(
      false /* is_sync */,
      base::Bind(&DrmSurface::OnPageFlip, weak_ptr_factory_.GetWeakPtr()));
}

void DrmSurface::OnPageFlip(gfx::SwapResult result) {
  pending_pageflip_ = false;
  if (!pending_image_)
    return;

  SchedulePageFlip();
}

}  // namespace ui
