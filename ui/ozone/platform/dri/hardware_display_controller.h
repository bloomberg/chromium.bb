// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_HARDWARE_DISPLAY_CONTROLLER_H_
#define UI_OZONE_PLATFORM_DRI_HARDWARE_DISPLAY_CONTROLLER_H_

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace gfx {
class Point;
}

namespace ui {

class ScanoutBuffer;

struct OverlayPlane {
  // Simpler constructor for the primary plane.
  explicit OverlayPlane(scoped_refptr<ScanoutBuffer> buffer);

  OverlayPlane(scoped_refptr<ScanoutBuffer> buffer,
               int z_order,
               gfx::OverlayTransform plane_transform,
               const gfx::Rect& display_bounds,
               const gfx::RectF& crop_rect);

  ~OverlayPlane();

  scoped_refptr<ScanoutBuffer> buffer;
  int z_order;
  gfx::OverlayTransform plane_transform;
  gfx::Rect display_bounds;
  gfx::RectF crop_rect;
  int overlay_plane;
};

typedef std::vector<OverlayPlane> OverlayPlaneList;

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
class HardwareDisplayController
    : public base::SupportsWeakPtr<HardwareDisplayController> {
 public:
  HardwareDisplayController(DriWrapper* drm,
                            uint32_t connector_id,
                            uint32_t crtc_id);

  ~HardwareDisplayController();

  // Performs the initial CRTC configuration. If successful, it will display the
  // framebuffer for |primary| with |mode|.
  bool Modeset(const OverlayPlane& primary,
               drmModeModeInfo mode);

  // Reconfigures the CRTC with the current surface and mode.
  bool Enable();

  // Disables the CRTC.
  void Disable();

  // Schedules the |overlays|' framebuffers to be displayed on the next vsync
  // event. The event will be posted on the graphics card file descriptor |fd_|
  // and it can be read and processed by |drmHandleEvent|. That function can
  // define the callback for the page flip event. A generic data argument will
  // be presented to the callback. We use that argument to pass in the HDCO
  // object the event belongs to.
  //
  // Between this call and the callback, the framebuffers used in this call
  // should not be modified in any way as it would cause screen tearing if the
  // hardware performed the flip. Note that the frontbuffer should also not
  // be modified as it could still be displayed.
  //
  // Note that this function does not block. Also, this function should not be
  // called again before the page flip occurrs.
  //
  // Returns true if the page flip was successfully registered, false otherwise.
  bool SchedulePageFlip(const OverlayPlaneList& overlays);

  // TODO(dnicoara) This should be on the MessageLoop when Ozone can have
  // BeginFrame can be triggered explicitly by Ozone.
  void WaitForPageFlipEvent();

  // Called when the page flip event occurred. The event is provided by the
  // kernel when a VBlank event finished. This allows the controller to
  // update internal state and propagate the update to the surface.
  // The tuple (seconds, useconds) represents the event timestamp. |seconds|
  // represents the number of seconds while |useconds| represents the
  // microseconds (< 1 second) in the timestamp.
  void OnPageFlipEvent(unsigned int frame,
                       unsigned int seconds,
                       unsigned int useconds);

  // Set the hardware cursor to show the contents of |surface|.
  bool SetCursor(scoped_refptr<ScanoutBuffer> buffer);

  bool UnsetCursor();

  // Moves the hardware cursor to |location|.
  bool MoveCursor(const gfx::Point& location);

  const drmModeModeInfo& get_mode() const { return mode_; };
  uint32_t connector_id() const { return connector_id_; }
  uint32_t crtc_id() const { return crtc_id_; }

  uint64_t get_time_of_last_flip() const {
    return time_of_last_flip_;
  };

 private:
  OverlayPlaneList current_planes_;
  OverlayPlaneList pending_planes_;

  // Object containing the connection to the graphics device and wraps the API
  // calls to control it.
  DriWrapper* drm_;

  // TODO(dnicoara) Need to allow a CRTC to have multiple connectors.
  uint32_t connector_id_;

  uint32_t crtc_id_;

  drmModeModeInfo mode_;

  scoped_refptr<ScanoutBuffer> cursor_buffer_;

  uint64_t time_of_last_flip_;

  // Keeps track of the CRTC state. If a surface has been bound, then the value
  // is set to false. Otherwise it is true.
  bool is_disabled_;

  // Store the state of the CRTC before we took over. Used to restore the CRTC
  // once we no longer need it.
  ScopedDrmCrtcPtr saved_crtc_;

  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayController);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_HARDWARE_DISPLAY_CONTROLLER_H_
