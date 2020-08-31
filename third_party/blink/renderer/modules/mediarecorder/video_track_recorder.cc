// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/paint/skia_paint_canvas.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/muxers/webm_muxer.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediarecorder/buildflags.h"
#include "third_party/blink/renderer/modules/mediarecorder/vea_encoder.h"
#include "third_party/blink/renderer/modules/mediarecorder/vpx_encoder.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(RTC_USE_H264)
#include "third_party/blink/renderer/modules/mediarecorder/h264_encoder.h"
#endif  // #if BUILDFLAG(RTC_USE_H264)

using media::VideoFrame;
using video_track_recorder::kVEAEncoderMinResolutionHeight;
using video_track_recorder::kVEAEncoderMinResolutionWidth;

namespace blink {

using CodecId = VideoTrackRecorder::CodecId;

libyuv::RotationMode MediaVideoRotationToRotationMode(
    media::VideoRotation rotation) {
  switch (rotation) {
    case media::VIDEO_ROTATION_0:
      return libyuv::kRotate0;
    case media::VIDEO_ROTATION_90:
      return libyuv::kRotate90;
    case media::VIDEO_ROTATION_180:
      return libyuv::kRotate180;
    case media::VIDEO_ROTATION_270:
      return libyuv::kRotate270;
  }
  NOTREACHED() << rotation;
  return libyuv::kRotate0;
}

namespace {

static const struct {
  CodecId codec_id;
  media::VideoCodecProfile min_profile;
  media::VideoCodecProfile max_profile;
} kPreferredCodecIdAndVEAProfiles[] = {
    {CodecId::VP8, media::VP8PROFILE_MIN, media::VP8PROFILE_MAX},
    {CodecId::VP9, media::VP9PROFILE_MIN, media::VP9PROFILE_MAX},
#if BUILDFLAG(RTC_USE_H264)
    {CodecId::H264, media::H264PROFILE_MIN, media::H264PROFILE_MAX}
#endif
};

static_assert(base::size(kPreferredCodecIdAndVEAProfiles) ==
                  static_cast<int>(CodecId::LAST),
              "|kPreferredCodecIdAndVEAProfiles| should consider all CodecIds");

// The maximum number of frames that we keep the reference alive for encode.
// This guarantees that there is limit on the number of frames in a FIFO queue
// that are being encoded and frames coming after this limit is reached are
// dropped.
// TODO(emircan): Make this a LIFO queue that has different sizes for each
// encoder implementation.
const int kMaxNumberOfFramesInEncode = 10;

// Obtains video encode accelerator's supported profiles.
media::VideoEncodeAccelerator::SupportedProfiles GetVEASupportedProfiles() {
  if (!Platform::Current()) {
    DVLOG(2) << "Couldn't access the render thread";
    return media::VideoEncodeAccelerator::SupportedProfiles();
  }

  media::GpuVideoAcceleratorFactories* const gpu_factories =
      Platform::Current()->GetGpuFactories();
  if (!gpu_factories || !gpu_factories->IsGpuVideoAcceleratorEnabled()) {
    DVLOG(2) << "Couldn't initialize GpuVideoAcceleratorFactories";
    return media::VideoEncodeAccelerator::SupportedProfiles();
  }
  return gpu_factories->GetVideoEncodeAcceleratorSupportedProfiles().value_or(
      media::VideoEncodeAccelerator::SupportedProfiles());
}

VideoTrackRecorderImpl::CodecEnumerator* GetCodecEnumerator() {
  static VideoTrackRecorderImpl::CodecEnumerator* enumerator =
      new VideoTrackRecorderImpl::CodecEnumerator(GetVEASupportedProfiles());
  return enumerator;
}

}  // anonymous namespace

VideoTrackRecorder::VideoTrackRecorder(
    base::OnceClosure on_track_source_ended_cb)
    : TrackRecorder(std::move(on_track_source_ended_cb)) {}

VideoTrackRecorderImpl::CodecEnumerator::CodecEnumerator(
    const media::VideoEncodeAccelerator::SupportedProfiles&
        vea_supported_profiles) {
  for (const auto& supported_profile : vea_supported_profiles) {
    const media::VideoCodecProfile codec = supported_profile.profile;
#if defined(OS_ANDROID)
    // TODO(mcasas): enable other codecs, https://crbug.com/638664.
    static_assert(media::VP8PROFILE_MAX + 1 == media::VP9PROFILE_MIN,
                  "VP8 and VP9 VideoCodecProfiles should be contiguous");
    if (codec < media::VP8PROFILE_MIN || codec > media::VP9PROFILE_MAX)
      continue;
#endif
    for (auto& codec_id_and_profile : kPreferredCodecIdAndVEAProfiles) {
      if (codec >= codec_id_and_profile.min_profile &&
          codec <= codec_id_and_profile.max_profile) {
        DVLOG(2) << "Accelerated codec found: " << media::GetProfileName(codec)
                 << ", min_resolution: "
                 << supported_profile.min_resolution.ToString()
                 << ", max_resolution: "
                 << supported_profile.max_resolution.ToString()
                 << ", max_framerate: "
                 << supported_profile.max_framerate_numerator << "/"
                 << supported_profile.max_framerate_denominator;
        auto iter = supported_profiles_.find(codec_id_and_profile.codec_id);
        if (iter == supported_profiles_.end()) {
          auto result = supported_profiles_.insert(
              codec_id_and_profile.codec_id,
              media::VideoEncodeAccelerator::SupportedProfiles());
          result.stored_value->value.push_back(supported_profile);
        } else {
          iter->value.push_back(supported_profile);
        }
        if (preferred_codec_id_ == CodecId::LAST)
          preferred_codec_id_ = codec_id_and_profile.codec_id;
      }
    }
  }
}

VideoTrackRecorderImpl::CodecEnumerator::~CodecEnumerator() = default;

VideoTrackRecorderImpl::CodecId
VideoTrackRecorderImpl::CodecEnumerator::GetPreferredCodecId() const {
  if (preferred_codec_id_ == CodecId::LAST)
    return CodecId::VP8;

  return preferred_codec_id_;
}

media::VideoCodecProfile
VideoTrackRecorderImpl::CodecEnumerator::GetFirstSupportedVideoCodecProfile(
    CodecId codec) const {
  const auto profile = supported_profiles_.find(codec);
  return profile == supported_profiles_.end()
             ? media::VIDEO_CODEC_PROFILE_UNKNOWN
             : profile->value.front().profile;
}

media::VideoEncodeAccelerator::SupportedProfiles
VideoTrackRecorderImpl::CodecEnumerator::GetSupportedProfiles(
    CodecId codec) const {
  const auto profile = supported_profiles_.find(codec);
  return profile == supported_profiles_.end()
             ? media::VideoEncodeAccelerator::SupportedProfiles()
             : profile->value;
}

VideoTrackRecorderImpl::Counter::Counter() : count_(0u) {}

VideoTrackRecorderImpl::Counter::~Counter() = default;

void VideoTrackRecorderImpl::Counter::IncreaseCount() {
  count_++;
}

void VideoTrackRecorderImpl::Counter::DecreaseCount() {
  count_--;
}

base::WeakPtr<VideoTrackRecorderImpl::Counter>
VideoTrackRecorderImpl::Counter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

VideoTrackRecorderImpl::Encoder::Encoder(
    const OnEncodedVideoCB& on_encoded_video_cb,
    int32_t bits_per_second,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> encoding_task_runner)
    : main_task_runner_(std::move(main_task_runner)),
      encoding_task_runner_(encoding_task_runner),
      paused_(false),
      on_encoded_video_cb_(on_encoded_video_cb),
      bits_per_second_(bits_per_second),
      num_frames_in_encode_(
          std::make_unique<VideoTrackRecorderImpl::Counter>()) {
  DCHECK(!on_encoded_video_cb_.is_null());
  if (encoding_task_runner_)
    return;

  encoding_thread_ = Thread::CreateThread(
      ThreadCreationParams(ThreadType::kVideoEncoderThread));

  encoding_task_runner_ = encoding_thread_->GetTaskRunner();
}

VideoTrackRecorderImpl::Encoder::~Encoder() {
  main_task_runner_->DeleteSoon(FROM_HERE, video_renderer_.release());
  if (origin_task_runner_ && !origin_task_runner_->BelongsToCurrentThread()) {
    origin_task_runner_->DeleteSoon(FROM_HERE,
                                    std::move(num_frames_in_encode_));
  }
}

void VideoTrackRecorderImpl::Encoder::StartFrameEncode(
    scoped_refptr<VideoFrame> video_frame,
    base::TimeTicks capture_timestamp) {
  // Cache the thread sending frames on first frame arrival.
  if (!origin_task_runner_.get())
    origin_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  DCHECK(origin_task_runner_->BelongsToCurrentThread());
  if (paused_)
    return;

  if (!(video_frame->format() == media::PIXEL_FORMAT_I420 ||
        video_frame->format() == media::PIXEL_FORMAT_ARGB ||
        video_frame->format() == media::PIXEL_FORMAT_ABGR ||
        video_frame->format() == media::PIXEL_FORMAT_I420A ||
        video_frame->format() == media::PIXEL_FORMAT_NV12 ||
        video_frame->format() == media::PIXEL_FORMAT_XRGB)) {
    NOTREACHED() << media::VideoPixelFormatToString(video_frame->format());
    return;
  }

  if (num_frames_in_encode_->count() > kMaxNumberOfFramesInEncode) {
    DLOG(WARNING) << "Too many frames are queued up. Dropping this one.";
    return;
  }

  if (video_frame->HasTextures() &&
      video_frame->storage_type() !=
          media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    PostCrossThreadTask(
        *main_task_runner_.get(), FROM_HERE,
        CrossThreadBindOnce(&Encoder::RetrieveFrameOnMainThread,
                            WrapRefCounted(this), std::move(video_frame),
                            capture_timestamp));

    return;
  }

  scoped_refptr<media::VideoFrame> frame = video_frame;
  if (frame->storage_type() != media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    // Drop alpha channel if the encoder does not support it yet.
    if (!CanEncodeAlphaChannel() &&
        video_frame->format() == media::PIXEL_FORMAT_I420A) {
      frame = media::WrapAsI420VideoFrame(video_frame);
    } else {
      frame = media::VideoFrame::WrapVideoFrame(
          video_frame, video_frame->format(), video_frame->visible_rect(),
          video_frame->natural_size());
    }
  }
  frame->AddDestructionObserver(media::BindToCurrentLoop(
      WTF::Bind(&VideoTrackRecorderImpl::Counter::DecreaseCount,
                num_frames_in_encode_->GetWeakPtr())));
  num_frames_in_encode_->IncreaseCount();

  PostCrossThreadTask(
      *encoding_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&Encoder::EncodeOnEncodingTaskRunner,
                          WrapRefCounted(this), frame, capture_timestamp));
}

