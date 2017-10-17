// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/webrtc_video_encoder_gpu.h"

#include <utility>
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "third_party/libyuv/include/libyuv/convert_from_argb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace {
// Currently, the frame scheduler only encodes a single frame at a time. Thus,
// there's no reason to have this set to anything greater than one.
const int kWebrtcVideoEncoderGpuOutputBufferCount = 1;

void ArgbToI420(const webrtc::DesktopFrame& frame,
                scoped_refptr<media::VideoFrame> video_frame) {
  const uint8_t* rgb_data = frame.data();
  const int rgb_stride = frame.stride();
  const int y_stride = video_frame->stride(0);
  DCHECK_EQ(video_frame->stride(1), video_frame->stride(2));
  const int uv_stride = video_frame->stride(1);
  uint8_t* y_data = video_frame->data(0);
  uint8_t* u_data = video_frame->data(1);
  uint8_t* v_data = video_frame->data(2);
  libyuv::ARGBToI420(rgb_data, rgb_stride, y_data, y_stride, u_data, uv_stride,
                     v_data, uv_stride, video_frame->visible_rect().width(),
                     video_frame->visible_rect().height());
}
}  // namespace

namespace remoting {

WebrtcVideoEncoderGpu::WebrtcVideoEncoderGpu(
    media::VideoCodecProfile codec_profile)
    : state_(UNINITIALIZED),
      codec_profile_(codec_profile),
      weak_factory_(this) {}

WebrtcVideoEncoderGpu::~WebrtcVideoEncoderGpu() {}

// TODO(gusss): Implement either a software fallback or some sort of delay if
// the hardware encoder crashes.
// Bug: crbug.com/751870
void WebrtcVideoEncoderGpu::Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
                                   const FrameParams& params,
                                   WebrtcVideoEncoder::EncodeCallback done) {
  DCHECK(frame);
  DCHECK(done);
  DCHECK_GT(params.duration, base::TimeDelta::FromMilliseconds(0));

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

  DVLOG(3) << __func__ << " bitrate = " << params.bitrate_kbps << ", "
           << "duration = " << params.duration << ", "
           << "key_frame = " << params.key_frame;

  if (state_ == UNINITIALIZED ||
      input_visible_size_.width() != frame->size().width() ||
      input_visible_size_.height() != frame->size().height()) {
    DVLOG(3) << __func__ << " Currently not initialized for frame size "
             << frame->size().width() << "x" << frame->size().height()
             << ". Initializing.";
    input_visible_size_ =
        gfx::Size(frame->size().width(), frame->size().height());

    pending_encode_ = base::BindOnce(&WebrtcVideoEncoderGpu::Encode,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(frame), params, std::move(done));

    BeginInitialization();

    return;
  }

  // If we get to this point and state_ != INITIALIZED, we may be attempting to
  // have multiple outstanding encode requests, which is not currently
  // supported. The current assumption is that the FrameScheduler will wait for
  // an Encode to finish before attempting another.
  DCHECK_EQ(state_, INITIALIZED);

  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      media::VideoPixelFormat::PIXEL_FORMAT_I420, input_coded_size_,
      gfx::Rect(input_visible_size_), input_visible_size_, base::TimeDelta());

  base::TimeDelta new_timestamp = previous_timestamp_ + params.duration;
  video_frame->set_timestamp(new_timestamp);
  previous_timestamp_ = new_timestamp;

  // H264 encoder on Windows supports only I420.
  ArgbToI420(*frame, video_frame);

  callbacks_[video_frame->timestamp()] = std::move(done);

  video_encode_accelerator_->Encode(video_frame, params.key_frame);
}

void WebrtcVideoEncoderGpu::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& input_coded_size,
    size_t output_buffer_size) {
  DCHECK(state_ == INITIALIZING);

  DVLOG(3) << __func__ << ", "
           << "input_count = " << input_count << ", "
           << "input_coded_size = " << input_coded_size.width() << "x"
           << input_coded_size.height() << ", "
           << "output_buffer_size = " << output_buffer_size;

  required_input_frame_count_ = input_count;
  input_coded_size_ = input_coded_size;
  output_buffer_size_ = output_buffer_size;

  output_buffers_.clear();

  for (unsigned int i = 0; i < kWebrtcVideoEncoderGpuOutputBufferCount; ++i) {
    auto shm = base::MakeUnique<base::SharedMemory>();
    // TODO(gusss): Do we need to handle mapping failure more gracefully?
    CHECK(shm->CreateAndMapAnonymous(output_buffer_size_));
    output_buffers_.push_back(std::move(shm));
  }

  for (size_t i = 0; i < output_buffers_.size(); ++i) {
    UseOutputBitstreamBufferId(i);
  }

  state_ = INITIALIZED;
  RunAnyPendingEncode();
}

