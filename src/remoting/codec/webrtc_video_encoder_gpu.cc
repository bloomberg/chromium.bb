// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/webrtc_video_encoder_gpu.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_post_task.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/video_frame.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "media/video/video_encode_accelerator.h"
#include "remoting/base/constants.h"
#include "remoting/codec/encoder_bitrate_filter.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace {

using media::VideoCodecProfile;
using media::VideoFrame;
using media::VideoPixelFormat;

// Currently, the WebrtcVideoEncoderWrapper only encodes a single frame at a
// time. Thus, there's no reason to have this set to anything greater than one.
const int kWebrtcVideoEncoderGpuOutputBufferCount = 1;

constexpr VideoCodecProfile kH264Profile = VideoCodecProfile::H264PROFILE_MAIN;

constexpr int kH264MinimumTargetBitrateKbpsPerMegapixel = 1800;

gpu::GpuPreferences CreateGpuPreferences() {
  gpu::GpuPreferences gpu_preferences;
#if defined(OS_WIN)
  gpu_preferences.enable_media_foundation_vea_on_windows7 = true;
#endif
  return gpu_preferences;
}

gpu::GpuDriverBugWorkarounds CreateGpuWorkarounds() {
  gpu::GpuDriverBugWorkarounds gpu_workarounds;
  return gpu_workarounds;
}

struct OutputBuffer {
  base::UnsafeSharedMemoryRegion region;
  base::WritableSharedMemoryMapping mapping;

  bool IsValid();
};

bool OutputBuffer::IsValid() {
  return region.IsValid() && mapping.IsValid();
}

}  // namespace

namespace remoting {

// WebrtcVideoEncoderGpu::Core handles the initialization, usage, and teardown
// of a VideoEncodeAccelerator object which is used to encode desktop frames for
// presentation on the client.
//
// A brief explanation of how this class is initialized:
// 1. An instance of WebrtcVideoEncoderGpu is created using the static
//      CreateForH264() function. At this point its |core_| member (an instance
//      of this class) is created with a state of UNINITIALIZED. After this
//      point, WebrtcVideoEncoderGpu will forward all Encode calls to its
//      |core_| member.
// 2. On the first encode call, the incoming DesktopFrame's dimensions are
//      stored and the Encode params are saved in |pending_encode_|. Before
//      returning, BeginInitialization() is called.
// 3. In BeginInitialization(), the Core instance constructs the
//      VideoEncodeAccelerator using the saved dimensions from the DesktopFrame.
//      If the VideoEncodeAccelerator is constructed successfully, the state is
//      set to INITIALIZING. If not, the state isset to INIITALIZATION_ERROR.
// 4. Some time later, the VideoEncodeAccelerator sets itself up and is ready
//      to encode. At this point, it calls the Core instance's
//      RequireBitstreamBuffers() method. Once bitstream buffers are allocated,
//      the state is INITIALIZED.
class WebrtcVideoEncoderGpu::Core
    : public WebrtcVideoEncoder,
      public media::VideoEncodeAccelerator::Client {
 public:
  explicit Core(media::VideoCodecProfile codec_profile);
  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;
  ~Core() override;

  // WebrtcVideoEncoder interface.
  void Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
              const FrameParams& params,
              WebrtcVideoEncoder::EncodeCallback done) override;

  // VideoEncodeAccelerator::Client interface.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyError(media::VideoEncodeAccelerator::Error error) override;

 private:
  enum State { UNINITIALIZED, INITIALIZING, INITIALIZED, INITIALIZATION_ERROR };

  void BeginInitialization();

  void UseOutputBitstreamBufferId(int32_t bitstream_buffer_id);

  void RunAnyPendingEncode();

#if defined(OS_WIN)
  // This object is required by Chromium to ensure proper init/uninit of COM on
  // this thread.  The guidance is to match the lifetime of this object to the
  // lifetime of the thread if possible.
  std::unique_ptr<base::win::ScopedCOMInitializer> scoped_com_initializer_;
#endif

  State state_ = UNINITIALIZED;

  // Only after the first encode request do we know how large the incoming
  // frames will be. Thus, we initialize after the first encode request,
  // postponing the encode until the encoder has been initialized.
  base::OnceClosure pending_encode_;

  std::unique_ptr<media::VideoEncodeAccelerator> video_encode_accelerator_;

  base::TimeDelta previous_timestamp_;

  media::VideoCodecProfile codec_profile_;

  // Shared memory with which the VEA transfers output to us.
  std::vector<std::unique_ptr<OutputBuffer>> output_buffers_;

  gfx::Size input_coded_size_;
  gfx::Size input_visible_size_;

  size_t output_buffer_size_;

  base::flat_map<base::TimeDelta, WebrtcVideoEncoder::EncodeCallback>
      callbacks_;

  EncoderBitrateFilter bitrate_filter_{
      kH264MinimumTargetBitrateKbpsPerMegapixel};

  THREAD_CHECKER(thread_checker_);
};

