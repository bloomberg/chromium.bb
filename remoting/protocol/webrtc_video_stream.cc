// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_stream.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/webrtc_dummy_video_capturer.h"
#include "remoting/protocol/webrtc_transport.h"
#include "third_party/webrtc/api/mediastreaminterface.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/api/test/fakeconstraints.h"
#include "third_party/webrtc/media/base/videocapturer.h"

namespace remoting {
namespace protocol {

namespace {

// Task running on the encoder thread to encode the |frame|.
std::unique_ptr<VideoPacket> EncodeFrame(
    VideoEncoder* encoder,
    std::unique_ptr<webrtc::DesktopFrame> frame,
    uint32_t target_bitrate_kbps,
    bool key_frame_request,
    int64_t capture_time_ms) {
  uint32_t flags = 0;
  if (key_frame_request)
    flags |= VideoEncoder::REQUEST_KEY_FRAME;

  base::TimeTicks current = base::TimeTicks::Now();
  encoder->UpdateTargetBitrate(target_bitrate_kbps);
  std::unique_ptr<VideoPacket> packet = encoder->Encode(*frame, flags);
  if (!packet)
    return nullptr;
  // TODO(isheriff): Note that while VideoPacket capture time is supposed
  // to be capture duration, we (ab)use it for capture timestamp here. This
  // will go away when we move away from VideoPacket.
  packet->set_capture_time_ms(capture_time_ms);

  VLOG(1) << "Encode duration "
          << (base::TimeTicks::Now() - current).InMilliseconds()
          << " payload size " << packet->data().size();
  return packet;
}

void PostTaskOnTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Closure& task) {
  task_runner->PostTask(FROM_HERE, task);
}

template <typename ParamType>
void PostTaskOnTaskRunnerWithParam(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Callback<void(ParamType param)>& task,
    ParamType param) {
  task_runner->PostTask(FROM_HERE, base::Bind(task, param));
}

}  // namespace

const char kStreamLabel[] = "screen_stream";
const char kVideoLabel[] = "screen_video";

WebrtcVideoStream::WebrtcVideoStream()
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {}

WebrtcVideoStream::~WebrtcVideoStream() {
  if (stream_) {
    for (const auto& track : stream_->GetVideoTracks()) {
      track->GetSource()->Stop();
      stream_->RemoveTrack(track.get());
    }
    peer_connection_->RemoveStream(stream_.get());
  }
  encode_task_runner_->DeleteSoon(FROM_HERE, encoder_.release());
}

bool WebrtcVideoStream::Start(
    std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer,
    WebrtcTransport* webrtc_transport,
    scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
    std::unique_ptr<VideoEncoder> video_encoder) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(webrtc_transport);
  DCHECK(desktop_capturer);
  DCHECK(encode_task_runner);
  DCHECK(video_encoder);

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory(
      webrtc_transport->peer_connection_factory());
  peer_connection_ = webrtc_transport->peer_connection();
  DCHECK(peer_connection_factory);
  DCHECK(peer_connection_);

  encode_task_runner_ = encode_task_runner;
  capturer_ = std::move(desktop_capturer);
  webrtc_transport_ = webrtc_transport;
  encoder_ = std::move(video_encoder);
  capture_timer_.reset(new base::RepeatingTimer());

  capturer_->Start(this);

  // Set video stream constraints.
  webrtc::FakeConstraints video_constraints;
  video_constraints.AddMandatory(
      webrtc::MediaConstraintsInterface::kMinFrameRate, 5);

  rtc::scoped_refptr<webrtc::VideoTrackSourceInterface> src =
      peer_connection_factory->CreateVideoSource(new WebrtcDummyVideoCapturer(),
                                                 &video_constraints);
  rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
      peer_connection_factory->CreateVideoTrack(kVideoLabel, src);

  stream_ = peer_connection_factory->CreateLocalMediaStream(kStreamLabel);

  if (!stream_->AddTrack(video_track.get()) ||
      !peer_connection_->AddStream(stream_.get())) {
    stream_ = nullptr;
    peer_connection_ = nullptr;
    return false;
  }

  // Register for PLI requests.
  webrtc_transport_->video_encoder_factory()->SetKeyFrameRequestCallback(
      base::Bind(&PostTaskOnTaskRunner, main_task_runner_,
                 base::Bind(&WebrtcVideoStream::SetKeyFrameRequest,
                            weak_factory_.GetWeakPtr())));

  // Register for target bitrate notifications.
  webrtc_transport_->video_encoder_factory()->SetTargetBitrateCallback(
      base::Bind(&PostTaskOnTaskRunnerWithParam<int>, main_task_runner_,
                 base::Bind(&WebrtcVideoStream::SetTargetBitrate,
                            weak_factory_.GetWeakPtr())));

  return true;
}

