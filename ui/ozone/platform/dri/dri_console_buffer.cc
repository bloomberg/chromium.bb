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
    : dri_(dri),
      handle_(0),
      framebuffer_(framebuffer),
      mmap_base_(NULL),
      mmap_size_(0) {
}

DriConsoleBuffer::~DriConsoleBuffer() {
  if (mmap_base_)
    if (munmap(mmap_base_, mmap_size_))
      PLOG(ERROR) << "munmap";
}

bool DriConsoleBuffer::Initialize() {
  ScopedDrmFramebufferPtr fb(dri_->GetFramebuffer(framebuffer_));

  if (!fb)
    return false;

  handle_ = fb->handle;
  stride_ = fb->pitch;
  SkImageInfo info = SkImageInfo::MakeN32Premul(fb->width, fb->height);

  mmap_size_ = info.getSafeSize(stride_);

  if (!MapDumbBuffer(dri_->get_fd(), fb->handle, mmap_size_, &mmap_base_)) {
    mmap_base_ = NULL;
    return false;
  }

  surface_ =
      skia::AdoptRef(SkSurface::NewRasterDirect(info, mmap_base_, stride_));
  if (!surface_)
    return false;

  return true;
}

}  // namespace ui
