// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/screen_manager.h"

#include <xf86drmMode.h>

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/dri/crtc_state.h"
#include "ui/ozone/platform/dri/dri_util.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"
#include "ui/ozone/platform/dri/scanout_buffer.h"

namespace ui {

ScreenManager::ScreenManager(DriWrapper* dri,
                             ScanoutBufferGenerator* buffer_generator)
    : dri_(dri), buffer_generator_(buffer_generator) {
}

ScreenManager::~ScreenManager() {
}

void ScreenManager::AddDisplayController(uint32_t crtc, uint32_t connector) {
  HardwareDisplayControllers::iterator it = FindDisplayController(crtc);
  // TODO(dnicoara): Turn this into a DCHECK when async display configuration is
  // properly supported. (When there can't be a race between forcing initial
  // display configuration in ScreenManager and NativeDisplayDelegate creating
  // the display controllers.)
  if (it != controllers_.end()) {
    LOG(WARNING) << "Display controller (crtc=" << crtc << ") already present.";
    return;
  }

  controllers_.push_back(new HardwareDisplayController(
      dri_, scoped_ptr<CrtcState>(new CrtcState(dri_, crtc, connector))));
}

void ScreenManager::RemoveDisplayController(uint32_t crtc) {
  HardwareDisplayControllers::iterator it = FindDisplayController(crtc);
  if (it != controllers_.end()) {
    bool is_mirrored = (*it)->IsMirrored();
    (*it)->RemoveCrtc(crtc);
    if (!is_mirrored)
      controllers_.erase(it);
  }
}

bool ScreenManager::ConfigureDisplayController(uint32_t crtc,
                                               uint32_t connector,
                                               const gfx::Point& origin,
                                               const drmModeModeInfo& mode) {
  gfx::Rect modeset_bounds(
      origin.x(), origin.y(), mode.hdisplay, mode.vdisplay);
  HardwareDisplayControllers::iterator it = FindDisplayController(crtc);
  DCHECK(controllers_.end() != it) << "Display controller (crtc=" << crtc
                                   << ") doesn't exist.";

  HardwareDisplayController* controller = *it;
  controller = *it;
  // If nothing changed just enable the controller. Note, we perform an exact
  // comparison on the mode since the refresh rate may have changed.
  if (SameMode(mode, controller->get_mode()) &&
      origin == controller->origin() && !controller->IsDisabled())
    return controller->Enable();

  // Either the mode or the location of the display changed, so exit mirror
  // mode and configure the display independently. If the caller still wants
  // mirror mode, subsequent calls configuring the other controllers will
  // restore mirror mode.
  if (controller->IsMirrored()) {
    controller =
        new HardwareDisplayController(dri_, controller->RemoveCrtc(crtc));
    controllers_.push_back(controller);
    it = controllers_.end() - 1;
  }

  HardwareDisplayControllers::iterator mirror =
      FindActiveDisplayControllerByLocation(modeset_bounds);
  // Handle mirror mode.
  if (mirror != controllers_.end() && it != mirror)
    return HandleMirrorMode(it, mirror, crtc, connector);

  return ModesetDisplayController(controller, origin, mode);
}

bool ScreenManager::DisableDisplayController(uint32_t crtc) {
  HardwareDisplayControllers::iterator it = FindDisplayController(crtc);
  if (it != controllers_.end()) {
    if ((*it)->IsMirrored()) {
      HardwareDisplayController* controller =
          new HardwareDisplayController(dri_, (*it)->RemoveCrtc(crtc));
      controllers_.push_back(controller);
    }

    (*it)->Disable();
    return true;
  }

  LOG(ERROR) << "Failed to find display controller crtc=" << crtc;
  return false;
}

base::WeakPtr<HardwareDisplayController> ScreenManager::GetDisplayController(
    const gfx::Rect& bounds) {
  // TODO(dnicoara): Remove hack once TestScreen uses a simple Ozone display
  // configuration reader and ScreenManager is called from there to create the
  // one display needed by the content_shell target.
  if (controllers_.empty())
    ForceInitializationOfPrimaryDisplay();

  HardwareDisplayControllers::iterator it =
      FindActiveDisplayControllerByLocation(bounds);
  if (it != controllers_.end())
    return (*it)->AsWeakPtr();

  return base::WeakPtr<HardwareDisplayController>();
}

ScreenManager::HardwareDisplayControllers::iterator
ScreenManager::FindDisplayController(uint32_t crtc) {
  for (HardwareDisplayControllers::iterator it = controllers_.begin();
       it != controllers_.end();
       ++it) {
    if ((*it)->HasCrtc(crtc))
      return it;
  }

  return controllers_.end();
}

ScreenManager::HardwareDisplayControllers::iterator
ScreenManager::FindActiveDisplayControllerByLocation(const gfx::Rect& bounds) {
  for (HardwareDisplayControllers::iterator it = controllers_.begin();
       it != controllers_.end();
       ++it) {
    gfx::Rect controller_bounds((*it)->origin(), (*it)->GetModeSize());
    // We don't perform a strict check since content_shell will have windows
    // smaller than the display size.
    if (controller_bounds.Contains(bounds) && !(*it)->IsDisabled())
      return it;
  }

  return controllers_.end();
}

void ScreenManager::ForceInitializationOfPrimaryDisplay() {
  LOG(WARNING) << "Forcing initialization of primary display.";
  ScopedVector<HardwareDisplayControllerInfo> displays =
      GetAvailableDisplayControllerInfos(dri_->get_fd());

  DCHECK_NE(0u, displays.size());

  ScopedDrmPropertyPtr dpms(
      dri_->GetProperty(displays[0]->connector(), "DPMS"));
  if (dpms)
    dri_->SetProperty(displays[0]->connector()->connector_id,
                      dpms->prop_id,
                      DRM_MODE_DPMS_ON);

  AddDisplayController(displays[0]->crtc()->crtc_id,
                       displays[0]->connector()->connector_id);
  ConfigureDisplayController(displays[0]->crtc()->crtc_id,
                             displays[0]->connector()->connector_id,
                             gfx::Point(),
                             displays[0]->connector()->modes[0]);
}

bool ScreenManager::ModesetDisplayController(
    HardwareDisplayController* controller,
    const gfx::Point& origin,
    const drmModeModeInfo& mode) {
  controller->set_origin(origin);
  // Create a surface suitable for the current controller.
  scoped_refptr<ScanoutBuffer> buffer =
      buffer_generator_->Create(gfx::Size(mode.hdisplay, mode.vdisplay));

  if (!buffer.get()) {
    LOG(ERROR) << "Failed to create scanout buffer";
    return false;
  }

  if (!controller->Modeset(OverlayPlane(buffer), mode)) {
    LOG(ERROR) << "Failed to modeset controller";
    return false;
  }

  return true;
}

bool ScreenManager::HandleMirrorMode(
    HardwareDisplayControllers::iterator original,
    HardwareDisplayControllers::iterator mirror,
    uint32_t crtc,
    uint32_t connector) {
  (*mirror)->AddCrtc((*original)->RemoveCrtc(crtc));
  if ((*mirror)->Enable()) {
    controllers_.erase(original);
    return true;
  }

  LOG(ERROR) << "Failed to switch to mirror mode";

  // When things go wrong revert back to the previous configuration since
  // it is expected that the configuration would not have changed if
  // things fail.
  (*original)->AddCrtc((*mirror)->RemoveCrtc(crtc));
  (*original)->Enable();
  return false;
}

}  // namespace ui