void WebrtcVideoEncoderGpu::BitstreamBufferReady(int32_t bitstream_buffer_id,
                                                 size_t payload_size,
                                                 bool key_frame,
                                                 base::TimeDelta timestamp) {
  DVLOG(3) << __func__ << " bitstream_buffer_id = " << bitstream_buffer_id
           << ", "
           << "payload_size = " << payload_size << ", "
           << "key_frame = " << key_frame << ", "
           << "timestamp ms = " << timestamp.InMilliseconds();

  std::unique_ptr<EncodedFrame> encoded_frame =
      base::MakeUnique<EncodedFrame>();
  base::SharedMemory* output_buffer =
      output_buffers_[bitstream_buffer_id].get();
  DCHECK(output_buffer->memory());
  encoded_frame->data.assign(reinterpret_cast<char*>(output_buffer->memory()),
                             payload_size);
  encoded_frame->key_frame = key_frame;
  encoded_frame->size = webrtc::DesktopSize(input_coded_size_.width(),
                                            input_coded_size_.height());
  encoded_frame->quantizer = 0;

  UseOutputBitstreamBufferId(bitstream_buffer_id);

  auto callback_it = callbacks_.find(timestamp);
  DCHECK(callback_it != callbacks_.end())
      << "Callback not found for timestamp " << timestamp;
  std::move(std::get<1>(*callback_it)).Run(
      EncodeResult::SUCCEEDED, std::move(encoded_frame));
  callbacks_.erase(timestamp);
}

void WebrtcVideoEncoderGpu::NotifyError(
    media::VideoEncodeAccelerator::Error error) {
  LOG(ERROR) << __func__ << " error: " << error;
}

void WebrtcVideoEncoderGpu::BeginInitialization() {
  DVLOG(3) << __func__;

  media::VideoPixelFormat input_format =
      media::VideoPixelFormat::PIXEL_FORMAT_I420;
  // TODO(zijiehe): implement some logical way to set an initial bitrate.
  // Currently we set the bitrate to 8M bits / 1M bytes per frame, and 30 frames
  // per second.
  uint32_t initial_bitrate = 30 * 1024 * 1024 * 8;
  gpu::GpuPreferences gpu_preferences;

  video_encode_accelerator_ =
      media::GpuVideoEncodeAcceleratorFactory::CreateVEA(
          input_format, input_visible_size_, codec_profile_, initial_bitrate,
          this, gpu_preferences);

  if (!video_encode_accelerator_) {
    LOG(ERROR) << "Could not create VideoEncodeAccelerator";
    state_ = INITIALIZATION_ERROR;
    RunAnyPendingEncode();
    return;
  }

  state_ = INITIALIZING;
}

void WebrtcVideoEncoderGpu::UseOutputBitstreamBufferId(
    int32_t bitstream_buffer_id) {
  DVLOG(3) << __func__ << " id=" << bitstream_buffer_id;
  video_encode_accelerator_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
      bitstream_buffer_id,
      output_buffers_[bitstream_buffer_id]->handle().Duplicate(),
      output_buffer_size_));
}

void WebrtcVideoEncoderGpu::RunAnyPendingEncode() {
  if (pending_encode_)
    std::move(pending_encode_).Run();
}

// static
std::unique_ptr<WebrtcVideoEncoderGpu> WebrtcVideoEncoderGpu::CreateForH264() {
  DVLOG(3) << __func__;

  // MediaFoundationVideoEncodeAccelerator supports only baseline profile.
  // TODO(zijiehe): H264 encoder on Windows supports more input formats and
  // profiles than the limitation in MediaFoundationVideoEncodeAccelerator.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dd797816(v=vs.85).aspx
  // Loosen the limitation in MediaFoundationVideoEncodeAccelerator and use a
  // profile with higher quality here.
  return base::WrapUnique(new WebrtcVideoEncoderGpu(
      media::VideoCodecProfile::H264PROFILE_BASELINE));
}

}  // namespace remoting
