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

namespace {

gfx::Size GetModeSize(const drmModeModeInfo& mode) {
  return gfx::Size(mode.hdisplay, mode.vdisplay);
}

}  // namespace

ScreenManager::ScreenManager(
    DriWrapper* dri, ScanoutBufferGenerator* buffer_generator)
    : dri_(dri), buffer_generator_(buffer_generator), last_added_widget_(0) {
}

ScreenManager::~ScreenManager() {
  STLDeleteContainerPairSecondPointers(
      controllers_.begin(), controllers_.end());
}

void ScreenManager::RemoveDisplayController(uint32_t crtc) {
  HardwareDisplayControllerMap::iterator it = FindDisplayController(crtc);
  if (it != controllers_.end()) {
    it->second->RemoveCrtc(crtc);
    if (!it->second->HasCrtcs()) {
      delete it->second;
      controllers_.erase(it);
    }
  }
}

bool ScreenManager::ConfigureDisplayController(uint32_t crtc,
                                               uint32_t connector,
                                               const gfx::Point& origin,
                                               const drmModeModeInfo& mode) {
  HardwareDisplayControllerMap::iterator it =
      FindDisplayController(crtc);
  HardwareDisplayController* controller = NULL;
  if (it != controllers_.end()) {
    // If nothing changed just enable the controller.
    if (SameMode(mode, it->second->get_mode()) &&
        origin == it->second->origin())
      return it->second->Enable();

    // Either the mode or the location of the display changed, so exit mirror
    // mode and configure the display independently. If the caller still wants
    // mirror mode, subsequent calls configuring the other controllers will
    // restore mirror mode.
    it->second->RemoveMirroredCrtcs();
    HardwareDisplayControllerMap::iterator mirror =
        FindDisplayControllerByOrigin(origin);
    // Handle mirror mode.
    if (mirror != controllers_.end() && it != mirror) {
      DCHECK(SameMode(mode, mirror->second->get_mode()));
      return HandleMirrorMode(it, mirror, crtc, connector);
    }

    controller = it->second;
  } else {
    HardwareDisplayControllerMap::iterator mirror =
        FindDisplayControllerByOrigin(origin);
    if (mirror != controllers_.end()) {
      mirror->second->AddCrtc(scoped_ptr<CrtcState>(
          new CrtcState(dri_, crtc, connector)));
      return mirror->second->Enable();
    }
  }

  if (!controller) {
    controller = new HardwareDisplayController(
        dri_,
        scoped_ptr<CrtcState>(new CrtcState(dri_, crtc, connector)));
    controllers_.insert(std::make_pair(++last_added_widget_, controller));
  }

  return ModesetDisplayController(controller, origin, mode);
}

bool ScreenManager::DisableDisplayController(uint32_t crtc) {
  HardwareDisplayControllerMap::iterator it = FindDisplayController(crtc);
  if (it != controllers_.end()) {
    it->second->Disable();
    return true;
  }

  LOG(ERROR) << "Failed to find display controller"
             << " crtc=" << crtc;
  return false;
}

base::WeakPtr<HardwareDisplayController> ScreenManager::GetDisplayController(
    gfx::AcceleratedWidget widget) {
  // TODO(dnicoara): Remove hack once TestScreen uses a simple Ozone display
  // configuration reader and ScreenManager is called from there to create the
  // one display needed by the content_shell target.
  if (controllers_.empty() && last_added_widget_ == 0)
    ForceInitializationOfPrimaryDisplay();

  HardwareDisplayControllerMap::iterator it = controllers_.find(widget);
  if (it != controllers_.end())
    return it->second->AsWeakPtr();

  return base::WeakPtr<HardwareDisplayController>();
}

base::WeakPtr<HardwareDisplayController> ScreenManager::GetDisplayController(
    const gfx::Rect& bounds) {
  // TODO(dnicoara): Remove hack once TestScreen uses a simple Ozone display
  // configuration reader and ScreenManager is called from there to create the
  // one display needed by the content_shell target.
  if (controllers_.empty())
    ForceInitializationOfPrimaryDisplay();

  HardwareDisplayControllerMap::iterator it =
      FindActiveDisplayControllerByLocation(bounds);
  if (it != controllers_.end())
    return it->second->AsWeakPtr();

  return base::WeakPtr<HardwareDisplayController>();
}

ScreenManager::HardwareDisplayControllerMap::iterator
ScreenManager::FindDisplayController(uint32_t crtc) {
  for (HardwareDisplayControllerMap::iterator it = controllers_.begin();
       it != controllers_.end();
       ++it) {
    if (it->second->HasCrtc(crtc))
      return it;
  }

  return controllers_.end();
}

ScreenManager::HardwareDisplayControllerMap::iterator
ScreenManager::FindDisplayControllerByOrigin(const gfx::Point& origin) {
  for (HardwareDisplayControllerMap::iterator it = controllers_.begin();
       it != controllers_.end();
       ++it) {
    if (it->second->origin() == origin)
      return it;
  }

  return controllers_.end();
}

ScreenManager::HardwareDisplayControllerMap::iterator
ScreenManager::FindActiveDisplayControllerByLocation(const gfx::Rect& bounds) {
  for (HardwareDisplayControllerMap::iterator it = controllers_.begin();
       it != controllers_.end();
       ++it) {
    gfx::Rect controller_bounds(it->second->origin(),
                                GetModeSize(it->second->get_mode()));
    // We don't perform a strict check since content_shell will have windows
    // smaller than the display size.
    if (controller_bounds.Contains(bounds))
      return it;
  }

  return controllers_.end();
}

void ScreenManager::ForceInitializationOfPrimaryDisplay() {
  ScopedVector<HardwareDisplayControllerInfo> displays =
      GetAvailableDisplayControllerInfos(dri_->get_fd());

  DCHECK_NE(0u, displays.size());

  ScopedDrmPropertyPtr dpms(
      dri_->GetProperty(displays[0]->connector(), "DPMS"));
  if (dpms)
    dri_->SetProperty(displays[0]->connector()->connector_id,
                      dpms->prop_id,
                      DRM_MODE_DPMS_ON);

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

  if (!buffer) {
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
    HardwareDisplayControllerMap::iterator original,
    HardwareDisplayControllerMap::iterator mirror,
    uint32_t crtc,
    uint32_t connector) {
  mirror->second->AddCrtc(original->second->RemoveCrtc(crtc));
  if (mirror->second->Enable()) {
    delete original->second;
    controllers_.erase(original);
    return true;
  }

  LOG(ERROR) << "Failed to switch to mirror mode";

  // When things go wrong revert back to the previous configuration since
  // it is expected that the configuration would not have changed if
  // things fail.
  original->second->AddCrtc(mirror->second->RemoveCrtc(crtc));
  original->second->Enable();
  return false;
}

}  // namespace ui