WebrtcVideoEncoderGpu::WebrtcVideoEncoderGpu(VideoCodecProfile codec_profile)
    : core_(std::make_unique<WebrtcVideoEncoderGpu::Core>(codec_profile)),
      hw_encode_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskPriority::HIGHEST},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED)) {}

WebrtcVideoEncoderGpu::~WebrtcVideoEncoderGpu() {
  hw_encode_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void WebrtcVideoEncoderGpu::Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
                                   const FrameParams& params,
                                   WebrtcVideoEncoder::EncodeCallback done) {
  DCHECK(core_);
  DCHECK(frame);
  DCHECK(done);
  DCHECK_GT(params.duration, base::Milliseconds(0));

  hw_encode_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebrtcVideoEncoderGpu::Core::Encode,
                     base::Unretained(core_.get()), std::move(frame), params,
                     base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                                        std::move(done))));
}

WebrtcVideoEncoderGpu::Core::Core(media::VideoCodecProfile codec_profile)
    : codec_profile_(codec_profile) {
  DETACH_FROM_THREAD(thread_checker_);
}

WebrtcVideoEncoderGpu::Core::~Core() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void WebrtcVideoEncoderGpu::Core::Encode(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    const FrameParams& params,
    WebrtcVideoEncoder::EncodeCallback done) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bitrate_filter_.SetFrameSize(frame->size().width(), frame->size().height());

  if (state_ == INITIALIZATION_ERROR) {
    // TODO(zijiehe): The screen resolution limitation of H264 encoder is much
    // smaller (3840x2176) than VP8 (16k x 16k) or VP9 (65k x 65k). It's more
    // likely the initialization may fail by using H264 encoder. We should
    // provide a way to tell the WebrtcVideoStream to stop the video stream.
    DLOG(ERROR) << "Encoder failed to initialize; dropping encode request";
    // Initialization fails only when the input frame size exceeds the
    // limitation.
    std::move(done).Run(EncodeResult::FRAME_SIZE_EXCEEDS_CAPABILITY, nullptr);
    return;
  }

  if (state_ == UNINITIALIZED ||
      input_visible_size_.width() != frame->size().width() ||
      input_visible_size_.height() != frame->size().height()) {
    input_visible_size_ =
        gfx::Size(frame->size().width(), frame->size().height());

    pending_encode_ = base::BindOnce(&WebrtcVideoEncoderGpu::Core::Encode,
                                     base::Unretained(this), std::move(frame),
                                     params, std::move(done));

    BeginInitialization();

    return;
  }

  // If we get to this point and state_ != INITIALIZED, we may be attempting to
  // have multiple outstanding encode requests, which is not currently
  // supported. The current assumption is that the WebrtcVideoEncoderWrapper
  // will wait for an Encode to finish before attempting another.
  DCHECK_EQ(state_, INITIALIZED);

  scoped_refptr<VideoFrame> video_frame = VideoFrame::CreateFrame(
      VideoPixelFormat::PIXEL_FORMAT_NV12, input_coded_size_,
      gfx::Rect(input_visible_size_), input_visible_size_, base::TimeDelta());

  base::TimeDelta new_timestamp = previous_timestamp_ + params.duration;
  video_frame->set_timestamp(new_timestamp);
  previous_timestamp_ = new_timestamp;

  // H264 encoder on Windows uses NV12 so convert here.
  libyuv::ARGBToNV12(frame->data(), frame->stride(),
                     video_frame->data(VideoFrame::kYPlane),
                     video_frame->stride(VideoFrame::kYPlane),
                     video_frame->data(VideoFrame::kUVPlane),
                     video_frame->stride(VideoFrame::kUVPlane),
                     video_frame->visible_rect().width(),
                     video_frame->visible_rect().height());

  callbacks_[video_frame->timestamp()] = std::move(done);

  if (params.bitrate_kbps > 0 && params.fps > 0) {
    // TODO(zijiehe): Forward frame_rate from FrameParams.
    bitrate_filter_.SetBandwidthEstimateKbps(params.bitrate_kbps);
    video_encode_accelerator_->RequestEncodingParametersChange(
        media::Bitrate::ConstantBitrate(bitrate_filter_.GetTargetBitrateKbps() *
                                        1000),
        params.fps);
  }
  video_encode_accelerator_->Encode(video_frame, params.key_frame);
}

void WebrtcVideoEncoderGpu::Core::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(state_ == INITIALIZING);

  input_coded_size_ = input_coded_size;
  output_buffer_size_ = output_buffer_size;

  output_buffers_.clear();

  for (unsigned int i = 0; i < kWebrtcVideoEncoderGpuOutputBufferCount; i++) {
    auto output_buffer = std::make_unique<OutputBuffer>();
    output_buffer->region =
        base::UnsafeSharedMemoryRegion::Create(output_buffer_size_);
    output_buffer->mapping = output_buffer->region.Map();
    // TODO(gusss): Do we need to handle mapping failure more gracefully?
    CHECK(output_buffer->IsValid());
    output_buffers_.push_back(std::move(output_buffer));
  }

  for (size_t i = 0; i < output_buffers_.size(); i++) {
    UseOutputBitstreamBufferId(i);
  }

  state_ = INITIALIZED;
  RunAnyPendingEncode();
}

