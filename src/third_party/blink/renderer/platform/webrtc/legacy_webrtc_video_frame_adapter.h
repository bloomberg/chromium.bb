// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBRTC_LEGACY_WEBRTC_VIDEO_FRAME_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBRTC_LEGACY_WEBRTC_VIDEO_FRAME_ADAPTER_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_pool.h"
#include "media/capture/video/video_capture_feedback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace blink {

// Thin adapter from media::VideoFrame to webrtc::VideoFrameBuffer. This
// implementation is read-only and will return null if trying to get a
// non-const pointer to the pixel data. This object will be accessed from
// different threads, but that's safe since it's read-only.
//
// 3 types of frames can be adapted, either,
// - The frame is mappable in either I420, I420A or NV12 pixel formats.
// - Or, the frame is on a GPU Memory Buffer in NV12.
// - Or, the frame has textures.
// Creators of LegacyWebRtcVideoFrameAdapter can ensure the frame is convertible
// by calling |IsFrameAdaptable| before constructing this object.
class PLATFORM_EXPORT LegacyWebRtcVideoFrameAdapter
    : public WebRtcVideoFrameAdapterInterface {
 public:
  class PLATFORM_EXPORT SharedResources
      : public base::RefCountedThreadSafe<SharedResources> {
   public:
    explicit SharedResources(
        media::GpuVideoAcceleratorFactories* gpu_factories);

    // Create frames for requested output format and resolution.
    virtual scoped_refptr<media::VideoFrame> CreateFrame(
        media::VideoPixelFormat format,
        const gfx::Size& coded_size,
        const gfx::Rect& visible_rect,
        const gfx::Size& natural_size,
        base::TimeDelta timestamp);

    // Temporary frames may have a different format or size than scaled frames.
    // However, VideoFramePool doesn't work nicely if the requested frame size
    // or format changes on the fly. Therefore a separate pool is used for
    // temporary frames.
    virtual scoped_refptr<media::VideoFrame> CreateTemporaryFrame(
        media::VideoPixelFormat format,
        const gfx::Size& coded_size,
        const gfx::Rect& visible_rect,
        const gfx::Size& natural_size,
        base::TimeDelta timestamp);

    virtual scoped_refptr<viz::RasterContextProvider>
    GetRasterContextProvider();

    // Constructs a VideoFrame from a texture by invoking RasterInterface,
    // which would perform a blocking call to a GPU process.
    // The pixel data is copied and may be in ARGB pixel format in some cases,
    // So additional conversion to I420 would be needed.
    virtual scoped_refptr<media::VideoFrame> ConstructVideoFrameFromTexture(
        scoped_refptr<media::VideoFrame> source_frame);

    // Constructs a VideoFrame from a GMB. Unless it's a Windows DXGI GMB,
    // the buffer is mapped to the memory and wrapped by a VideoFrame with no
    // copies. For DXGI buffers a blocking call to GPU process is made to
    // copy pixel data.
    virtual scoped_refptr<media::VideoFrame> ConstructVideoFrameFromGpu(
        scoped_refptr<media::VideoFrame> source_frame);

    // Used to report feedback from an adapter upon destruction.
    void SetFeedback(const media::VideoCaptureFeedback& feedback);

    // Returns the most recently stored feedback.
    media::VideoCaptureFeedback GetFeedback();

   protected:
    friend class base::RefCountedThreadSafe<SharedResources>;
    virtual ~SharedResources();

   private:
    media::VideoFramePool pool_;
    media::VideoFramePool pool_for_mapped_frames_;
    media::VideoFramePool pool_for_tmp_frames_;

    base::Lock context_provider_lock_;
    scoped_refptr<viz::RasterContextProvider> raster_context_provider_
        GUARDED_BY(context_provider_lock_);

    media::GpuVideoAcceleratorFactories* gpu_factories_;

    base::Lock feedback_lock_;

    // Contains feedback from the most recently destroyed Adapter.
    media::VideoCaptureFeedback last_feedback_ GUARDED_BY(feedback_lock_);
  };

  // Returns true if |frame| is adaptable to a webrtc::VideoFrameBuffer.
  static bool IsFrameAdaptable(const media::VideoFrame* frame);
  static base::span<const media::VideoPixelFormat>
  AdaptableMappablePixelFormats();

  explicit LegacyWebRtcVideoFrameAdapter(
      scoped_refptr<media::VideoFrame> frame);
  LegacyWebRtcVideoFrameAdapter(
      scoped_refptr<media::VideoFrame> frame,
      scoped_refptr<SharedResources> shared_resources);

  bool SupportsOptimizedScaling() const override { return false; }
  scoped_refptr<media::VideoFrame> getMediaVideoFrame() const override {
    return frame_;
  }

 private:
  Type type() const override;
  int width() const override;
  int height() const override;

  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override;
  const webrtc::I420BufferInterface* GetI420() const override;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> GetMappedFrameBuffer(
      rtc::ArrayView<webrtc::VideoFrameBuffer::Type> types) override;
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> CropAndScale(
      int offset_x,
      int offset_y,
      int crop_width,
      int crop_height,
      int scaled_width,
      int scaled_height) override;

 protected:
  ~LegacyWebRtcVideoFrameAdapter() override;

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> CreateFrameAdapter() const;

  mutable base::Lock adapter_lock_;

  // Used to cache result of CreateFrameAdapter. Which is called from const
  // GetI420().
  mutable rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter_
      GUARDED_BY(adapter_lock_);

  scoped_refptr<media::VideoFrame> frame_;

  mutable scoped_refptr<SharedResources> shared_resources_;

  media::GpuVideoAcceleratorFactories* gpu_factories_;

  mutable bool was_mapped_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBRTC_LEGACY_WEBRTC_VIDEO_FRAME_ADAPTER_H_
