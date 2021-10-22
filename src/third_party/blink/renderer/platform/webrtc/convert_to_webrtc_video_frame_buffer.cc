// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/convert_to_webrtc_video_frame_buffer.h"

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "media/base/video_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/webrtc/api/video/i420_buffer.h"
#include "third_party/webrtc/common_video/include/video_frame_buffer.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace WTF {

// Template specializations of [1], needed to be able to pass WTF callbacks
// that have VideoTrackAdapterSettings or gfx::Size parameters across threads.
//
// [1] third_party/blink/renderer/platform/wtf/cross_thread_copier.h.
template <>
struct CrossThreadCopier<scoped_refptr<webrtc::VideoFrameBuffer>>
    : public CrossThreadCopierPassThrough<
          scoped_refptr<webrtc::VideoFrameBuffer>> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace {

class I420FrameAdapter : public webrtc::I420BufferInterface {
 public:
  explicit I420FrameAdapter(scoped_refptr<media::VideoFrame> frame)
      : frame_(std::move(frame)) {
    DCHECK_EQ(frame_->format(), media::PIXEL_FORMAT_I420);
    DCHECK_EQ(frame_->visible_rect().size(), frame_->natural_size());
  }

  int width() const override { return frame_->visible_rect().width(); }
  int height() const override { return frame_->visible_rect().height(); }

  const uint8_t* DataY() const override {
    return frame_->visible_data(media::VideoFrame::kYPlane);
  }

  const uint8_t* DataU() const override {
    return frame_->visible_data(media::VideoFrame::kUPlane);
  }

  const uint8_t* DataV() const override {
    return frame_->visible_data(media::VideoFrame::kVPlane);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::kYPlane);
  }

  int StrideU() const override {
    return frame_->stride(media::VideoFrame::kUPlane);
  }

  int StrideV() const override {
    return frame_->stride(media::VideoFrame::kVPlane);
  }

 protected:
  scoped_refptr<media::VideoFrame> frame_;
};

class I420AFrameAdapter : public webrtc::I420ABufferInterface {
 public:
  explicit I420AFrameAdapter(scoped_refptr<media::VideoFrame> frame)
      : frame_(std::move(frame)) {
    DCHECK_EQ(frame_->format(), media::PIXEL_FORMAT_I420A);
    DCHECK_EQ(frame_->visible_rect().size(), frame_->natural_size());
  }

  int width() const override { return frame_->visible_rect().width(); }
  int height() const override { return frame_->visible_rect().height(); }

  const uint8_t* DataY() const override {
    return frame_->visible_data(media::VideoFrame::kYPlane);
  }

  const uint8_t* DataU() const override {
    return frame_->visible_data(media::VideoFrame::kUPlane);
  }

  const uint8_t* DataV() const override {
    return frame_->visible_data(media::VideoFrame::kVPlane);
  }

  const uint8_t* DataA() const override {
    return frame_->visible_data(media::VideoFrame::kAPlane);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::kYPlane);
  }

  int StrideU() const override {
    return frame_->stride(media::VideoFrame::kUPlane);
  }

  int StrideV() const override {
    return frame_->stride(media::VideoFrame::kVPlane);
  }

  int StrideA() const override {
    return frame_->stride(media::VideoFrame::kAPlane);
  }

 protected:
  scoped_refptr<media::VideoFrame> frame_;
};

class NV12FrameAdapter : public webrtc::NV12BufferInterface {
 public:
  explicit NV12FrameAdapter(scoped_refptr<media::VideoFrame> frame)
      : frame_(std::move(frame)) {
    DCHECK_EQ(frame_->format(), media::PIXEL_FORMAT_NV12);
    DCHECK_EQ(frame_->visible_rect().size(), frame_->natural_size());
  }

  int width() const override { return frame_->visible_rect().width(); }
  int height() const override { return frame_->visible_rect().height(); }

  const uint8_t* DataY() const override {
    return frame_->visible_data(media::VideoFrame::kYPlane);
  }

  const uint8_t* DataUV() const override {
    return frame_->visible_data(media::VideoFrame::kUVPlane);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::kYPlane);
  }

  int StrideUV() const override {
    return frame_->stride(media::VideoFrame::kUVPlane);
  }

  rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override {
    rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer;
    i420_buffer = webrtc::I420Buffer::Create(width(), height());
    libyuv::NV12ToI420(DataY(), StrideY(), DataUV(), StrideUV(),
                       i420_buffer->MutableDataY(), i420_buffer->StrideY(),
                       i420_buffer->MutableDataU(), i420_buffer->StrideU(),
                       i420_buffer->MutableDataV(), i420_buffer->StrideV(),
                       width(), height());
    return i420_buffer;
  }

 protected:
  scoped_refptr<media::VideoFrame> frame_;
};

