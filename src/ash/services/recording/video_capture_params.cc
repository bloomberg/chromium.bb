// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/recording/video_capture_params.h"

#include "ash/services/recording/recording_service_constants.h"
#include "base/check.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "media/base/video_types.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_video_capture.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace recording {

namespace {

// -----------------------------------------------------------------------------
// FullscreenCaptureParams:

class FullscreenCaptureParams : public VideoCaptureParams {
 public:
  FullscreenCaptureParams(viz::FrameSinkId frame_sink_id,
                          const gfx::Size& frame_sink_size)
      : VideoCaptureParams(frame_sink_id,
                           viz::SubtreeCaptureId(),
                           frame_sink_size) {}
  FullscreenCaptureParams(const FullscreenCaptureParams&) = delete;
  FullscreenCaptureParams& operator=(const FullscreenCaptureParams&) = delete;
  ~FullscreenCaptureParams() override = default;

  // VideoCaptureParams:
  gfx::Size GetVideoSize() const override { return current_frame_sink_size_; }

  bool OnFrameSinkSizeChanged(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      const gfx::Size& new_frame_sink_size) override {
    // We override the default behavior, as we want the video size to remain at
    // the original requested size. This gives a nice indication of a display
    // rotation or DSF change. The new video frames will letterbox to adhere to
    // the original requested resolution constraints.
    return false;
  }
};

// -----------------------------------------------------------------------------
// WindowCaptureParams:

class WindowCaptureParams : public VideoCaptureParams {
 public:
  WindowCaptureParams(viz::FrameSinkId frame_sink_id,
                      viz::SubtreeCaptureId subtree_capture_id,
                      const gfx::Size& window_size,
                      const gfx::Size& frame_sink_size)
      : VideoCaptureParams(frame_sink_id, subtree_capture_id, frame_sink_size),
        current_window_size_(window_size) {}
  WindowCaptureParams(const WindowCaptureParams&) = delete;
  WindowCaptureParams& operator=(const WindowCaptureParams&) = delete;
  ~WindowCaptureParams() override = default;

  // VideoCaptureParams:
  gfx::Rect GetVideoFrameVisibleRect(
      const gfx::Rect& original_frame_visible_rect) const override {
    return gfx::Rect(current_window_size_);
  }

  gfx::Size GetVideoSize() const override { return current_window_size_; }

  bool OnRecordedWindowChangingRoot(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      viz::FrameSinkId new_frame_sink_id,
      const gfx::Size& new_frame_sink_size) override {
    DCHECK(new_frame_sink_id.is_valid());
    DCHECK_NE(frame_sink_id_, new_frame_sink_id);

    // The video encoder deals with video frames. Changing the frame sink ID
    // doesn't affect the encoder. What affects it is a change in the video
    // frames size.
    const bool should_reconfigure_video_encoder =
        current_frame_sink_size_ != new_frame_sink_size;

    current_frame_sink_size_ = new_frame_sink_size;
    frame_sink_id_ = new_frame_sink_id;
    capturer->SetResolutionConstraints(/*min_size=*/current_frame_sink_size_,
                                       /*max_size=*/current_frame_sink_size_,
                                       /*use_fixed_aspect_ratio=*/true);
    capturer->ChangeTarget(frame_sink_id_, subtree_capture_id_);

    return should_reconfigure_video_encoder;
  }

  bool OnRecordedWindowSizeChanged(
      mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
      const gfx::Size& new_window_size) override {
    if (current_window_size_ == new_window_size)
      return false;
    current_window_size_ = new_window_size;
    return true;
  }

 private:
  gfx::Size current_window_size_;
};

// -----------------------------------------------------------------------------
// RegionCaptureParams:

class RegionCaptureParams : public VideoCaptureParams {
 public:
  RegionCaptureParams(viz::FrameSinkId frame_sink_id,
                      const gfx::Size& frame_sink_size,
                      const gfx::Rect& crop_region)
      : VideoCaptureParams(frame_sink_id,
                           viz::SubtreeCaptureId(),
                           frame_sink_size),
        crop_region_(crop_region) {}
  RegionCaptureParams(const RegionCaptureParams&) = delete;
  RegionCaptureParams& operator=(const RegionCaptureParams&) = delete;
  ~RegionCaptureParams() override = default;

