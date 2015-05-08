// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_util.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <xf86drmMode.h>

#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

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
    : connector_(connector.Pass()), crtc_(crtc.Pass()) {
}

HardwareDisplayControllerInfo::~HardwareDisplayControllerInfo() {
}

ScopedVector<HardwareDisplayControllerInfo> GetAvailableDisplayControllerInfos(
    int fd) {
  ScopedDrmResourcesPtr resources(drmModeGetResources(fd));
  DCHECK(resources) << "Failed to get DRM resources";
  ScopedVector<HardwareDisplayControllerInfo> displays;

  for (int i = 0; i < resources->count_connectors; ++i) {
    ScopedDrmConnectorPtr connector(
        drmModeGetConnector(fd, resources->connectors[i]));

    if (!connector || connector->connection != DRM_MODE_CONNECTED ||
        connector->count_modes == 0)
      continue;

    uint32_t crtc_id = GetCrtc(fd, connector.get(), resources.get(), displays);
    if (!crtc_id)
      continue;

    ScopedDrmCrtcPtr crtc(drmModeGetCrtc(fd, crtc_id));
    displays.push_back(
        new HardwareDisplayControllerInfo(connector.Pass(), crtc.Pass()));
  }

  return displays.Pass();
}

bool SameMode(const drmModeModeInfo& lhs, const drmModeModeInfo& rhs) {
  return lhs.clock == rhs.clock && lhs.hdisplay == rhs.hdisplay &&
         lhs.vdisplay == rhs.vdisplay && lhs.vrefresh == rhs.vrefresh &&
         lhs.hsync_start == rhs.hsync_start && lhs.hsync_end == rhs.hsync_end &&
         lhs.htotal == rhs.htotal && lhs.hskew == rhs.hskew &&
         lhs.vsync_start == rhs.vsync_start && lhs.vsync_end == rhs.vsync_end &&
         lhs.vtotal == rhs.vtotal && lhs.vscan == rhs.vscan &&
         lhs.flags == rhs.flags && strcmp(lhs.name, rhs.name) == 0;
}

void ForceInitializationOfPrimaryDisplay(const scoped_refptr<DrmDevice>& drm,
                                         ScreenManager* screen_manager) {
  VLOG(2) << "Forcing initialization of primary display.";
  ScopedVector<HardwareDisplayControllerInfo> displays =
      GetAvailableDisplayControllerInfos(drm->get_fd());

  if (displays.empty())
    return;

  screen_manager->AddDisplayController(drm, displays[0]->crtc()->crtc_id,
                                       displays[0]->connector()->connector_id);
  screen_manager->ConfigureDisplayController(
      drm, displays[0]->crtc()->crtc_id, displays[0]->connector()->connector_id,
      gfx::Point(), displays[0]->connector()->modes[0]);
}

}  // namespace ui
