// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_console_buffer.h"

#include <sys/mman.h>
#include <xf86drmMode.h>

#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/ozone/platform/dri/dri_util.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/scoped_drm_types.h"

namespace ui {

DriConsoleBuffer::DriConsoleBuffer(DriWrapper* dri, uint32_t framebuffer)
    : dri_(dri), handle_(0), framebuffer_(framebuffer) {}

DriConsoleBuffer::~DriConsoleBuffer() {
  SkImageInfo info;
  void* pixels = const_cast<void*>(surface_->peekPixels(&info, NULL));
  if (!pixels)
    return;

  munmap(pixels, info.getSafeSize(stride_));
}

bool DriConsoleBuffer::Initialize() {
  ScopedDrmFramebufferPtr fb(dri_->GetFramebuffer(framebuffer_));

  if (!fb)
    return false;

  handle_ = fb->handle;
  stride_ = fb->pitch;
  void* pixels = NULL;
  SkImageInfo info = SkImageInfo::MakeN32Premul(fb->width, fb->height);

  if (!MapDumbBuffer(dri_->get_fd(),
                     fb->handle,
                     info.getSafeSize(stride_),
                     &pixels))
    return false;

  surface_ = skia::AdoptRef(SkSurface::NewRasterDirect(info, pixels, stride_));
  if (!surface_)
    return false;

  return true;
}

}  // namespace ui
