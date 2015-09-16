// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/cast/overlay_manager_cast.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "chromecast/media/base/media_message_loop.h"
#include "chromecast/public/cast_media_shlib.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/video_plane.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {
namespace {

// Helper class for calling VideoPlane::SetGeometry with rate-limiting.
// SetGeometry can take on the order of 100ms to run in some implementations
// and can be called on the order of 20x / second (as fast as graphics frames
// are produced).  This creates an ever-growing backlog of tasks on the media
// thread.
// This class measures the time taken to run SetGeometry to determine a
// reasonable frequency at which to call it.  Excess calls are coalesced
// to just set the most recent geometry.
class RateLimitedSetVideoPlaneGeometry
    : public base::RefCountedThreadSafe<RateLimitedSetVideoPlaneGeometry> {
 public:
  RateLimitedSetVideoPlaneGeometry(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : pending_display_rect_(0, 0, 0, 0),
        pending_set_geometry_(false),
        min_calling_interval_ms_(0),
        sample_counter_(0),
        task_runner_(task_runner) {}

  void SetGeometry(
      const chromecast::RectF& display_rect,
      chromecast::media::VideoPlane::CoordinateType coordinate_type,
      chromecast::media::VideoPlane::Transform transform) {
    DCHECK(task_runner_->BelongsToCurrentThread());

    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta elapsed = now - last_set_geometry_time_;

    if (elapsed < base::TimeDelta::FromMilliseconds(min_calling_interval_ms_)) {
      if (!pending_set_geometry_) {
        pending_set_geometry_ = true;

        task_runner_->PostDelayedTask(
            FROM_HERE,
            base::Bind(
                &RateLimitedSetVideoPlaneGeometry::ApplyPendingSetGeometry,
                this),
            base::TimeDelta::FromMilliseconds(2 * min_calling_interval_ms_));
      }

      pending_display_rect_ = display_rect;
      pending_coordinate_type_ = coordinate_type;
      pending_transform_ = transform;
      return;
    }
    last_set_geometry_time_ = now;

    chromecast::media::VideoPlane* video_plane =
        chromecast::media::CastMediaShlib::GetVideoPlane();
    CHECK(video_plane);
    base::TimeTicks start = base::TimeTicks::Now();
    video_plane->SetGeometry(display_rect, coordinate_type, transform);

    base::TimeDelta set_geometry_time = base::TimeTicks::Now() - start;
    UpdateAverageTime(set_geometry_time.InMilliseconds());
  }

 private:
  friend class base::RefCountedThreadSafe<RateLimitedSetVideoPlaneGeometry>;
  ~RateLimitedSetVideoPlaneGeometry() {}

  void UpdateAverageTime(int64 sample) {
    const size_t kSampleCount = 5;
    if (samples_.size() < kSampleCount)
      samples_.push_back(sample);
    else
      samples_[sample_counter_++ % kSampleCount] = sample;
    int64 total = 0;
    for (int64 s : samples_)
      total += s;
    min_calling_interval_ms_ = 2 * total / samples_.size();
  }

  void ApplyPendingSetGeometry() {
    if (pending_set_geometry_) {
      pending_set_geometry_ = false;
      SetGeometry(pending_display_rect_, pending_coordinate_type_,
                  pending_transform_);
    }
  }

  chromecast::RectF pending_display_rect_;
  chromecast::media::VideoPlane::CoordinateType pending_coordinate_type_;
  chromecast::media::VideoPlane::Transform pending_transform_;
  bool pending_set_geometry_;
  base::TimeTicks last_set_geometry_time_;

  // Don't call SetGeometry faster than this interval.
  int64 min_calling_interval_ms_;

  // Min calling interval is computed as double average of last few time samples
  // (i.e. allow at least as much time between calls as the call itself takes).
  std::vector<int64> samples_;
  size_t sample_counter_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(RateLimitedSetVideoPlaneGeometry);
};

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
      : media_task_runner_(
            chromecast::media::MediaMessageLoop::GetTaskRunner()),
        transform_(gfx::OVERLAY_TRANSFORM_INVALID),
        display_rect_(0, 0, 0, 0),
        video_plane_wrapper_(
            new RateLimitedSetVideoPlaneGeometry(media_task_runner_)) {}

  void CheckOverlaySupport(OverlaySurfaceCandidateList* surfaces) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  gfx::OverlayTransform transform_;
  chromecast::RectF display_rect_;
  scoped_refptr<RateLimitedSetVideoPlaneGeometry> video_plane_wrapper_;
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
    // This must be done on media thread - and no point doing if it hasn't
    // changed.
    if (candidate.transform != transform_ ||
        !ExactlyEqual(display_rect, display_rect_)) {
      transform_ = candidate.transform;
      display_rect_ = display_rect;

      media_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &RateLimitedSetVideoPlaneGeometry::SetGeometry,
              video_plane_wrapper_, display_rect,
              chromecast::media::VideoPlane::COORDINATE_TYPE_GRAPHICS_PLANE,
              ConvertTransform(candidate.transform)));
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
