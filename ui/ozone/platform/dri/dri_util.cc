// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_util.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace ui {

namespace {

bool IsCrtcInUse(uint32_t crtc,
                 const ScopedVector<HardwareDisplayControllerInfo>& displays) {
  for (size_t i = 0; i < displays.size(); ++i) {
    if (crtc == displays[i]->crtc()->crtc_id)
      return true;
  }

  return false;
}

uint32_t GetCrtc(int fd,
                 drmModeConnector* connector,
                 drmModeRes* resources,
                 const ScopedVector<HardwareDisplayControllerInfo>& displays) {
  // If the connector already has an encoder try to re-use.
  if (connector->encoder_id) {
    ScopedDrmEncoderPtr encoder(drmModeGetEncoder(fd, connector->encoder_id));
    if (encoder && encoder->crtc_id && !IsCrtcInUse(encoder->crtc_id, displays))
      return encoder->crtc_id;
  }

  // Try to find an encoder for the connector.
  for (int i = 0; i < connector->count_encoders; ++i) {
    ScopedDrmEncoderPtr encoder(drmModeGetEncoder(fd, connector->encoders[i]));
    if (!encoder)
      continue;

    for (int j = 0; j < resources->count_crtcs; ++j) {
      // Check if the encoder is compatible with this CRTC
      if (!(encoder->possible_crtcs & (1 << j)) ||
          IsCrtcInUse(resources->crtcs[j], displays))
        continue;

      return resources->crtcs[j];
    }
  }

  return 0;
}

}  // namespace

HardwareDisplayControllerInfo::HardwareDisplayControllerInfo(
    ScopedDrmConnectorPtr connector,
    ScopedDrmCrtcPtr crtc)
    : connector_(connector.Pass()),
      crtc_(crtc.Pass()) {}

HardwareDisplayControllerInfo::~HardwareDisplayControllerInfo() {}

ScopedVector<HardwareDisplayControllerInfo>
GetAvailableDisplayControllerInfos(int fd) {
  ScopedDrmResourcesPtr resources(drmModeGetResources(fd));
  DCHECK(resources) << "Failed to get DRM resources";
  ScopedVector<HardwareDisplayControllerInfo> displays;

  for (int i = 0; i < resources->count_connectors; ++i) {
    ScopedDrmConnectorPtr connector(drmModeGetConnector(
        fd, resources->connectors[i]));

    if (!connector || connector->connection != DRM_MODE_CONNECTED ||
        connector->count_modes == 0)
      continue;

    uint32_t crtc_id = GetCrtc(fd, connector.get(), resources.get(), displays);
    if (!crtc_id)
      continue;

    ScopedDrmCrtcPtr crtc(drmModeGetCrtc(fd, crtc_id));
    displays.push_back(new HardwareDisplayControllerInfo(connector.Pass(),
                                                         crtc.Pass()));
  }

  return displays.Pass();
}

bool SameMode(const drmModeModeInfo& lhs, const drmModeModeInfo& rhs) {
  return lhs.clock == rhs.clock &&
         lhs.hdisplay == rhs.hdisplay &&
         lhs.vdisplay == rhs.vdisplay &&
         lhs.vrefresh == rhs.vrefresh &&
         lhs.hsync_start == rhs.hsync_start &&
         lhs.hsync_end == rhs.hsync_end &&
         lhs.htotal == rhs.htotal &&
         lhs.hskew == rhs.hskew &&
         lhs.vsync_start == rhs.vsync_start &&
         lhs.vsync_end == rhs.vsync_end &&
         lhs.vtotal == rhs.vtotal &&
         lhs.vscan == rhs.vscan &&
         lhs.flags == rhs.flags &&
         strcmp(lhs.name, rhs.name) == 0;
}

bool MapDumbBuffer(int fd,
                   uint32_t handle,
                   uint32_t size,
                   void** pixels) {
  struct drm_mode_map_dumb map_request;
  memset(&map_request, 0, sizeof(map_request));
  map_request.handle = handle;
  if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_request)) {
    VLOG(2) << "Cannot prepare dumb buffer for mapping (" << errno << ") "
            << strerror(errno);
    return false;
  }

  *pixels = mmap(0,
                 size,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED,
                 fd,
                 map_request.offset);
  if (*pixels == MAP_FAILED) {
    VLOG(2) << "Cannot mmap dumb buffer (" << errno << ") " << strerror(errno);
    return false;
  }

  return true;
}

}  // namespace ui
