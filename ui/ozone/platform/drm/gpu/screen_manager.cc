// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/screen_manager.h"

#include <xf86drmMode.h>

#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_console_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

namespace ui {

namespace {

// Copies the contents of the saved framebuffer from the CRTCs in |controller|
// to the new modeset buffer |buffer|.
void FillModesetBuffer(const scoped_refptr<DrmDevice>& drm,
                       HardwareDisplayController* controller,
                       ScanoutBuffer* buffer) {
  DrmConsoleBuffer modeset_buffer(drm, buffer->GetFramebufferId());
  if (!modeset_buffer.Initialize()) {
    LOG(ERROR) << "Failed to grab framebuffer " << buffer->GetFramebufferId();
    return;
  }

  auto crtcs = controller->crtc_controllers();
  DCHECK(!crtcs.empty());

  if (!crtcs[0]->saved_crtc() || !crtcs[0]->saved_crtc()->buffer_id)
    return;

  // If the display controller is in mirror mode, the CRTCs should be sharing
  // the same framebuffer.
  DrmConsoleBuffer saved_buffer(drm, crtcs[0]->saved_crtc()->buffer_id);
  if (!saved_buffer.Initialize()) {
    LOG(ERROR) << "Failed to grab saved framebuffer "
               << crtcs[0]->saved_crtc()->buffer_id;
    return;
  }

  // Don't copy anything if the sizes mismatch. This can happen when the user
  // changes modes.
  if (saved_buffer.canvas()->getBaseLayerSize() !=
      modeset_buffer.canvas()->getBaseLayerSize())
    return;

  skia::RefPtr<SkImage> image = saved_buffer.image();
  SkPaint paint;
  // Copy the source buffer. Do not perform any blending.
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  modeset_buffer.canvas()->drawImage(image.get(), 0, 0, &paint);
}

}  // namespace

ScreenManager::ScreenManager(ScanoutBufferGenerator* buffer_generator)
    : buffer_generator_(buffer_generator) {
}

ScreenManager::~ScreenManager() {
}

void ScreenManager::AddDisplayController(const scoped_refptr<DrmDevice>& drm,
                                         uint32_t crtc,
                                         uint32_t connector) {
  HardwareDisplayControllers::iterator it = FindDisplayController(drm, crtc);
  // TODO(dnicoara): Turn this into a DCHECK when async display configuration is
  // properly supported. (When there can't be a race between forcing initial
  // display configuration in ScreenManager and NativeDisplayDelegate creating
  // the display controllers.)
  if (it != controllers_.end()) {
    LOG(WARNING) << "Display controller (crtc=" << crtc << ") already present.";
    return;
  }

  controllers_.push_back(new HardwareDisplayController(
      scoped_ptr<CrtcController>(new CrtcController(drm, crtc, connector))));
}

void ScreenManager::RemoveDisplayController(const scoped_refptr<DrmDevice>& drm,
                                            uint32_t crtc) {
  HardwareDisplayControllers::iterator it = FindDisplayController(drm, crtc);
  if (it != controllers_.end()) {
    bool is_mirrored = (*it)->IsMirrored();
    (*it)->RemoveCrtc(drm, crtc);
    if (!is_mirrored) {
      FOR_EACH_OBSERVER(DisplayChangeObserver, observers_,
                        OnDisplayRemoved(*it));
      controllers_.erase(it);
    }
  }
}

bool ScreenManager::ConfigureDisplayController(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t crtc,
    uint32_t connector,
    const gfx::Point& origin,
    const drmModeModeInfo& mode) {
  gfx::Rect modeset_bounds(origin.x(), origin.y(), mode.hdisplay,
                           mode.vdisplay);
  HardwareDisplayControllers::iterator it = FindDisplayController(drm, crtc);
  DCHECK(controllers_.end() != it) << "Display controller (crtc=" << crtc
                                   << ") doesn't exist.";

  HardwareDisplayController* controller = *it;
  // If nothing changed just enable the controller. Note, we perform an exact
  // comparison on the mode since the refresh rate may have changed.
  if (SameMode(mode, controller->get_mode()) &&
      origin == controller->origin()) {
    if (controller->IsDisabled()) {
      HardwareDisplayControllers::iterator mirror =
          FindActiveDisplayControllerByLocation(modeset_bounds);
      // If there is an active controller at the same location then start mirror
      // mode.
      if (mirror != controllers_.end())
        return HandleMirrorMode(it, mirror, drm, crtc, connector);
    }

    // Just re-enable the controller to re-use the current state.
    bool enabled = controller->Enable();
    FOR_EACH_OBSERVER(DisplayChangeObserver, observers_,
                      OnDisplayChanged(controller));

    return enabled;
  }

  // Either the mode or the location of the display changed, so exit mirror
  // mode and configure the display independently. If the caller still wants
  // mirror mode, subsequent calls configuring the other controllers will
  // restore mirror mode.
  if (controller->IsMirrored()) {
    controller =
        new HardwareDisplayController(controller->RemoveCrtc(drm, crtc));
    controllers_.push_back(controller);
    it = controllers_.end() - 1;
  }

  HardwareDisplayControllers::iterator mirror =
      FindActiveDisplayControllerByLocation(modeset_bounds);
  // Handle mirror mode.
  if (mirror != controllers_.end() && it != mirror)
    return HandleMirrorMode(it, mirror, drm, crtc, connector);

  return ModesetDisplayController(controller, origin, mode);
}

bool ScreenManager::DisableDisplayController(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t crtc) {
  HardwareDisplayControllers::iterator it = FindDisplayController(drm, crtc);
  if (it != controllers_.end()) {
    if ((*it)->IsMirrored()) {
      HardwareDisplayController* controller =
          new HardwareDisplayController((*it)->RemoveCrtc(drm, crtc));
      controllers_.push_back(controller);
    }

    (*it)->Disable();
    return true;
  }

  LOG(ERROR) << "Failed to find display controller crtc=" << crtc;
  return false;
}

HardwareDisplayController* ScreenManager::GetDisplayController(
    const gfx::Rect& bounds) {
  HardwareDisplayControllers::iterator it =
      FindActiveDisplayControllerByLocation(bounds);
  if (it != controllers_.end())
    return *it;

  return nullptr;
}

void ScreenManager::AddObserver(DisplayChangeObserver* observer) {
  observers_.AddObserver(observer);
}

void ScreenManager::RemoveObserver(DisplayChangeObserver* observer) {
  observers_.RemoveObserver(observer);
}

ScreenManager::HardwareDisplayControllers::iterator
ScreenManager::FindDisplayController(const scoped_refptr<DrmDevice>& drm,
                                     uint32_t crtc) {
  for (HardwareDisplayControllers::iterator it = controllers_.begin();
       it != controllers_.end(); ++it) {
    if ((*it)->HasCrtc(drm, crtc))
      return it;
  }

  return controllers_.end();
}

ScreenManager::HardwareDisplayControllers::iterator
ScreenManager::FindActiveDisplayControllerByLocation(const gfx::Rect& bounds) {
  for (HardwareDisplayControllers::iterator it = controllers_.begin();
       it != controllers_.end(); ++it) {
    gfx::Rect controller_bounds((*it)->origin(), (*it)->GetModeSize());
    if (controller_bounds == bounds && !(*it)->IsDisabled())
      return it;
  }

  return controllers_.end();
}

bool ScreenManager::ModesetDisplayController(
    HardwareDisplayController* controller,
    const gfx::Point& origin,
    const drmModeModeInfo& mode) {
  DCHECK(!controller->crtc_controllers().empty());
  scoped_refptr<DrmDevice> drm = controller->GetAllocationDrmDevice();
  controller->set_origin(origin);

  // Create a surface suitable for the current controller.
  scoped_refptr<ScanoutBuffer> buffer =
      buffer_generator_->Create(drm, gfx::Size(mode.hdisplay, mode.vdisplay));

  if (!buffer.get()) {
    LOG(ERROR) << "Failed to create scanout buffer";
    return false;
  }

  FillModesetBuffer(drm, controller, buffer.get());

  if (!controller->Modeset(OverlayPlane(buffer), mode)) {
    LOG(ERROR) << "Failed to modeset controller";
    return false;
  }

  FOR_EACH_OBSERVER(DisplayChangeObserver, observers_,
                    OnDisplayChanged(controller));
  return true;
}

bool ScreenManager::HandleMirrorMode(
    HardwareDisplayControllers::iterator original,
    HardwareDisplayControllers::iterator mirror,
    const scoped_refptr<DrmDevice>& drm,
    uint32_t crtc,
    uint32_t connector) {
  (*mirror)->AddCrtc((*original)->RemoveCrtc(drm, crtc));
  if ((*mirror)->Enable()) {
    FOR_EACH_OBSERVER(DisplayChangeObserver, observers_,
                      OnDisplayRemoved(*original));
    FOR_EACH_OBSERVER(DisplayChangeObserver, observers_,
                      OnDisplayChanged(*mirror));
    controllers_.erase(original);
    return true;
  }

  LOG(ERROR) << "Failed to switch to mirror mode";

  // When things go wrong revert back to the previous configuration since
  // it is expected that the configuration would not have changed if
  // things fail.
  (*original)->AddCrtc((*mirror)->RemoveCrtc(drm, crtc));
  (*original)->Enable();
  return false;
}

}  // namespace ui