void VideoTrackRecorderImpl::Encoder::RetrieveFrameOnMainThread(
    scoped_refptr<VideoFrame> video_frame,
    base::TimeTicks capture_timestamp) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  scoped_refptr<media::VideoFrame> frame;

  // |context_provider| is null if the GPU process has crashed or isn't there
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider =
      Platform::Current()->CreateSharedOffscreenGraphicsContext3DProvider();

  if (!context_provider) {
    // Send black frames (yuv = {0, 127, 127}).
    frame = media::VideoFrame::CreateColorFrame(
        video_frame->visible_rect().size(), 0u, 0x80, 0x80,
        video_frame->timestamp());
  } else {
    // Accelerated decoders produce ARGB/ABGR texture-backed frames (see
    // https://crbug.com/585242), fetch them using a PaintCanvasVideoRenderer.
    // Additionally, Macintosh accelerated decoders can produce XRGB content
    // and are treated the same way.
    //
    // TODO(crbug/1023390): Add browsertest for these.
    DCHECK(video_frame->HasTextures());
    DCHECK(video_frame->format() == media::PIXEL_FORMAT_ARGB ||
           video_frame->format() == media::PIXEL_FORMAT_ABGR ||
           video_frame->format() == media::PIXEL_FORMAT_XRGB);

    const gfx::Size& old_visible_size = video_frame->visible_rect().size();
    gfx::Size new_visible_size = old_visible_size;

    media::VideoRotation video_rotation = media::VIDEO_ROTATION_0;
    if (video_frame->metadata()->GetRotation(
            media::VideoFrameMetadata::ROTATION, &video_rotation) &&
        (video_rotation == media::VIDEO_ROTATION_90 ||
         video_rotation == media::VIDEO_ROTATION_270)) {
      new_visible_size.SetSize(old_visible_size.height(),
                               old_visible_size.width());
    }

    frame = media::VideoFrame::CreateFrame(
        media::PIXEL_FORMAT_I420, new_visible_size, gfx::Rect(new_visible_size),
        new_visible_size, video_frame->timestamp());

    const SkImageInfo info = SkImageInfo::MakeN32(
        frame->visible_rect().width(), frame->visible_rect().height(),
        kOpaque_SkAlphaType);

    // Create |surface_| if it doesn't exist or incoming resolution has changed.
    if (!canvas_ || canvas_->imageInfo().width() != info.width() ||
        canvas_->imageInfo().height() != info.height()) {
      bitmap_.allocPixels(info);
      canvas_ = std::make_unique<cc::SkiaPaintCanvas>(bitmap_);
    }
    if (!video_renderer_)
      video_renderer_.reset(new media::PaintCanvasVideoRenderer);

    context_provider->CopyVideoFrame(video_renderer_.get(), video_frame.get(),
                                     canvas_.get());

    SkPixmap pixmap;
    if (!bitmap_.peekPixels(&pixmap)) {
      DLOG(ERROR) << "Error trying to map PaintSurface's pixels";
      return;
    }

#if SK_PMCOLOR_BYTE_ORDER(R, G, B, A)
    const uint32_t source_pixel_format = libyuv::FOURCC_ABGR;
#else
    const uint32_t source_pixel_format = libyuv::FOURCC_ARGB;
#endif
    if (libyuv::ConvertToI420(static_cast<uint8_t*>(pixmap.writable_addr()),
                              pixmap.computeByteSize(),
                              frame->visible_data(media::VideoFrame::kYPlane),
                              frame->stride(media::VideoFrame::kYPlane),
                              frame->visible_data(media::VideoFrame::kUPlane),
                              frame->stride(media::VideoFrame::kUPlane),
                              frame->visible_data(media::VideoFrame::kVPlane),
                              frame->stride(media::VideoFrame::kVPlane),
                              0 /* crop_x */, 0 /* crop_y */, pixmap.width(),
                              pixmap.height(), old_visible_size.width(),
                              old_visible_size.height(),
                              MediaVideoRotationToRotationMode(video_rotation),
                              source_pixel_format) != 0) {
      DLOG(ERROR) << "Error converting frame to I420";
      return;
    }
  }

  PostCrossThreadTask(
      *encoding_task_runner_.get(), FROM_HERE,
      CrossThreadBindOnce(&Encoder::EncodeOnEncodingTaskRunner,
                          WrapRefCounted(this), frame, capture_timestamp));
}

