// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OBSERVER_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OBSERVER_H_

#include <stdint.h>
#include <wayland-server-protocol-core.h>

#include "ui/display/display.h"
#include "ui/display/display_observer.h"

struct wl_resource;

namespace exo {
namespace wayland {

class WaylandDisplayObserver : public display::DisplayObserver {
 public:
  class ScaleObserver : public base::SupportsWeakPtr<ScaleObserver> {
   public:
    ScaleObserver() {}

    virtual void OnDisplayScalesChanged(const display::Display& display) = 0;

   protected:
    virtual ~ScaleObserver() {}
  };

  WaylandDisplayObserver(int64_t id, wl_resource* output_resource);
  ~WaylandDisplayObserver() override;
  void SetScaleObserver(base::WeakPtr<ScaleObserver> scale_observer);
  bool HasScaleObserver() const;

  // Overridden from display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  void SendDisplayMetrics();
  // Returns the transform that a compositor will apply to a surface to
  // compensate for the rotation of an output device.
  wl_output_transform OutputTransform(display::Display::Rotation rotation);

  // The ID of the display being observed.
  const int64_t id_;

  // The output resource associated with the display.
  wl_resource* const output_resource_;

  base::WeakPtr<ScaleObserver> scale_observer_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDisplayObserver);
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OBSERVER_H_
