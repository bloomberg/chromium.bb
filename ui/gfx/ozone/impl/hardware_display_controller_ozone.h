// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OZONE_IMPL_HARDWARE_DISPLAY_CONTROLLER_OZONE_H_
#define UI_GFX_OZONE_IMPL_HARDWARE_DISPLAY_CONTROLLER_OZONE_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/ozone/impl/drm_wrapper_ozone.h"

namespace gfx {

class SoftwareSurfaceOzone;

// The HDCOz will handle modesettings and scannout operations for hardware
// devices.
//
// In the DRM world there are 3 components that need to be paired up to be able
// to display an image to the monitor: CRTC (cathode ray tube controller),
// encoder and connector. The CRTC determines which framebuffer to read, when
// to scanout and where to scanout. Encoders converts the stream from the CRTC
// to the appropriate format for the connector. The connector is the physical
// connection that monitors connect to.
//
// There is no 1:1:1 pairing for these components. It is possible for an encoder
// to be compatible to multiple CRTCs and each connector can be used with
// multiple encoders. In addition, it is possible to use one CRTC with multiple
// connectors such that we can display the same image on multiple monitors.
//
// For example, the following configuration shows 2 different screens being
// initialized separately.
// -------------      -------------
// | Connector |      | Connector |
// |   HDMI    |      |    VGA    |
// -------------      -------------
//       ^                  ^
//       |                  |
// -------------      -------------
// |  Encoder1  |     |  Encoder2 |
// -------------      -------------
//       ^                  ^
//       |                  |
// -------------      -------------
// |   CRTC1   |      |   CRTC2   |
// -------------      -------------
//
// In the following configuration 2 different screens are associated with the
// same CRTC, so on scanout the same framebuffer will be displayed on both
// monitors.
// -------------      -------------
// | Connector |      | Connector |
// |   HDMI    |      |    VGA    |
// -------------      -------------
//       ^                  ^
//       |                  |
// -------------      -------------
// |  Encoder1  |     |  Encoder2 |
// -------------      -------------
//       ^                  ^
//       |                  |
//      ----------------------
//      |        CRTC1       |
//      ----------------------
//
// Note that it is possible to have more connectors than CRTCs which means that
// only a subset of connectors can be active independently, showing different
// framebuffers. Though, in this case, it would be possible to have all
// connectors active if some use the same CRTC to mirror the display.
//
// TODO(dnicoara) Need to have a way to detect events (such as monitor
// connected or disconnected).
class HardwareDisplayControllerOzone {
 public:
  // Controller states. The state transitions will happen from top to bottom.
  enum State {
    // When we allocate a HDCO as a stub. At this point there is no connector
    // and CRTC associated with this device.
    UNASSOCIATED,

    // When |SetControllerInfo| is called and the HDCO has the information of
    // the hardware it will control. At this point it knows everything it needs
    // to control the hardware but doesn't have a surface.
    UNINITIALIZED,

    // A surface is associated with the HDCO. This means that the controller can
    // potentially display the backing surface to the display. Though the
    // surface framebuffer still needs to be registered with the CRTC.
    SURFACE_INITIALIZED,

    // The CRTC now knows about the surface attributes.
    INITIALIZED,

    // Error state if any of the initialization steps fail.
    FAILED,
  };

  HardwareDisplayControllerOzone();

  ~HardwareDisplayControllerOzone();

  // Set the hardware configuration for this HDCO. Once this is set, the HDCO is
  // responsible for keeping track of the connector and CRTC and cleaning up
  // when it is destroyed.
  void SetControllerInfo(DrmWrapperOzone* drm,
                         uint32_t connector_id,
                         uint32_t crtc_id,
                         drmModeModeInfo mode);

  // Associate the HDCO with a surface implementation and initialize it.
  bool BindSurfaceToController(SoftwareSurfaceOzone* surface);

  // Schedules the |surface_|'s framebuffer to be displayed on the next vsync
  // event. The event will be posted on the graphics card file descriptor |fd_|
  // and it can be read and processed by |drmHandleEvent|. That function can
  // define the callback for the page flip event. A generic data argument will
  // be presented to the callback. We use that argument to pass in the HDCO
  // object the event belongs to.
  //
  // Between this call and the callback, the framebuffer used in this call
  // should not be modified in any way as it would cause screen tearing if the
  // hardware performed the flip. Note that the frontbuffer should also not
  // be modified as it could still be displayed.
  //
  // Note that this function does not block. Also, this function should not be
  // called again before the page flip occurrs.
  //
  // Returns true if the page flip was successfully registered, false otherwise.
  bool SchedulePageFlip();

  State get_state() const { return state_; };

  int get_fd() const { return drm_->get_fd(); };

  const drmModeModeInfo& get_mode() const { return mode_; };

  SoftwareSurfaceOzone* get_surface() const { return surface_.get(); };

 private:
  // Object containing the connection to the graphics device and wraps the API
  // calls to control it.
  DrmWrapperOzone* drm_;

  // TODO(dnicoara) Need to allow a CRTC to have multiple connectors.
  uint32_t connector_id_;

  uint32_t crtc_id_;

  // TODO(dnicoara) Need to store all the modes.
  drmModeModeInfo mode_;

  // Saved CRTC state from before we used it. Need it to restore state once we
  // are finished using this device.
  drmModeCrtc* saved_crtc_;

  State state_;

  scoped_ptr<SoftwareSurfaceOzone> surface_;

  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayControllerOzone);
};

}  // namespace gfx

#endif  // UI_GFX_OZONE_IMPL_HARDWARE_DISPLAY_CONTROLLER_OZONE_H_