// static
void VideoTrackRecorderImpl::Encoder::OnFrameEncodeCompleted(
    const OnEncodedVideoInternalCB& on_encoded_video_cb,
    const media::WebmMuxer::VideoParameters& params,
    std::string data,
    std::string alpha_data,
    base::TimeTicks capture_timestamp,
    bool keyframe) {
  DVLOG(1) << (keyframe ? "" : "non ") << "keyframe " << data.length() << "B, "
           << capture_timestamp << " ms";
  on_encoded_video_cb.Run(params, std::move(data), std::move(alpha_data),
                          capture_timestamp, keyframe);
}

void VideoTrackRecorderImpl::Encoder::SetPaused(bool paused) {
  if (!encoding_task_runner_->BelongsToCurrentThread()) {
    PostCrossThreadTask(
        *encoding_task_runner_.get(), FROM_HERE,
        CrossThreadBindOnce(&Encoder::SetPaused, WrapRefCounted(this), paused));
    return;
  }
  paused_ = paused;
}

bool VideoTrackRecorderImpl::Encoder::CanEncodeAlphaChannel() {
  return false;
}

// static
scoped_refptr<media::VideoFrame>
VideoTrackRecorderImpl::Encoder::ConvertToI420ForSoftwareEncoder(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK_EQ(frame->storage_type(),
            media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
  // NV12 is currently the only supported pixel format for GpuMemoryBuffer.
  DCHECK_EQ(frame->format(), media::VideoPixelFormat::PIXEL_FORMAT_NV12);

  auto* gmb = frame->GetGpuMemoryBuffer();
  if (!gmb->Map())
    return frame;
  scoped_refptr<media::VideoFrame> i420_frame = media::VideoFrame::CreateFrame(
      media::VideoPixelFormat::PIXEL_FORMAT_I420, frame->coded_size(),
      frame->visible_rect(), frame->natural_size(), frame->timestamp());
  auto ret = libyuv::NV12ToI420(
      static_cast<const uint8_t*>(gmb->memory(0)), gmb->stride(0),
      static_cast<const uint8_t*>(gmb->memory(1)), gmb->stride(1),
      i420_frame->data(media::VideoFrame::kYPlane),
      i420_frame->stride(media::VideoFrame::kYPlane),
      i420_frame->data(media::VideoFrame::kUPlane),
      i420_frame->stride(media::VideoFrame::kUPlane),
      i420_frame->data(media::VideoFrame::kVPlane),
      i420_frame->stride(media::VideoFrame::kVPlane),
      frame->coded_size().width(), frame->coded_size().height());
  gmb->Unmap();
  if (ret)
    return frame;
  return i420_frame;
}

// static
VideoTrackRecorderImpl::CodecId VideoTrackRecorderImpl::GetPreferredCodecId() {
  return GetCodecEnumerator()->GetPreferredCodecId();
}

// static
bool VideoTrackRecorderImpl::CanUseAcceleratedEncoder(CodecId codec,
                                                      size_t width,
                                                      size_t height,
                                                      double framerate) {
  const auto profiles = GetCodecEnumerator()->GetSupportedProfiles(codec);
  if (profiles.empty())
    return false;

  // Now we only consider the first profile.
  // TODO(crbug.com/931035): Handle multiple profile cases.
  const media::VideoEncodeAccelerator::SupportedProfile& profile = profiles[0];

  if (profile.profile == media::VIDEO_CODEC_PROFILE_UNKNOWN)
    return false;

  const gfx::Size& min_resolution = profile.min_resolution;
  const size_t min_width = static_cast<size_t>(
      std::max(kVEAEncoderMinResolutionWidth, min_resolution.width()));
  const size_t min_height = static_cast<size_t>(
      std::max(kVEAEncoderMinResolutionHeight, min_resolution.height()));

  const gfx::Size& max_resolution = profile.max_resolution;
  DCHECK_GE(max_resolution.width(), 0);
  const size_t max_width = static_cast<size_t>(max_resolution.width());
  DCHECK_GE(max_resolution.height(), 0);
  const size_t max_height = static_cast<size_t>(max_resolution.height());

  const bool width_within_range = max_width >= width && width >= min_width;
  const bool height_within_range = max_height >= height && height >= min_height;
  const bool valid_framerate = framerate * profile.max_framerate_denominator <=
                               profile.max_framerate_numerator;
  return width_within_range && height_within_range && valid_framerate;
}

VideoTrackRecorderImpl::VideoTrackRecorderImpl(
    CodecId codec,
    MediaStreamComponent* track,
    OnEncodedVideoCB on_encoded_video_cb,
    base::OnceClosure on_track_source_ended_cb,
    int32_t bits_per_second,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : VideoTrackRecorder(std::move(on_track_source_ended_cb)),
      track_(track),
      should_pause_encoder_on_initialization_(false),
      main_task_runner_(std::move(main_task_runner)) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DCHECK(track_);
  DCHECK(track_->Source()->GetType() == MediaStreamSource::kTypeVideo);

  initialize_encoder_cb_ = WTF::BindRepeating(
      &VideoTrackRecorderImpl::InitializeEncoder, weak_factory_.GetWeakPtr(),
      codec, std::move(on_encoded_video_cb), bits_per_second);

  // InitializeEncoder() will be called on Render Main thread.
  ConnectToTrack(media::BindToCurrentLoop(WTF::BindRepeating(
      initialize_encoder_cb_, true /* allow_vea_encoder */)));
}

VideoTrackRecorderImpl::~VideoTrackRecorderImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DisconnectFromTrack();
}