  // VideoCaptureParams:
  gfx::Rect GetVideoFrameVisibleRect(
      const gfx::Rect& original_frame_visible_rect) const override {
    // We can't crop the video frame by an invalid bounds. The crop bounds must
    // be contained within the original frame bounds.
    gfx::Rect visible_rect = original_frame_visible_rect;
    visible_rect.Intersect(crop_region_);
    return visible_rect;
  }

  gfx::Size GetVideoSize() const override {
    return GetVideoFrameVisibleRect(gfx::Rect(current_frame_sink_size_)).size();
  }

 private:
  const gfx::Rect crop_region_;
};

}  // namespace

// -----------------------------------------------------------------------------
// VideoCaptureParams:

// static
std::unique_ptr<VideoCaptureParams>
VideoCaptureParams::CreateForFullscreenCapture(
    viz::FrameSinkId frame_sink_id,
    const gfx::Size& frame_sink_size) {
  return std::make_unique<FullscreenCaptureParams>(frame_sink_id,
                                                   frame_sink_size);
}

// static
std::unique_ptr<VideoCaptureParams> VideoCaptureParams::CreateForWindowCapture(
    viz::FrameSinkId frame_sink_id,
    viz::SubtreeCaptureId subtree_capture_id,
    const gfx::Size& window_size,
    const gfx::Size& frame_sink_size) {
  return std::make_unique<WindowCaptureParams>(
      frame_sink_id, subtree_capture_id, window_size, frame_sink_size);
}

// static
std::unique_ptr<VideoCaptureParams> VideoCaptureParams::CreateForRegionCapture(
    viz::FrameSinkId frame_sink_id,
    const gfx::Size& full_capture_size,
    const gfx::Rect& crop_region) {
  return std::make_unique<RegionCaptureParams>(frame_sink_id, full_capture_size,
                                               crop_region);
}

void VideoCaptureParams::InitializeVideoCapturer(
    mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer) const {
  DCHECK(capturer);

  capturer->SetMinCapturePeriod(kMinCapturePeriod);
  capturer->SetMinSizeChangePeriod(kMinPeriodForResizeThrottling);
  capturer->SetResolutionConstraints(/*min_size=*/current_frame_sink_size_,
                                     /*max_size=*/current_frame_sink_size_,
                                     /*use_fixed_aspect_ratio=*/true);
  capturer->SetAutoThrottlingEnabled(false);
  // TODO(afakhry): Discuss with //media/ team the implications of color space
  // conversions.
  capturer->SetFormat(media::PIXEL_FORMAT_I420, kColorSpace);
  capturer->ChangeTarget(frame_sink_id_, subtree_capture_id_);
}

gfx::Rect VideoCaptureParams::GetVideoFrameVisibleRect(
    const gfx::Rect& original_frame_visible_rect) const {
  return original_frame_visible_rect;
}

bool VideoCaptureParams::OnRecordedWindowChangingRoot(
    mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
    viz::FrameSinkId new_frame_sink_id,
    const gfx::Size& new_frame_sink_size) {
  CHECK(false) << "This can only be called when recording a window";
  return false;
}

bool VideoCaptureParams::OnRecordedWindowSizeChanged(
    mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
    const gfx::Size& new_window_size) {
  CHECK(false) << "This can only be called when recording a window";
  return false;
}

bool VideoCaptureParams::OnFrameSinkSizeChanged(
    mojo::Remote<viz::mojom::FrameSinkVideoCapturer>& capturer,
    const gfx::Size& new_frame_sink_size) {
  if (new_frame_sink_size == current_frame_sink_size_)
    return false;

  current_frame_sink_size_ = new_frame_sink_size;
  capturer->SetResolutionConstraints(/*min_size=*/current_frame_sink_size_,
                                     /*max_size=*/current_frame_sink_size_,
                                     /*use_fixed_aspect_ratio=*/true);
  return true;
}

VideoCaptureParams::VideoCaptureParams(viz::FrameSinkId frame_sink_id,
                                       viz::SubtreeCaptureId subtree_capture_id,
                                       const gfx::Size& current_frame_sink_size)
    : frame_sink_id_(frame_sink_id),
      subtree_capture_id_(subtree_capture_id),
      current_frame_sink_size_(current_frame_sink_size) {
  DCHECK(frame_sink_id_.is_valid());
}

}  // namespace recording
