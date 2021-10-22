// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_display_observer.h"

#include <wayland-server-core.h>
#include <xdg-output-unstable-v1-server-protocol.h>

#include <string>

#include "components/exo/wayland/wayland_display_output.h"
#include "components/exo/wm_helper.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"

namespace exo {
namespace wayland {

WaylandDisplayHandler::WaylandDisplayHandler(WaylandDisplayOutput* output,
                                             wl_resource* output_resource)
    : output_(output), output_resource_(output_resource) {
}

WaylandDisplayHandler::~WaylandDisplayHandler() {
  output_->UnregisterOutput(output_resource_);
}

void WaylandDisplayHandler::Initialize() {
  // Adding itself as an observer will send the initial display metrics.
  AddObserver(this);
  output_->RegisterOutput(output_resource_);
}

void WaylandDisplayHandler::AddObserver(WaylandDisplayObserver* observer) {
  observers_.AddObserver(observer);

  display::Display display;
  bool exists = display::Screen::GetScreen()->GetDisplayWithDisplayId(
      output_->id(), &display);
  if (!exists) {
    // WaylandDisplayHandler is created asynchronously, and the
    // display can be deleted before created. This usually won't happen
    // in real environment, but can happen in test environment.
    return;
  }

  // Send the first round of changes to the observer.
  constexpr uint32_t all_changes = 0xFFFFFFFF;
  if (observer->SendDisplayMetrics(display, all_changes)) {
    if (wl_resource_get_version(output_resource_) >=
        WL_OUTPUT_DONE_SINCE_VERSION) {
      wl_output_send_done(output_resource_);
    }
    wl_client_flush(wl_resource_get_client(output_resource_));
  }
}

int64_t WaylandDisplayHandler::id() const {
  DCHECK(output_);
  return output_->id();
}

void WaylandDisplayHandler::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  DCHECK(output_resource_);

  if (output_->id() != display.id())
    return;

  bool needs_done = false;
  for (auto& observer : observers_)
    needs_done |= observer.SendDisplayMetrics(display, changed_metrics);

  if (needs_done) {
    if (wl_resource_get_version(output_resource_) >=
        WL_OUTPUT_DONE_SINCE_VERSION) {
      wl_output_send_done(output_resource_);
    }
    wl_client_flush(wl_resource_get_client(output_resource_));
  }
}

void WaylandDisplayHandler::OnXdgOutputCreated(
    wl_resource* xdg_output_resource) {
  DCHECK(!xdg_output_resource_);
  xdg_output_resource_ = xdg_output_resource;

  display::Display display;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(output_->id(),
                                                             &display)) {
    return;
  }
  OnDisplayMetricsChanged(display, 0xFFFFFFFF);
}

void WaylandDisplayHandler::UnsetXdgOutputResource() {
  DCHECK(xdg_output_resource_);
  xdg_output_resource_ = nullptr;
}

bool WaylandDisplayHandler::SendDisplayMetrics(const display::Display& display,
                                               uint32_t changed_metrics) {
  if (!output_resource_)
    return false;

  // There is no need to check DISPLAY_METRIC_PRIMARY because when primary
  // changes, bounds always changes. (new primary should have had non
  // 0,0 origin).
  // Only exception is when switching to newly connected primary with
  // the same bounds. This happens whenyou're in docked mode, suspend,
  // unplug the display, then resume to the internal display which has
  // the same resolution. Since metrics does not change, there is no need
  // to notify clients.
  if (!(changed_metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
         DISPLAY_METRIC_ROTATION))) {
    return false;
  }

  const display::ManagedDisplayInfo& info =
      WMHelper::GetInstance()->GetDisplayInfo(display.id());

  const float kInchInMm = 25.4f;
  const char* kUnknown = "unknown";

  const std::string& make = info.manufacturer_id();
  const std::string& model = info.product_id();

  // TODO(oshima): The current Wayland protocol currently has no way of
  // informing a client about any overscan the display has, and what the safe
  // area of the display might be. We may want to make a change to the
  // aura-shell (zaura_output) protocol, or to upstream a change to the
  // xdg-output (currently unstable) protocol to add that information.

  // |origin| is used in wayland service to identify the workspace
  // the pixel size will be applied.
  const gfx::Point origin = display.bounds().origin();

  // |physical_size_px| is the physical resolution of the display in pixels.
  // The value should not include any overscan insets or display rotation,
  // except for any panel orientation adjustment.
  const gfx::Size physical_size_px = info.bounds_in_native().size();

  // |physical_size_mm| is our best-effort approximation for the physical size
  // of the display in millimeters, given the display resolution and DPI. The
  // value should not include any overscan insets or display rotation, except
  // for any panel orientation adjustment.
  const gfx::Size physical_size_mm =
      ScaleToRoundedSize(physical_size_px, kInchInMm / info.device_dpi());

  // Use panel_rotation otherwise some X apps will refuse to take events from
  // outside the "visible" region.
  wl_output_send_geometry(output_resource_, origin.x(), origin.y(),
                          physical_size_mm.width(), physical_size_mm.height(),
                          WL_OUTPUT_SUBPIXEL_UNKNOWN,
                          make.empty() ? kUnknown : make.c_str(),
                          model.empty() ? kUnknown : model.c_str(),
                          OutputTransform(display.panel_rotation()));

  // TODO(reveman): Send real list of modes.
  wl_output_send_mode(output_resource_,
                      WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
                      physical_size_px.width(), physical_size_px.height(),
                      static_cast<int>(60000));

  if (xdg_output_resource_) {
    const gfx::Size logical_size = ScaleToRoundedSize(
        physical_size_px, 1.0f / display.device_scale_factor());
    zxdg_output_v1_send_logical_size(xdg_output_resource_, logical_size.width(),
                                     logical_size.height());
  } else {
    if (wl_resource_get_version(output_resource_) >=
        WL_OUTPUT_SCALE_SINCE_VERSION) {
      // wl_output only supports integer scaling, so if device scale factor is
      // fractional we need to round it up to the closest integer.
      wl_output_send_scale(output_resource_,
                           std::ceil(display.device_scale_factor()));
    }
  }

  return true;
}

wl_output_transform WaylandDisplayHandler::OutputTransform(
    display::Display::Rotation rotation) {
  // Note: |rotation| describes the counter clockwise rotation that a
  // display's output is currently adjusted for, which is the inverse
  // of what we need to return.
  switch (rotation) {
    case display::Display::ROTATE_0:
      return WL_OUTPUT_TRANSFORM_NORMAL;
    case display::Display::ROTATE_90:
      return WL_OUTPUT_TRANSFORM_270;
    case display::Display::ROTATE_180:
      return WL_OUTPUT_TRANSFORM_180;
    case display::Display::ROTATE_270:
      return WL_OUTPUT_TRANSFORM_90;
  }
  NOTREACHED();
  return WL_OUTPUT_TRANSFORM_NORMAL;
}

}  // namespace wayland
}  // namespace exo
