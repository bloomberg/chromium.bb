// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ozone/impl/drm_wrapper_ozone.h"

#include <fcntl.h>
#include <unistd.h>
#include <xf86drmMode.h>

#include "base/logging.h"

namespace gfx {

DrmWrapperOzone::DrmWrapperOzone(const char* device_path) {
  fd_ = open(device_path, O_RDWR | O_CLOEXEC);
}

DrmWrapperOzone::~DrmWrapperOzone() {
  if (fd_ >= 0)
    close(fd_);
}

drmModeCrtc* DrmWrapperOzone::GetCrtc(uint32_t crtc_id) {
  CHECK(fd_ >= 0);
  return drmModeGetCrtc(fd_, crtc_id);
}

void DrmWrapperOzone::FreeCrtc(drmModeCrtc* crtc) {
  drmModeFreeCrtc(crtc);
}

bool DrmWrapperOzone::SetCrtc(uint32_t crtc_id,
                              uint32_t framebuffer,
                              uint32_t* connectors,
                              drmModeModeInfo* mode) {
  CHECK(fd_ >= 0);
  return !drmModeSetCrtc(fd_, crtc_id, framebuffer, 0, 0, connectors, 1, mode);
}

bool DrmWrapperOzone::SetCrtc(drmModeCrtc* crtc, uint32_t* connectors) {
  CHECK(fd_ >= 0);
  return !drmModeSetCrtc(fd_,
                         crtc->crtc_id,
                         crtc->buffer_id,
                         crtc->x,
                         crtc->y,
                         connectors,
                         1,
                         &crtc->mode);
}

bool DrmWrapperOzone::AddFramebuffer(const drmModeModeInfo& mode,
                                     uint8_t depth,
                                     uint8_t bpp,
                                     uint32_t stride,
                                     uint32_t handle,
                                     uint32_t* framebuffer) {
  CHECK(fd_ >= 0);
  return !drmModeAddFB(fd_,
                       mode.hdisplay,
                       mode.vdisplay,
                       depth,
                       bpp,
                       stride,
                       handle,
                       framebuffer);
}

bool DrmWrapperOzone::RemoveFramebuffer(uint32_t framebuffer) {
  CHECK(fd_ >= 0);
  return !drmModeRmFB(fd_, framebuffer);
}

bool DrmWrapperOzone::PageFlip(uint32_t crtc_id,
                               uint32_t framebuffer,
                               void* data) {
  CHECK(fd_ >= 0);
  return !drmModePageFlip(fd_,
                          crtc_id,
                          framebuffer,
                          DRM_MODE_PAGE_FLIP_EVENT,
                          data);
}

}  // namespace gfx
