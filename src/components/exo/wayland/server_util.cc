// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server_util.h"

#include "base/time/time.h"
#include "components/exo/data_offer.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace exo {
namespace wayland {

namespace {

// A property key containing the surface resource that is associated with
// window. If unset, no surface resource is associated with surface object.
DEFINE_UI_CLASS_PROPERTY_KEY(wl_resource*, kSurfaceResourceKey, nullptr);

// A property key containing the data offer resource that is associated with
// data offer object.
DEFINE_UI_CLASS_PROPERTY_KEY(wl_resource*, kDataOfferResourceKey, nullptr);

}  // namespace

uint32_t TimeTicksToMilliseconds(base::TimeTicks ticks) {
  return (ticks - base::TimeTicks()).InMilliseconds();
}

uint32_t NowInMilliseconds() {
  return TimeTicksToMilliseconds(base::TimeTicks::Now());
}

wl_resource* GetSurfaceResource(Surface* surface) {
  return surface->GetProperty(kSurfaceResourceKey);
}

void SetSurfaceResource(Surface* surface, wl_resource* resource) {
  surface->SetProperty(kSurfaceResourceKey, resource);
}

wl_resource* GetDataOfferResource(const DataOffer* data_offer) {
  return data_offer->GetProperty(kDataOfferResourceKey);
}

void SetDataOfferResource(DataOffer* data_offer,
                          wl_resource* data_offer_resource) {
  data_offer->SetProperty(kDataOfferResourceKey, data_offer_resource);
}

// Scale the |child_bounds| in such a way that if it should fill the
// |parent_size|'s width/height, it returns the |parent_size_in_pixel|'s
// width/height.
gfx::Rect ScaleBoundsToPixelSnappedToParent(
    const gfx::Size& parent_size_in_pixel,
    const gfx::Size& parent_size,
    float device_scale_factor,
    const gfx::Rect& child_bounds) {
  int right = child_bounds.right();
  int bottom = child_bounds.bottom();

  int new_x = std::round(child_bounds.x() * device_scale_factor);
  int new_y = std::round(child_bounds.y() * device_scale_factor);

  int new_right = right == parent_size.width()
                      ? parent_size_in_pixel.width()
                      : std::round(right * device_scale_factor);

  int new_bottom = bottom == parent_size.height()
                       ? parent_size_in_pixel.height()
                       : std::round(bottom * device_scale_factor);

  return gfx::Rect(new_x, new_y, new_right - new_x, new_bottom - new_y);
}

// Create the insets make sure that work area will be within the chrome's
// work area when converted to the pixel on client side.
// TODO(oshima): We should send these information in pixel so that
// client do not have to convert it back.
gfx::Insets GetAdjustedInsets(const display::Display& display) {
  float scale = display.device_scale_factor();
  gfx::Size size_in_pixel = display.GetSizeInPixel();
  gfx::Rect work_area_in_display = display.work_area();
  work_area_in_display.Offset(-display.bounds().x(), -display.bounds().y());
  gfx::Rect work_area_in_pixel = ScaleBoundsToPixelSnappedToParent(
      size_in_pixel, display.bounds().size(), scale, work_area_in_display);
  gfx::Insets insets_in_pixel =
      gfx::Rect(size_in_pixel).InsetsFrom(work_area_in_pixel);
  return gfx::Insets(std::ceil(insets_in_pixel.top() / scale),
                     std::ceil(insets_in_pixel.left() / scale),
                     std::ceil(insets_in_pixel.bottom() / scale),
                     std::ceil(insets_in_pixel.right() / scale));
}

}  // namespace wayland
}  // namespace exo