void VideoTrackRecorderImpl::Pause() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (encoder_)
    encoder_->SetPaused(true);
  else
    should_pause_encoder_on_initialization_ = true;
}

void VideoTrackRecorderImpl::Resume() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (encoder_)
    encoder_->SetPaused(false);
  else
    should_pause_encoder_on_initialization_ = false;
}

void VideoTrackRecorderImpl::OnVideoFrameForTesting(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks timestamp) {
  DVLOG(3) << __func__;

  if (!encoder_) {
    DCHECK(!initialize_encoder_cb_.is_null());
    initialize_encoder_cb_.Run(true /* allow_vea_encoder */, frame, timestamp);
  }

  encoder_->StartFrameEncode(std::move(frame), timestamp);
}

void VideoTrackRecorderImpl::InitializeEncoder(
    CodecId codec,
    const OnEncodedVideoCB& on_encoded_video_cb,
    int32_t bits_per_second,
    bool allow_vea_encoder,
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks capture_time) {
  DVLOG(3) << __func__ << frame->visible_rect().size().ToString();
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // Avoid reinitializing |encoder_| when there are multiple frames sent to the
  // sink to initialize, https://crbug.com/698441.
  if (encoder_)
    return;

  DisconnectFromTrack();

  const gfx::Size& input_size = frame->visible_rect().size();
  if (allow_vea_encoder && CanUseAcceleratedEncoder(codec, input_size.width(),
                                                    input_size.height())) {
    UMA_HISTOGRAM_BOOLEAN("Media.MediaRecorder.VEAUsed", true);
    const auto vea_profile =
        GetCodecEnumerator()->GetFirstSupportedVideoCodecProfile(codec);
    bool use_import_mode =
        frame->storage_type() == media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER;
    encoder_ = VEAEncoder::Create(
        on_encoded_video_cb,
        media::BindToCurrentLoop(WTF::BindRepeating(
            &VideoTrackRecorderImpl::OnError, weak_factory_.GetWeakPtr())),
        bits_per_second, vea_profile, input_size, use_import_mode,
        main_task_runner_);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Media.MediaRecorder.VEAUsed", false);
    switch (codec) {
#if BUILDFLAG(RTC_USE_H264)
      case CodecId::H264:
        encoder_ = base::MakeRefCounted<H264Encoder>(
            on_encoded_video_cb, bits_per_second, main_task_runner_);
        break;
#endif
      case CodecId::VP8:
      case CodecId::VP9:
        encoder_ = base::MakeRefCounted<VpxEncoder>(
            codec == CodecId::VP9, on_encoded_video_cb, bits_per_second,
            main_task_runner_);
        break;
      default:
        NOTREACHED() << "Unsupported codec " << static_cast<int>(codec);
    }
  }

  if (should_pause_encoder_on_initialization_)
    encoder_->SetPaused(should_pause_encoder_on_initialization_);

  // StartFrameEncode() will be called on Render IO thread.
  ConnectToTrack(ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
      &VideoTrackRecorderImpl::Encoder::StartFrameEncode, encoder_)));
}

