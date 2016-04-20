// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_frame_scheduler.h"

#include <memory>

#include "base/logging.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"

namespace remoting {
namespace protocol {

// The frame scheduler currently uses a simple polling technique
// at 30 FPS to capture, encode and send frames over webrtc transport.
// An improved solution will use target bitrate feedback to pace out
// the capture rate.
WebRtcFrameScheduler::WebRtcFrameScheduler(
    scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
    std::unique_ptr<webrtc::DesktopCapturer> capturer,
    WebrtcTransport* webrtc_transport,
    std::unique_ptr<VideoEncoder> encoder)
    : encode_task_runner_(encode_task_runner),
      capturer_(std::move(capturer)),
      webrtc_transport_(webrtc_transport),
      encoder_(std::move(encoder)),
      weak_factory_(this) {
  DCHECK(encode_task_runner_);
  DCHECK(capturer_);
  DCHECK(webrtc_transport_);
  DCHECK(encoder_);
  // Does not really start anything. Registers callback on this class.
  capturer_->Start(this);
  capture_timer_.reset(new base::RepeatingTimer());
}

WebRtcFrameScheduler::~WebRtcFrameScheduler() {
  encode_task_runner_->DeleteSoon(FROM_HERE, encoder_.release());
}

void WebRtcFrameScheduler::Start() {
  // Register for PLI requests.
  webrtc_transport_->video_encoder_factory()->SetKeyFrameRequestCallback(
      base::Bind(&WebRtcFrameScheduler::SetKeyFrameRequest,
                 base::Unretained(this)));
  capture_timer_->Start(FROM_HERE, base::TimeDelta::FromSeconds(1) / 30, this,
                        &WebRtcFrameScheduler::CaptureNextFrame);
}

void WebRtcFrameScheduler::Stop() {
  // Clear PLI request callback.
  webrtc_transport_->video_encoder_factory()->SetKeyFrameRequestCallback(
      base::Closure());
  // Cancel any pending encode.
  task_tracker_.TryCancelAll();
  capture_timer_->Stop();
}

void WebRtcFrameScheduler::Pause(bool pause) {
  if (pause) {
    Stop();
  } else {
    Start();
  }
}

void WebRtcFrameScheduler::SetSizeCallback(
    const VideoStream::SizeCallback& callback) {
  size_callback_ = callback;
}

void WebRtcFrameScheduler::SetKeyFrameRequest() {
  VLOG(1) << "Request key frame";
  base::AutoLock lock(lock_);
  key_frame_request_ = true;
}

bool WebRtcFrameScheduler::ClearAndGetKeyFrameRequest() {
  base::AutoLock lock(lock_);
  bool key_frame_request = key_frame_request_;
  key_frame_request_ = false;
  return key_frame_request;
}

void WebRtcFrameScheduler::OnCaptureCompleted(webrtc::DesktopFrame* frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Capture overhead "
          << (base::TimeTicks::Now() - last_capture_ticks_).InMilliseconds();
  capture_pending_ = false;

  std::unique_ptr<webrtc::DesktopFrame> owned_frame(frame);

  if (encode_pending_) {
    // TODO(isheriff): consider queuing here
    VLOG(1) << "Dropping captured frame since encoder is still busy";
    return;
  }
  if (!frame || frame->updated_region().is_empty())
    return;

  webrtc::DesktopVector dpi =
      frame->dpi().is_zero() ? webrtc::DesktopVector(kDefaultDpi, kDefaultDpi)
                             : frame->dpi();

  if (!frame_size_.equals(frame->size()) || !frame_dpi_.equals(dpi)) {
    frame_size_ = frame->size();
    frame_dpi_ = dpi;
    if (!size_callback_.is_null())
      size_callback_.Run(frame_size_, frame_dpi_);
  }
  encode_pending_ = true;
  task_tracker_.PostTaskAndReplyWithResult(
      encode_task_runner_.get(), FROM_HERE,
      base::Bind(&WebRtcFrameScheduler::EncodeFrame, encoder_.get(),
                 base::Passed(std::move(owned_frame)),
                 ClearAndGetKeyFrameRequest()),
      base::Bind(&WebRtcFrameScheduler::OnFrameEncoded,
                 weak_factory_.GetWeakPtr()));
}

void WebRtcFrameScheduler::CaptureNextFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (capture_pending_ || encode_pending_) {
    VLOG(1) << "Capture/encode still pending..";
    return;
  }
  capture_pending_ = true;
  VLOG(1) << "Capture next frame after "
          << (base::TimeTicks::Now() - last_capture_ticks_).InMilliseconds();
  last_capture_ticks_ = base::TimeTicks::Now();
  capturer_->Capture(webrtc::DesktopRegion());
}

// static
std::unique_ptr<VideoPacket> WebRtcFrameScheduler::EncodeFrame(
    VideoEncoder* encoder,
    std::unique_ptr<webrtc::DesktopFrame> frame,
    bool key_frame_request) {
  uint32_t flags = 0;
  if (key_frame_request)
    flags |= VideoEncoder::REQUEST_KEY_FRAME;

  base::TimeTicks current = base::TimeTicks::Now();
  std::unique_ptr<VideoPacket> packet = encoder->Encode(*frame, flags);
  if (!packet)
    return nullptr;

  VLOG(1) << "Encode duration "
          << (base::TimeTicks::Now() - current).InMilliseconds()
          << " payload size " << packet->data().size();
  return packet;
}

void WebRtcFrameScheduler::OnFrameEncoded(std::unique_ptr<VideoPacket> packet) {
  DCHECK(thread_checker_.CalledOnValidThread());
  encode_pending_ = false;
  if (!packet)
    return;
  int64_t capture_timestamp_ms =
      (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds();
  base::TimeTicks current = base::TimeTicks::Now();
  // TODO(isheriff): Investigate why first frame fails to send at times.
  // This gets resolved through a PLI request.
  webrtc_transport_->video_encoder_factory()->SendEncodedFrame(
      capture_timestamp_ms, std::move(packet));

  VLOG(1) << "Send duration "
          << (base::TimeTicks::Now() - current).InMilliseconds();
}

}  // namespace protocol
}  // namespace remoting
