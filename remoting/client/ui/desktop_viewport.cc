// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/ui/desktop_viewport.h"

#include <algorithm>

#include "base/logging.h"

namespace remoting {

namespace {

float MAX_ZOOM_LEVEL = 100.f;

}  // namespace

DesktopViewport::DesktopViewport() : desktop_to_surface_transform_() {}

DesktopViewport::~DesktopViewport() {}

void DesktopViewport::SetDesktopSize(int desktop_width, int desktop_height) {
  if (desktop_width == desktop_size_.x && desktop_height == desktop_size_.y) {
    return;
  }

  desktop_size_.x = desktop_width;
  desktop_size_.y = desktop_height;
  ResizeToFit();
}

void DesktopViewport::SetSurfaceSize(int surface_width, int surface_height) {
  if (surface_width == surface_size_.x && surface_height == surface_size_.y) {
    return;
  }

  // Only reset the viewport if both dimensions have changed, otherwise keep
  // the offset and scale and just change the constraint. This is to cover these
  // use cases:
  // * Rotation => Reset
  // * Keyboard => No reset
  // * Settings menu => No reset
  //
  // TODO(yuweih): This is probably too much inferring. Let the caller to decide
  // when to call ResizeToFit() if things don't work right.
  bool need_to_reset =
      surface_width != surface_size_.x && surface_height != surface_size_.y;

  surface_size_.x = surface_width;
  surface_size_.y = surface_height;

  if (need_to_reset) {
    ResizeToFit();
    return;
  }

  UpdateViewport();
}

void DesktopViewport::MoveDesktop(float dx, float dy) {
  desktop_to_surface_transform_.PostTranslate({dx, dy});
  UpdateViewport();
}

void DesktopViewport::ScaleDesktop(float px, float py, float scale) {
  desktop_to_surface_transform_.PostScale({px, py}, scale);
  UpdateViewport();
}

void DesktopViewport::MoveViewport(float dx, float dy) {
  MoveViewportWithoutUpdate(dx, dy);
  UpdateViewport();
}

void DesktopViewport::SetViewportCenter(float x, float y) {
  ViewMatrix::Point old_center = GetViewportCenter();
  MoveViewport(x - old_center.x, y - old_center.y);
}

ViewMatrix::Point DesktopViewport::GetViewportCenter() const {
  if (!IsViewportReady()) {
    LOG(WARNING) << "Viewport is not ready before getting the viewport center";
    return {0.f, 0.f};
  }
  return desktop_to_surface_transform_.Invert().MapPoint(
      {surface_size_.x / 2.f, surface_size_.y / 2.f});
}

bool DesktopViewport::IsPointWithinDesktopBounds(
    const ViewMatrix::Point& point) const {
  if (!IsViewportReady()) {
    LOG(WARNING) << "Viewport is not ready";
    return false;
  }
  return point.x >= 0 && point.y >= 0 && point.x < desktop_size_.x &&
         point.y < desktop_size_.y;
}

ViewMatrix::Point DesktopViewport::ConstrainPointToDesktop(
    const ViewMatrix::Point& point) const {
  if (!IsViewportReady()) {
    LOG(WARNING) << "Cannot constrain point to desktop. Viewport is not ready.";
    return point;
  }

  return ConstrainPointToBounds({0.f, desktop_size_.x, 0.f, desktop_size_.y},
                                point);
}

void DesktopViewport::RegisterOnTransformationChangedCallback(
    const TransformationCallback& callback,
    bool run_immediately) {
  on_transformation_changed_ = callback;
  if (IsViewportReady() && run_immediately) {
    callback.Run(desktop_to_surface_transform_);
  }
}

const ViewMatrix& DesktopViewport::GetTransformation() const {
  return desktop_to_surface_transform_;
}

void DesktopViewport::ResizeToFit() {
  if (!IsViewportReady()) {
    return;
  }

  // <---Desktop Width---->
  // +==========+---------+
  // |          |         |
  // | Viewport | Desktop |
  // |          |         |
  // +==========+---------+
  //
  // +==========+ ^
  // |          | |
  // | Viewport | |
  // |          | |
  // +==========+ | Desktop
  // |          | | Height
  // | Desktop  | |
  // |          | |
  // +----------+ v
  // resize the desktop such that it fits the viewport in one dimension.

  float scale = std::max(surface_size_.x / desktop_size_.x,
                         surface_size_.y / desktop_size_.y);
  desktop_to_surface_transform_.SetScale(scale);
  desktop_to_surface_transform_.SetOffset({0.f, 0.f});
  UpdateViewport();
}

bool DesktopViewport::IsViewportReady() const {
  return desktop_size_.x != 0 && desktop_size_.y != 0 && surface_size_.x != 0 &&
         surface_size_.y != 0;
}

void DesktopViewport::UpdateViewport() {
  if (!IsViewportReady()) {
    // User may attempt to zoom and pan before the desktop image is received.
    // This should be fine since the viewport will be reset once the image
    // dimension is set.
    VLOG(1) << "Viewport is not ready yet.";
    return;
  }

  // Adjust zoom level.
  float zoom_level = desktop_to_surface_transform_.GetScale();
  if (zoom_level > MAX_ZOOM_LEVEL) {
    // TODO(yuweih): This doesn't account for the effect of the pivot point,
    //     which will shift the desktop closer to the origin.
    desktop_to_surface_transform_.SetScale(MAX_ZOOM_LEVEL);
  }

  ViewMatrix::Vector2D desktop_size_on_surface_ =
      desktop_to_surface_transform_.MapVector(desktop_size_);
  if (desktop_size_on_surface_.x < surface_size_.x &&
      desktop_size_on_surface_.y < surface_size_.y) {
    //    +==============+
    //    |      VP      |      +==========+
    //    |              |      |    VP    |
    //    | +----------+ |      +----------+
    //    | |    DP    | |  ==> |    DP    |
    //    | +----------+ |      +----------+
    //    |              |      |          |
    //    |              |      +==========+
    //    +==============+
    // Displayed desktop is too small in both directions, so apply the minimum
    // zoom level needed to fit either the width or height.
    float scale = std::min(surface_size_.x / desktop_size_.x,
                           surface_size_.y / desktop_size_.y);
    desktop_to_surface_transform_.SetScale(scale);
  }

  // Adjust position.
  // Scenarios:
  // 1. If the viewport can fully fit inside the desktop (smaller or equal width
  //    and height) but it overlaps with the border, it will be moved in minimum
  //    distance to be fitted inside the desktop.
  //
  //    +========+
  //    | VP     |
  //    |   +----|--------+     +========+-----+
  //    |   |    |        |     |        |     |
  //    +========+        |     |   VP   | DP  |
  //        |       DP    | ==> |        |     |
  //        |             |     +========+     |
  //        |             |     |              |
  //        +-------------+     +--------------+
  //
  // 2. If the viewport is larger than the desktop, the viewport will always
  //    share the same center as the desktop.
  //
  //    +==========+------+==+     +======+------+======+
  //    | VP       |  DP  |  | ==> | VP   |  DP  |      |
  //    +==========+------+==+     +======+------+======+
  //
  // This is done on the desktop's reference frame to simplify things a bit.
  ViewMatrix::Point old_center = GetViewportCenter();
  ViewMatrix::Point new_center =
      ConstrainPointToBounds(GetViewportCenterBounds(), old_center);
  MoveViewportWithoutUpdate(new_center.x - old_center.x,
                            new_center.y - old_center.y);

  if (on_transformation_changed_) {
    on_transformation_changed_.Run(desktop_to_surface_transform_);
  }
}

DesktopViewport::Bounds DesktopViewport::GetViewportCenterBounds() const {
  Bounds bounds;

  // Viewport size on the desktop space.
  ViewMatrix::Vector2D viewport_size =
      desktop_to_surface_transform_.Invert().MapVector(surface_size_);

  // Scenario 1: If VP can fully fit inside the desktop, then VP's center can be
  // anywhere inside the desktop as long as VP doesn't overlap with the border.
  bounds.left = viewport_size.x / 2.f;
  bounds.right = desktop_size_.x - viewport_size.x / 2.f;
  bounds.top = viewport_size.y / 2.f;
  bounds.bottom = desktop_size_.y - viewport_size.y / 2.f;

  // Scenario 2: If VP can't fully fit inside the desktop in dimension D, then
  // its bounds in dimension D is tightly restricted to the center of the
  // desktop.
  if (bounds.left > bounds.right) {
    float desktop_width_center = desktop_size_.x / 2.f;
    bounds.left = desktop_width_center;
    bounds.right = desktop_width_center;
  }

  if (bounds.top > bounds.bottom) {
    float desktop_height_center = desktop_size_.y / 2.f;
    bounds.top = desktop_height_center;
    bounds.bottom = desktop_height_center;
  }

  return bounds;
}

void DesktopViewport::MoveViewportWithoutUpdate(float dx, float dy) {
  // <dx, dy> is defined on desktop's reference frame. Translation must be
  // flipped and scaled.
  desktop_to_surface_transform_.PostTranslate(
      desktop_to_surface_transform_.MapVector({-dx, -dy}));
}

// static
ViewMatrix::Point DesktopViewport::ConstrainPointToBounds(
    const Bounds& bounds,
    const ViewMatrix::Point& point) {
  ViewMatrix::Point new_point = point;
  if (new_point.x < bounds.left) {
    new_point.x = bounds.left;
  } else if (new_point.x > bounds.right) {
    new_point.x = bounds.right;
  }

  if (new_point.y < bounds.top) {
    new_point.y = bounds.top;
  } else if (new_point.y > bounds.bottom) {
    new_point.y = bounds.bottom;
  }
  return new_point;
}

}  // namespace remoting