void VideoTrackRecorderImpl::OnError() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);

  // InitializeEncoder() will be called to reinitialize encoder on Render Main
  // thread.
  DisconnectFromTrack();
  encoder_ = nullptr;
  ConnectToTrack(media::BindToCurrentLoop(
      WTF::BindRepeating(initialize_encoder_cb_, false /*allow_vea_encoder*/)));
}

void VideoTrackRecorderImpl::ConnectToTrack(
    const VideoCaptureDeliverFrameCB& callback) {
  auto* video_track =
      static_cast<MediaStreamVideoTrack*>(track_->GetPlatformTrack());
  video_track->AddSink(this, callback, false);
}

void VideoTrackRecorderImpl::DisconnectFromTrack() {
  auto* video_track =
      static_cast<MediaStreamVideoTrack*>(track_->GetPlatformTrack());
  video_track->RemoveSink(this);
}

VideoTrackRecorderPassthrough::VideoTrackRecorderPassthrough(
    MediaStreamComponent* track,
    OnEncodedVideoCB on_encoded_video_cb,
    base::OnceClosure on_track_source_ended_cb,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : VideoTrackRecorder(std::move(on_track_source_ended_cb)),
      track_(track),
      state_(KeyFrameState::kWaitingForKeyFrame),
      main_task_runner_(main_task_runner),
      callback_(std::move(on_encoded_video_cb)) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  // HandleEncodedVideoFrame() will be called on Render Main thread.
  // Note: Adding an encoded sink internally generates a new key frame
  // request, no need to RequestRefreshFrame().
  ConnectEncodedToTrack(
      WebMediaStreamTrack(track_),
      media::BindToCurrentLoop(WTF::BindRepeating(
          &VideoTrackRecorderPassthrough::HandleEncodedVideoFrame,
          weak_factory_.GetWeakPtr())));
}

