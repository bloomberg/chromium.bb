// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/overlay_manager_cast.h"

#include "chromecast/media/base/video_plane_controller.h"
#include "chromecast/public/graphics_types.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {
namespace {

// Translates a gfx::OverlayTransform into a VideoPlane::Transform.
// Could be just a lookup table once we have unit tests for this code
// to ensure it stays in sync with OverlayTransform.
chromecast::media::VideoPlane::Transform ConvertTransform(
    gfx::OverlayTransform transform) {
  switch (transform) {
    case gfx::OVERLAY_TRANSFORM_NONE:
      return chromecast::media::VideoPlane::TRANSFORM_NONE;
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      return chromecast::media::VideoPlane::FLIP_HORIZONTAL;
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return chromecast::media::VideoPlane::FLIP_VERTICAL;
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
      return chromecast::media::VideoPlane::ROTATE_90;
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      return chromecast::media::VideoPlane::ROTATE_180;
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      return chromecast::media::VideoPlane::ROTATE_270;
    default:
      NOTREACHED();
      return chromecast::media::VideoPlane::TRANSFORM_NONE;
  }
}

bool ExactlyEqual(const chromecast::RectF& r1, const chromecast::RectF& r2) {
  return r1.x == r2.x && r1.y == r2.y && r1.width == r2.width &&
         r1.height == r2.height;
}

class OverlayCandidatesCast : public OverlayCandidatesOzone {
 public:
  OverlayCandidatesCast()
      : transform_(gfx::OVERLAY_TRANSFORM_INVALID), display_rect_(0, 0, 0, 0) {}

  void CheckOverlaySupport(OverlaySurfaceCandidateList* surfaces) override;

 private:
  gfx::OverlayTransform transform_;
  chromecast::RectF display_rect_;
};

void OverlayCandidatesCast::CheckOverlaySupport(
    OverlaySurfaceCandidateList* surfaces) {
  for (auto& candidate : *surfaces) {
    if (candidate.plane_z_order != -1)
      continue;

    candidate.overlay_handled = true;

    // Compositor requires all overlay rectangles to have integer coords.
    candidate.display_rect =
        gfx::RectF(gfx::ToEnclosedRect(candidate.display_rect));

    chromecast::RectF display_rect(
        candidate.display_rect.x(), candidate.display_rect.y(),
        candidate.display_rect.width(), candidate.display_rect.height());

    // Update video plane geometry + transform to match compositor quad.
    if (candidate.transform != transform_ ||
        !ExactlyEqual(display_rect, display_rect_)) {
      transform_ = candidate.transform;
      display_rect_ = display_rect;

      chromecast::media::VideoPlaneController* video_plane =
          chromecast::media::VideoPlaneController::GetInstance();
      video_plane->SetGeometry(display_rect,
                               ConvertTransform(candidate.transform));
    }
    return;
  }
}

}  // namespace

OverlayManagerCast::OverlayManagerCast() {
}

OverlayManagerCast::~OverlayManagerCast() {
}

scoped_ptr<OverlayCandidatesOzone> OverlayManagerCast::CreateOverlayCandidates(
    gfx::AcceleratedWidget w) {
  return make_scoped_ptr(new OverlayCandidatesCast());
}

}  // namespace ui