rtc::scoped_refptr<webrtc::VideoFrameBuffer> MakeFrameAdapter(
    scoped_refptr<media::VideoFrame> video_frame) {
  switch (video_frame->format()) {
    case media::PIXEL_FORMAT_I420:
      return new rtc::RefCountedObject<I420FrameAdapter>(
          std::move(video_frame));
    case media::PIXEL_FORMAT_I420A:
      return new rtc::RefCountedObject<I420AFrameAdapter>(
          std::move(video_frame));
    case media::PIXEL_FORMAT_NV12:
      return new rtc::RefCountedObject<NV12FrameAdapter>(
          std::move(video_frame));
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<media::VideoFrame> MakeScaledI420VideoFrame(
    scoped_refptr<media::VideoFrame> source_frame,
    scoped_refptr<blink::WebRtcVideoFrameAdapter::SharedResources>
        shared_resources) {
  // ARGB pixel format may be produced by readback of texture backed frames.
  DCHECK(source_frame->format() == media::PIXEL_FORMAT_I420 ||
         source_frame->format() == media::PIXEL_FORMAT_I420A ||
         source_frame->format() == media::PIXEL_FORMAT_ARGB ||
         source_frame->format() == media::PIXEL_FORMAT_XRGB ||
         source_frame->format() == media::PIXEL_FORMAT_ABGR ||
         source_frame->format() == media::PIXEL_FORMAT_XBGR);
  const bool has_alpha = source_frame->format() == media::PIXEL_FORMAT_I420A;
  // Convert to I420 and scale to the natural size specified in
  // |source_frame|.
  auto dst_frame = shared_resources->CreateFrame(
      has_alpha ? media::PIXEL_FORMAT_I420A : media::PIXEL_FORMAT_I420,
      source_frame->natural_size(), gfx::Rect(source_frame->natural_size()),
      source_frame->natural_size(), source_frame->timestamp());
  if (!dst_frame) {
    LOG(ERROR) << "Failed to create I420 frame from pool.";
    return nullptr;
  }
  dst_frame->metadata().MergeMetadataFrom(source_frame->metadata());

  switch (source_frame->format()) {
    case media::PIXEL_FORMAT_I420A:
      libyuv::ScalePlane(source_frame->visible_data(media::VideoFrame::kAPlane),
                         source_frame->stride(media::VideoFrame::kAPlane),
                         source_frame->visible_rect().width(),
                         source_frame->visible_rect().height(),
                         dst_frame->data(media::VideoFrame::kAPlane),
                         dst_frame->stride(media::VideoFrame::kAPlane),
                         dst_frame->coded_size().width(),
                         dst_frame->coded_size().height(), libyuv::kFilterBox);
      // Fallthrough to I420 in order to scale the YUV planes as well.
      ABSL_FALLTHROUGH_INTENDED;
    case media::PIXEL_FORMAT_I420:
      libyuv::I420Scale(source_frame->visible_data(media::VideoFrame::kYPlane),
                        source_frame->stride(media::VideoFrame::kYPlane),
                        source_frame->visible_data(media::VideoFrame::kUPlane),
                        source_frame->stride(media::VideoFrame::kUPlane),
                        source_frame->visible_data(media::VideoFrame::kVPlane),
                        source_frame->stride(media::VideoFrame::kVPlane),
                        source_frame->visible_rect().width(),
                        source_frame->visible_rect().height(),
                        dst_frame->data(media::VideoFrame::kYPlane),
                        dst_frame->stride(media::VideoFrame::kYPlane),
                        dst_frame->data(media::VideoFrame::kUPlane),
                        dst_frame->stride(media::VideoFrame::kUPlane),
                        dst_frame->data(media::VideoFrame::kVPlane),
                        dst_frame->stride(media::VideoFrame::kVPlane),
                        dst_frame->coded_size().width(),
                        dst_frame->coded_size().height(), libyuv::kFilterBox);
      break;
    case media::PIXEL_FORMAT_XRGB:
    case media::PIXEL_FORMAT_ARGB:
    case media::PIXEL_FORMAT_XBGR:
    case media::PIXEL_FORMAT_ABGR: {
      auto convert_func = (source_frame->format() == media::PIXEL_FORMAT_XRGB ||
                           source_frame->format() == media::PIXEL_FORMAT_ARGB)
                              ? libyuv::ARGBToI420
                              : libyuv::ABGRToI420;

      auto visible_size = source_frame->visible_rect().size();
      if (visible_size == dst_frame->coded_size()) {
        // Direct conversion to dst_frame with no scaling.
        convert_func(source_frame->visible_data(media::VideoFrame::kARGBPlane),
                     source_frame->stride(media::VideoFrame::kARGBPlane),
                     dst_frame->data(media::VideoFrame::kYPlane),
                     dst_frame->stride(media::VideoFrame::kYPlane),
                     dst_frame->data(media::VideoFrame::kUPlane),
                     dst_frame->stride(media::VideoFrame::kUPlane),
                     dst_frame->data(media::VideoFrame::kVPlane),
                     dst_frame->stride(media::VideoFrame::kVPlane),
                     visible_size.width(), visible_size.height());
      } else {
        // Convert to I420 tmp image and then scale to the dst_frame.
        auto tmp_frame = shared_resources->CreateTemporaryFrame(
            media::PIXEL_FORMAT_I420, visible_size, gfx::Rect(visible_size),
            visible_size, source_frame->timestamp());
        convert_func(source_frame->visible_data(media::VideoFrame::kARGBPlane),
                     source_frame->stride(media::VideoFrame::kARGBPlane),
                     tmp_frame->data(media::VideoFrame::kYPlane),
                     tmp_frame->stride(media::VideoFrame::kYPlane),
                     tmp_frame->data(media::VideoFrame::kUPlane),
                     tmp_frame->stride(media::VideoFrame::kUPlane),
                     tmp_frame->data(media::VideoFrame::kVPlane),
                     tmp_frame->stride(media::VideoFrame::kVPlane),
                     visible_size.width(), visible_size.height());
        libyuv::I420Scale(tmp_frame->data(media::VideoFrame::kYPlane),
                          tmp_frame->stride(media::VideoFrame::kYPlane),
                          tmp_frame->data(media::VideoFrame::kUPlane),
                          tmp_frame->stride(media::VideoFrame::kUPlane),
                          tmp_frame->data(media::VideoFrame::kVPlane),
                          tmp_frame->stride(media::VideoFrame::kVPlane),
                          visible_size.width(), visible_size.height(),
                          dst_frame->data(media::VideoFrame::kYPlane),
                          dst_frame->stride(media::VideoFrame::kYPlane),
                          dst_frame->data(media::VideoFrame::kUPlane),
                          dst_frame->stride(media::VideoFrame::kUPlane),
                          dst_frame->data(media::VideoFrame::kVPlane),
                          dst_frame->stride(media::VideoFrame::kVPlane),
                          dst_frame->coded_size().width(),
                          dst_frame->coded_size().height(), libyuv::kFilterBox);
      }
    } break;
    default:
      NOTREACHED();
  }
  return dst_frame;
}

scoped_refptr<media::VideoFrame> MakeScaledNV12VideoFrame(
    scoped_refptr<media::VideoFrame> source_frame,
    scoped_refptr<blink::WebRtcVideoFrameAdapter::SharedResources>
        shared_resources) {
  DCHECK_EQ(source_frame->format(), media::PIXEL_FORMAT_NV12);
  auto dst_frame = shared_resources->CreateFrame(
      media::PIXEL_FORMAT_NV12, source_frame->natural_size(),
      gfx::Rect(source_frame->natural_size()), source_frame->natural_size(),
      source_frame->timestamp());
  dst_frame->metadata().MergeMetadataFrom(source_frame->metadata());
  const auto& nv12_planes = dst_frame->layout().planes();
  libyuv::NV12Scale(source_frame->visible_data(media::VideoFrame::kYPlane),
                    source_frame->stride(media::VideoFrame::kYPlane),
                    source_frame->visible_data(media::VideoFrame::kUVPlane),
                    source_frame->stride(media::VideoFrame::kUVPlane),
                    source_frame->visible_rect().width(),
                    source_frame->visible_rect().height(),
                    dst_frame->data(media::VideoFrame::kYPlane),
                    nv12_planes[media::VideoFrame::kYPlane].stride,
                    dst_frame->data(media::VideoFrame::kUVPlane),
                    nv12_planes[media::VideoFrame::kUVPlane].stride,
                    dst_frame->coded_size().width(),
                    dst_frame->coded_size().height(), libyuv::kFilterBox);
  return dst_frame;
}

scoped_refptr<media::VideoFrame> MaybeConvertAndScaleFrame(
    scoped_refptr<media::VideoFrame> source_frame,
    scoped_refptr<blink::WebRtcVideoFrameAdapter::SharedResources>
        shared_resources) {
  if (!source_frame)
    return nullptr;
  // Texture frames may be readback in ARGB format.
  RTC_DCHECK(source_frame->format() == media::PIXEL_FORMAT_I420 ||
             source_frame->format() == media::PIXEL_FORMAT_I420A ||
             source_frame->format() == media::PIXEL_FORMAT_NV12 ||
             source_frame->format() == media::PIXEL_FORMAT_ARGB ||
             source_frame->format() == media::PIXEL_FORMAT_XRGB ||
             source_frame->format() == media::PIXEL_FORMAT_ABGR ||
             source_frame->format() == media::PIXEL_FORMAT_XBGR);
  RTC_DCHECK(shared_resources);

  const bool source_is_i420 =
      source_frame->format() == media::PIXEL_FORMAT_I420 ||
      source_frame->format() == media::PIXEL_FORMAT_I420A;
  const bool source_is_nv12 =
      source_frame->format() == media::PIXEL_FORMAT_NV12;
  const bool no_scaling_needed =
      source_frame->natural_size() == source_frame->visible_rect().size();

  if ((source_is_nv12 || source_is_i420) && no_scaling_needed) {
    // |source_frame| already has correct pixel format and resolution.
    return source_frame;
  } else if (source_is_nv12) {
    // Output NV12 only if no conversion is needed.
    return MakeScaledNV12VideoFrame(std::move(source_frame),
                                    std::move(shared_resources));
  }
  return MakeScaledI420VideoFrame(std::move(source_frame),
                                  std::move(shared_resources));
}

}  // anonymous namespace

namespace blink {

// static
bool CanConvertToWebRtcVideoFrameBuffer(const media::VideoFrame* frame) {
  // Currently accept I420, I420A, NV12 formats in a mapped frame,
  // or a texture or GPU memory buffer frame.
  return (frame->IsMappable() &&
          base::Contains(GetPixelFormatsMappableToWebRtcVideoFrameBuffer(),
                         frame->format())) ||
         frame->storage_type() ==
             media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER ||
         frame->HasTextures();
}

// static
base::span<const media::VideoPixelFormat>
GetPixelFormatsMappableToWebRtcVideoFrameBuffer() {
  static constexpr const media::VideoPixelFormat
      kGetPixelFormatsMappableToWebRtcVideoFrameBuffer[] = {
          media::PIXEL_FORMAT_I420, media::PIXEL_FORMAT_I420A,
          media::PIXEL_FORMAT_NV12, media::PIXEL_FORMAT_ARGB,
          media::PIXEL_FORMAT_XRGB, media::PIXEL_FORMAT_ABGR,
          media::PIXEL_FORMAT_XBGR};
  return base::make_span(kGetPixelFormatsMappableToWebRtcVideoFrameBuffer);
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer> ConvertToWebRtcVideoFrameBuffer(
    scoped_refptr<media::VideoFrame> video_frame,
    scoped_refptr<WebRtcVideoFrameAdapter::SharedResources> shared_resources) {
  DCHECK(CanConvertToWebRtcVideoFrameBuffer(video_frame.get()))
      << "Can not create WebRTC frame buffer for frame "
      << video_frame->AsHumanReadableString();

  if (video_frame->storage_type() ==
      media::VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER) {
    auto converted_frame =
        shared_resources
            ? shared_resources->ConstructVideoFrameFromGpu(video_frame)
            : nullptr;
    converted_frame =
        MaybeConvertAndScaleFrame(converted_frame, shared_resources);
    if (!converted_frame) {
      return MakeFrameAdapter(media::VideoFrame::CreateColorFrame(
          video_frame->natural_size(), 0u, 0x80, 0x80,
          video_frame->timestamp()));
    }
    return MakeFrameAdapter(std::move(converted_frame));
  } else if (video_frame->HasTextures()) {
    auto converted_frame =
        shared_resources
            ? shared_resources->ConstructVideoFrameFromTexture(video_frame)
            : nullptr;
    converted_frame =
        MaybeConvertAndScaleFrame(converted_frame, shared_resources);
    if (!converted_frame) {
      DLOG(ERROR) << "Texture backed frame cannot be accessed.";
      return MakeFrameAdapter(media::VideoFrame::CreateColorFrame(
          video_frame->natural_size(), 0u, 0x80, 0x80,
          video_frame->timestamp()));
    }
    return MakeFrameAdapter(std::move(converted_frame));
  }

  // Since scaling is required, hard-apply both the cropping and scaling
  // before we hand the frame over to WebRTC.
  scoped_refptr<media::VideoFrame> scaled_frame =
      MaybeConvertAndScaleFrame(video_frame, shared_resources);
  return MakeFrameAdapter(std::move(scaled_frame));
}

scoped_refptr<media::VideoFrame> ConvertFromMappedWebRtcVideoFrameBuffer(
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer,
    base::TimeDelta timestamp) {
  RTC_DCHECK(buffer->type() != webrtc::VideoFrameBuffer::Type::kNative);
  const gfx::Size size(buffer->width(), buffer->height());
  scoped_refptr<media::VideoFrame> video_frame;
  switch (buffer->type()) {
    case webrtc::VideoFrameBuffer::Type::kI420A: {
      const webrtc::I420ABufferInterface* yuva_buffer = buffer->GetI420A();
      video_frame = media::VideoFrame::WrapExternalYuvaData(
          media::PIXEL_FORMAT_I420A, size, gfx::Rect(size), size,
          yuva_buffer->StrideY(), yuva_buffer->StrideU(),
          yuva_buffer->StrideV(), yuva_buffer->StrideA(),
          const_cast<uint8_t*>(yuva_buffer->DataY()),
          const_cast<uint8_t*>(yuva_buffer->DataU()),
          const_cast<uint8_t*>(yuva_buffer->DataV()),
          const_cast<uint8_t*>(yuva_buffer->DataA()), timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kI420: {
      const webrtc::I420BufferInterface* yuv_buffer = buffer->GetI420();
      video_frame = media::VideoFrame::WrapExternalYuvData(
          media::PIXEL_FORMAT_I420, size, gfx::Rect(size), size,
          yuv_buffer->StrideY(), yuv_buffer->StrideU(), yuv_buffer->StrideV(),
          const_cast<uint8_t*>(yuv_buffer->DataY()),
          const_cast<uint8_t*>(yuv_buffer->DataU()),
          const_cast<uint8_t*>(yuv_buffer->DataV()), timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kI444: {
      const webrtc::I444BufferInterface* yuv_buffer = buffer->GetI444();
      video_frame = media::VideoFrame::WrapExternalYuvData(
          media::PIXEL_FORMAT_I444, size, gfx::Rect(size), size,
          yuv_buffer->StrideY(), yuv_buffer->StrideU(), yuv_buffer->StrideV(),
          const_cast<uint8_t*>(yuv_buffer->DataY()),
          const_cast<uint8_t*>(yuv_buffer->DataU()),
          const_cast<uint8_t*>(yuv_buffer->DataV()), timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kI010: {
      const webrtc::I010BufferInterface* yuv_buffer = buffer->GetI010();
      // WebRTC defines I010 data as uint16 whereas Chromium uses uint8 for all
      // video formats, so conversion and cast is needed.
      video_frame = media::VideoFrame::WrapExternalYuvData(
          media::PIXEL_FORMAT_YUV420P10, size, gfx::Rect(size), size,
          yuv_buffer->StrideY() * 2, yuv_buffer->StrideU() * 2,
          yuv_buffer->StrideV() * 2,
          const_cast<uint8_t*>(
              reinterpret_cast<const uint8_t*>(yuv_buffer->DataY())),
          const_cast<uint8_t*>(
              reinterpret_cast<const uint8_t*>(yuv_buffer->DataU())),
          const_cast<uint8_t*>(
              reinterpret_cast<const uint8_t*>(yuv_buffer->DataV())),
          timestamp);
      break;
    }
    case webrtc::VideoFrameBuffer::Type::kNV12: {
      const webrtc::NV12BufferInterface* nv12_buffer = buffer->GetNV12();
      video_frame = media::VideoFrame::WrapExternalYuvData(
          media::PIXEL_FORMAT_NV12, size, gfx::Rect(size), size,
          nv12_buffer->StrideY(), nv12_buffer->StrideUV(),
          const_cast<uint8_t*>(nv12_buffer->DataY()),
          const_cast<uint8_t*>(nv12_buffer->DataUV()), timestamp);
      break;
    }
    default:
      NOTREACHED();
      return nullptr;
  }
  // The bind ensures that we keep a reference to the underlying buffer.
  video_frame->AddDestructionObserver(ConvertToBaseOnceCallback(
      CrossThreadBindOnce([](const scoped_refptr<rtc::RefCountInterface>&) {},
                          scoped_refptr<webrtc::VideoFrameBuffer>(buffer))));
  return video_frame;
}

}  // namespace blink