void WebrtcVideoEncoderGpu::Core::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<EncodedFrame> encoded_frame =
      std::make_unique<EncodedFrame>();
  OutputBuffer* output_buffer = output_buffers_[bitstream_buffer_id].get();
  DCHECK(output_buffer->IsValid());
  base::span<char> data_span =
      output_buffer->mapping.GetMemoryAsSpan<char>(metadata.payload_size_bytes);
  encoded_frame->data.assign(data_span.begin(), data_span.end());
  encoded_frame->key_frame = metadata.key_frame;
  encoded_frame->size = webrtc::DesktopSize(input_coded_size_.width(),
                                            input_coded_size_.height());
  encoded_frame->quantizer = 0;
  encoded_frame->codec = webrtc::kVideoCodecH264;

  UseOutputBitstreamBufferId(bitstream_buffer_id);

  auto callback_it = callbacks_.find(metadata.timestamp);
  DCHECK(callback_it != callbacks_.end())
      << "Callback not found for timestamp " << metadata.timestamp;
  std::move(std::get<1>(*callback_it)).Run(
      EncodeResult::SUCCEEDED, std::move(encoded_frame));
  callbacks_.erase(metadata.timestamp);
}

void WebrtcVideoEncoderGpu::Core::NotifyError(
    media::VideoEncodeAccelerator::Error error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG(ERROR) << __func__ << " error: " << error;
}

void WebrtcVideoEncoderGpu::Core::BeginInitialization() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if defined(OS_WIN)
  if (!scoped_com_initializer_) {
    scoped_com_initializer_ =
        std::make_unique<base::win::ScopedCOMInitializer>();
  }
#endif

  VideoPixelFormat input_format = VideoPixelFormat::PIXEL_FORMAT_NV12;
  // TODO(zijiehe): implement some logical way to set an initial bitrate.
  // Currently we set the bitrate to 8M bits / 1M bytes per frame, and 30 frames
  // per second.
  media::Bitrate initial_bitrate =
      media::Bitrate::ConstantBitrate(kTargetFrameRate * 1024 * 1024 * 8);

  const media::VideoEncodeAccelerator::Config config(
      input_format, input_visible_size_, codec_profile_, initial_bitrate);
  video_encode_accelerator_ =
      media::GpuVideoEncodeAcceleratorFactory::CreateVEA(
          config, this, CreateGpuPreferences(), CreateGpuWorkarounds());

  if (!video_encode_accelerator_) {
    LOG(ERROR) << "Could not create VideoEncodeAccelerator";
    state_ = INITIALIZATION_ERROR;
    RunAnyPendingEncode();
    return;
  }

  state_ = INITIALIZING;
}

void WebrtcVideoEncoderGpu::Core::UseOutputBitstreamBufferId(
    int32_t bitstream_buffer_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  video_encode_accelerator_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
      bitstream_buffer_id,
      output_buffers_[bitstream_buffer_id]->region.Duplicate(),
      output_buffers_[bitstream_buffer_id]->region.GetSize()));
}

void WebrtcVideoEncoderGpu::Core::RunAnyPendingEncode() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (pending_encode_) {
    std::move(pending_encode_).Run();
  }
}

// static
std::unique_ptr<WebrtcVideoEncoder> WebrtcVideoEncoderGpu::CreateForH264() {
  LOG(WARNING) << "H264 video encoder is created.";
  // HIGH profile requires Windows 8 or upper. Considering encoding latency,
  // frame size and image quality, MAIN should be fine for us.
  return base::WrapUnique(new WebrtcVideoEncoderGpu(kH264Profile));
}

// static
bool WebrtcVideoEncoderGpu::IsSupportedByH264(
    const WebrtcVideoEncoderSelector::Profile& profile) {
#if defined(OS_WIN)
  // This object is required by Chromium to ensure proper init/uninit of COM on
  // this thread.  The guidance is to match the lifetime of this object to the
  // lifetime of the thread if possible.  Since we are still experimenting with
  // H.264 and run the encoder on a different thread, we use a locally scoped
  // object for now.
  base::win::ScopedCOMInitializer scoped_com_initializer;
#endif

  media::VideoEncodeAccelerator::SupportedProfiles profiles =
      media::GpuVideoEncodeAcceleratorFactory::GetSupportedProfiles(
          CreateGpuPreferences(), CreateGpuWorkarounds());
  for (const auto& supported_profile : profiles) {
    if (supported_profile.profile != kH264Profile) {
      continue;
    }

    double supported_framerate = supported_profile.max_framerate_numerator;
    supported_framerate /= supported_profile.max_framerate_denominator;
    if (profile.frame_rate > supported_framerate) {
      continue;
    }

    if (profile.resolution.GetArea() >
        supported_profile.max_resolution.GetArea()) {
      continue;
    }

    return true;
  }
  return false;
}

}  // namespace remoting
