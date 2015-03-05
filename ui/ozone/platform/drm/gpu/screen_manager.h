// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_SCREEN_MANAGER_H_
#define UI_OZONE_PLATFORM_DRM_GPU_SCREEN_MANAGER_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/platform/drm/gpu/display_change_observer.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

typedef struct _drmModeModeInfo drmModeModeInfo;

namespace gfx {
class Point;
class Rect;
class Size;
}  // namespace gfx

namespace ui {

class DrmDevice;
class ScanoutBufferGenerator;

// Responsible for keeping track of active displays and configuring them.
class OZONE_EXPORT ScreenManager {
 public:
  ScreenManager(ScanoutBufferGenerator* surface_generator);
  virtual ~ScreenManager();

  // Register a display controller. This must be called before trying to
  // configure it.
  void AddDisplayController(const scoped_refptr<DrmDevice>& drm,
                            uint32_t crtc,
                            uint32_t connector);

  // Remove a display controller from the list of active controllers. The
  // controller is removed since it was disconnected.
  void RemoveDisplayController(const scoped_refptr<DrmDevice>& drm,
                               uint32_t crtc);

  // Configure a display controller. The display controller is identified by
  // (|crtc|, |connector|) and the controller is modeset using |mode|.
  bool ConfigureDisplayController(const scoped_refptr<DrmDevice>& drm,
                                  uint32_t crtc,
                                  uint32_t connector,
                                  const gfx::Point& origin,
                                  const drmModeModeInfo& mode);

  // Disable the display controller identified by |crtc|. Note, the controller
  // may still be connected, so this does not remove the controller.
  bool DisableDisplayController(const scoped_refptr<DrmDevice>& drm,
                                uint32_t crtc);

  // Returns a reference to the display controller configured to display within
  // |bounds|. If the caller caches the controller it must also register as an
  // observer to be notified when the controller goes out of scope.
  HardwareDisplayController* GetDisplayController(const gfx::Rect& bounds);

  void AddObserver(DisplayChangeObserver* observer);
  void RemoveObserver(DisplayChangeObserver* observer);

 private:
  typedef ScopedVector<HardwareDisplayController> HardwareDisplayControllers;

  // Returns an iterator into |controllers_| for the controller identified by
  // (|crtc|, |connector|).
  HardwareDisplayControllers::iterator FindDisplayController(
      const scoped_refptr<DrmDevice>& drm,
      uint32_t crtc);

  // Returns an iterator into |controllers_| for the controller located at
  // |origin|.
  HardwareDisplayControllers::iterator FindActiveDisplayControllerByLocation(
      const gfx::Rect& bounds);

  // Perform modesetting in |controller| using |origin| and |mode|.
  bool ModesetDisplayController(HardwareDisplayController* controller,
                                const gfx::Point& origin,
                                const drmModeModeInfo& mode);

  // Tries to set the controller identified by (|crtc|, |connector|) to mirror
  // those in |mirror|. |original| is an iterator to the HDC where the
  // controller is currently present.
  bool HandleMirrorMode(HardwareDisplayControllers::iterator original,
                        HardwareDisplayControllers::iterator mirror,
                        const scoped_refptr<DrmDevice>& drm,
                        uint32_t crtc,
                        uint32_t connector);

  ScanoutBufferGenerator* buffer_generator_;  // Not owned.
  // List of display controllers (active and disabled).
  HardwareDisplayControllers controllers_;

  ObserverList<DisplayChangeObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ScreenManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_SCREEN_MANAGER_H_