VideoTrackRecorderPassthrough::~VideoTrackRecorderPassthrough() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DisconnectFromTrack();
}

void VideoTrackRecorderPassthrough::Pause() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  state_ = KeyFrameState::kPaused;
}

void VideoTrackRecorderPassthrough::Resume() {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  state_ = KeyFrameState::kWaitingForKeyFrame;
  RequestRefreshFrame();
}

void VideoTrackRecorderPassthrough::OnEncodedVideoFrameForTesting(
    scoped_refptr<EncodedVideoFrame> frame,
    base::TimeTicks capture_time) {
  HandleEncodedVideoFrame(frame, capture_time);
}

void VideoTrackRecorderPassthrough::RequestRefreshFrame() {
  auto* video_track =
      static_cast<MediaStreamVideoTrack*>(track_->GetPlatformTrack());
  DCHECK(video_track->source());
  video_track->source()->RequestRefreshFrame();
}

void VideoTrackRecorderPassthrough::DisconnectFromTrack() {
  // TODO(crbug.com/704136) : Remove this method when moving
  // MediaStreamVideoTrack to Oilpan's heap.
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  DisconnectEncodedFromTrack();
}

void VideoTrackRecorderPassthrough::HandleEncodedVideoFrame(
    scoped_refptr<EncodedVideoFrame> encoded_frame,
    base::TimeTicks estimated_capture_time) {
  DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  if (state_ == KeyFrameState::kPaused)
    return;
  if (state_ == KeyFrameState::kWaitingForKeyFrame &&
      !encoded_frame->IsKeyFrame()) {
    // Don't RequestRefreshFrame() here - we already did this implicitly when
    // Creating/Starting or explicitly when Resuming this object.
    return;
  }
  state_ = KeyFrameState::kKeyFrameReceivedOK;

  base::Optional<gfx::ColorSpace> color_space;
  if (encoded_frame->ColorSpace())
    color_space = encoded_frame->ColorSpace()->ToGfxColorSpace();
  auto span = encoded_frame->Data();
  const char* span_begin = reinterpret_cast<const char*>(span.data());
  std::string data(span_begin, span_begin + span.size());
  media::WebmMuxer::VideoParameters params(encoded_frame->Resolution(),
                                           /*framerate=*/0.0f,
                                           /*codec=*/encoded_frame->Codec(),
                                           color_space);
  callback_.Run(params, std::move(data), {}, estimated_capture_time,
                encoded_frame->IsKeyFrame());
}

}  // namespace blink