void WebrtcVideoStream::Pause(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (pause) {
    capture_timer_->Stop();
  } else {
    if (received_first_frame_request_) {
      StartCaptureTimer();
    }
  }
}

void WebrtcVideoStream::OnInputEventReceived(int64_t event_timestamp) {
  NOTIMPLEMENTED();
}

void WebrtcVideoStream::SetLosslessEncode(bool want_lossless) {
  NOTIMPLEMENTED();
}

void WebrtcVideoStream::SetLosslessColor(bool want_lossless) {
  NOTIMPLEMENTED();
}

void WebrtcVideoStream::SetObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_ = observer;
}

void WebrtcVideoStream::SetKeyFrameRequest() {
  DCHECK(thread_checker_.CalledOnValidThread());

  key_frame_request_ = true;
  if (!received_first_frame_request_) {
    received_first_frame_request_ = true;
    StartCaptureTimer();
    main_task_runner_->PostTask(
        FROM_HERE, base::Bind(&WebrtcVideoStream::StartCaptureTimer,
                              weak_factory_.GetWeakPtr()));
  }
}

void WebrtcVideoStream::StartCaptureTimer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  capture_timer_->Start(FROM_HERE, base::TimeDelta::FromSeconds(1) / 30, this,
                        &WebrtcVideoStream::CaptureNextFrame);
}

void WebrtcVideoStream::SetTargetBitrate(int target_bitrate_kbps) {
  DCHECK(thread_checker_.CalledOnValidThread());

  VLOG(1) << "Set Target bitrate " << target_bitrate_kbps;
  target_bitrate_kbps_ = target_bitrate_kbps;
}

bool WebrtcVideoStream::ClearAndGetKeyFrameRequest() {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool key_frame_request = key_frame_request_;
  key_frame_request_ = false;
  return key_frame_request;
}

void WebrtcVideoStream::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::TimeTicks captured_ticks = base::TimeTicks::Now();
  int64_t capture_timestamp_ms =
      (captured_ticks - base::TimeTicks()).InMilliseconds();
  capture_pending_ = false;

  if (encode_pending_) {
    // TODO(isheriff): consider queuing here
    VLOG(1) << "Dropping captured frame since encoder is still busy";
    return;
  }

  // TODO(sergeyu): Handle ERROR_PERMANENT result here.

  webrtc::DesktopVector dpi =
      frame->dpi().is_zero() ? webrtc::DesktopVector(kDefaultDpi, kDefaultDpi)
                             : frame->dpi();

  if (!frame_size_.equals(frame->size()) || !frame_dpi_.equals(dpi)) {
    frame_size_ = frame->size();
    frame_dpi_ = dpi;
    if (observer_)
      observer_->OnVideoSizeChanged(this, frame_size_, frame_dpi_);
  }
  encode_pending_ = true;
  base::PostTaskAndReplyWithResult(
      encode_task_runner_.get(), FROM_HERE,
      base::Bind(&EncodeFrame, encoder_.get(), base::Passed(std::move(frame)),
                 target_bitrate_kbps_, ClearAndGetKeyFrameRequest(),
                 capture_timestamp_ms),
      base::Bind(&WebrtcVideoStream::OnFrameEncoded,
                 weak_factory_.GetWeakPtr()));
}

void WebrtcVideoStream::CaptureNextFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (capture_pending_ || encode_pending_) {
    VLOG(1) << "Capture/encode still pending..";
    return;
  }

  capture_pending_ = true;
  VLOG(1) << "Capture next frame after "
          << (base::TimeTicks::Now() - last_capture_started_ticks_)
                 .InMilliseconds();
  last_capture_started_ticks_ = base::TimeTicks::Now();
  capturer_->Capture(webrtc::DesktopRegion());
}

void WebrtcVideoStream::OnFrameEncoded(std::unique_ptr<VideoPacket> packet) {
  DCHECK(thread_checker_.CalledOnValidThread());

  encode_pending_ = false;
  if (!packet)
    return;
  base::TimeTicks current = base::TimeTicks::Now();
  float encoded_bits = packet->data().size() * 8.0;

  // Simplistic adaptation of frame polling in the range 5 FPS to 30 FPS.
  uint32_t next_sched_ms = std::max(
      33, std::min(static_cast<int>(encoded_bits / target_bitrate_kbps_), 200));
  if (webrtc_transport_->video_encoder_factory()->SendEncodedFrame(
          std::move(packet)) >= 0) {
    VLOG(1) << "Send duration "
            << (base::TimeTicks::Now() - current).InMilliseconds()
            << ", next sched " << next_sched_ms;
  } else {
    LOG(ERROR) << "SendEncodedFrame() failed";
  }
  capture_timer_->Start(FROM_HERE,
                        base::TimeDelta::FromMilliseconds(next_sched_ms), this,
                        &WebrtcVideoStream::CaptureNextFrame);
}

}  // namespace protocol
}  // namespace remoting
