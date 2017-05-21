// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CHROMEOS_TOUCH_TRANSFORM_CONTROLLER_H_
#define UI_DISPLAY_MANAGER_CHROMEOS_TOUCH_TRANSFORM_CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/transform.h"

namespace ui {
struct TouchscreenDevice;
}

namespace display {

class DisplayConfigurator;
class DisplayManager;
class ManagedDisplayInfo;
class TouchTransformSetter;

struct TouchDeviceTransform;

namespace test {
class TouchTransformControllerTest;
}

// TouchTransformController matches touchscreen displays with touch
// input-devices and computes the coordinate transforms between display space
// and input-device space.
class DISPLAY_MANAGER_EXPORT TouchTransformController {
 public:
  TouchTransformController(DisplayConfigurator* display_configurator,
                           DisplayManager* display_manager,
                           std::unique_ptr<TouchTransformSetter> setter);
  ~TouchTransformController();

  // Updates the transform for touch input-devices and pushes the new transforms
  // into device manager.
  void UpdateTouchTransforms() const;

  // During touch calibration we remove the previous transform and update touch
  // transformer until calibration is complete.
  void SetForCalibration(bool is_calibrating);

 private:
  friend class test::TouchTransformControllerTest;

  // Contains the data that is passed to TouchTransformSetter.
  struct UpdateData {
    UpdateData();
    ~UpdateData();

    std::map<int32_t, double> device_to_scale;
    std::vector<TouchDeviceTransform> touch_device_transforms;
  };

  void UpdateTouchTransforms(UpdateData* data) const;

  // Returns a transform that will be used to change an event's location from
  // the touchscreen's coordinate system into |display|'s coordinate system.
  // The transform is also responsible for properly scaling the display if the
  // display supports panel fitting.
  //
  // On X11 events are reported in framebuffer coordinate space, so the
  // |framebuffer_size| is used for scaling.
  // On Ozone events are reported in the touchscreen's resolution, so
  // |touch_display| is used to determine the size and scale the event.
  gfx::Transform GetTouchTransform(const ManagedDisplayInfo& display,
                                   const ManagedDisplayInfo& touch_display,
                                   const ui::TouchscreenDevice& touchscreen,
                                   const gfx::Size& framebuffer_size) const;

  // Returns the scaling factor for the touch radius such that it scales the
  // radius from |touch_device|'s coordinate system to the |touch_display|'s
  // coordinate system.
  double GetTouchResolutionScale(
      const ManagedDisplayInfo& touch_display,
      const ui::TouchscreenDevice& touch_device) const;

  // For the provided |display| update the touch radius mapping in
  // |update_data|.
  void UpdateTouchRadius(const ManagedDisplayInfo& display,
                         UpdateData* update_data) const;

  // For a given |target_display| and |target_display_id| update the touch
  // transformation in |update_data| based on the touchscreen associated with
  // |touch_display|. |target_display_id| is the display id to update the
  // transform for.
  // |touch_display| is the physical display that has the touchscreen
  // from which the events arrive.
  // |target_display| provides the dimensions to which the touch event will be
  // transformed.
  void UpdateTouchTransform(int64_t target_display_id,
                            const ManagedDisplayInfo& touch_display,
                            const ManagedDisplayInfo& target_display,
                            UpdateData* update_data) const;

  // Both |display_configurator_| and |display_manager_| are not owned and must
  // outlive TouchTransformController.
  DisplayConfigurator* display_configurator_;
  DisplayManager* display_manager_;

  bool is_calibrating_ = false;

  std::unique_ptr<TouchTransformSetter> setter_;

  DISALLOW_COPY_AND_ASSIGN(TouchTransformController);
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CHROMEOS_TOUCH_TRANSFORM_CONTROLLER_H_
