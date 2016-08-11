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
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/host_video_stats_dispatcher.h"
#include "remoting/protocol/webrtc_dummy_video_capturer.h"
#include "remoting/protocol/webrtc_transport.h"
#include "third_party/webrtc/api/mediastreaminterface.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/api/test/fakeconstraints.h"
#include "third_party/webrtc/media/base/videocapturer.h"

namespace remoting {
namespace protocol {

namespace {

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

struct WebrtcVideoStream::FrameTimestamps {
  // The following two fields are set only for one frame after each incoming
  // input event. |input_event_client_timestamp| is event timestamp
  // received from the client. |input_event_received_time| is local time when
  // the event was received.
  int64_t input_event_client_timestamp = -1;
  base::TimeTicks input_event_received_time;

  base::TimeTicks capture_started_time;
  base::TimeTicks capture_ended_time;
  base::TimeDelta capture_delay;
  base::TimeTicks encode_started_time;
  base::TimeTicks encode_ended_time;
};

struct WebrtcVideoStream::EncodedFrameWithTimestamps {
  std::unique_ptr<VideoPacket> frame;
  std::unique_ptr<FrameTimestamps> timestamps;
};

WebrtcVideoStream::WebrtcVideoStream()
    : video_stats_dispatcher_(kStreamLabel), weak_factory_(this) {}

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
      base::Bind(&PostTaskOnTaskRunner, base::ThreadTaskRunnerHandle::Get(),
                 base::Bind(&WebrtcVideoStream::SetKeyFrameRequest,
                            weak_factory_.GetWeakPtr())));

  // Register for target bitrate notifications.
  webrtc_transport_->video_encoder_factory()->SetTargetBitrateCallback(
      base::Bind(&PostTaskOnTaskRunnerWithParam<int>,
                 base::ThreadTaskRunnerHandle::Get(),
                 base::Bind(&WebrtcVideoStream::SetTargetBitrate,
                            weak_factory_.GetWeakPtr())));

  video_stats_dispatcher_.Init(webrtc_transport_->CreateOutgoingChannel(
                                   video_stats_dispatcher_.channel_name()),
                               this);
  return true;
}

void WebrtcVideoStream::Pause(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (pause) {
    capture_timer_.Stop();
  } else {
    if (received_first_frame_request_) {
      StartCaptureTimer();
    }
  }
}

void WebrtcVideoStream::OnInputEventReceived(int64_t event_timestamp) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!next_frame_timestamps_)
    next_frame_timestamps_.reset(new FrameTimestamps());
  next_frame_timestamps_->input_event_client_timestamp = event_timestamp;
  next_frame_timestamps_->input_event_received_time = base::TimeTicks::Now();
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&WebrtcVideoStream::StartCaptureTimer,
                              weak_factory_.GetWeakPtr()));
  }
}

void WebrtcVideoStream::StartCaptureTimer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  capture_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1) / 30, this,
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
  DCHECK(capture_pending_);
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

  captured_frame_timestamps_->capture_ended_time = base::TimeTicks::Now();
  captured_frame_timestamps_->capture_delay =
      base::TimeDelta::FromMilliseconds(frame->capture_time_ms());

  encode_pending_ = true;
  base::PostTaskAndReplyWithResult(
      encode_task_runner_.get(), FROM_HERE,
      base::Bind(&WebrtcVideoStream::EncodeFrame, encoder_.get(),
                 base::Passed(std::move(frame)),
                 base::Passed(std::move(captured_frame_timestamps_)),
                 target_bitrate_kbps_, ClearAndGetKeyFrameRequest()),
      base::Bind(&WebrtcVideoStream::OnFrameEncoded,
                 weak_factory_.GetWeakPtr()));
}

void WebrtcVideoStream::OnChannelInitialized(
    ChannelDispatcherBase* channel_dispatcher) {
  DCHECK(&video_stats_dispatcher_ == channel_dispatcher);
}
void WebrtcVideoStream::OnChannelClosed(
    ChannelDispatcherBase* channel_dispatcher) {
  DCHECK(&video_stats_dispatcher_ == channel_dispatcher);
  LOG(WARNING) << "video_stats channel was closed.";
}

void WebrtcVideoStream::CaptureNextFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (capture_pending_ || encode_pending_) {
    VLOG(1) << "Capture/encode still pending..";
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();

  capture_pending_ = true;
  VLOG(1) << "Capture next frame after "
          << (base::TimeTicks::Now() - last_capture_started_ticks_)
                 .InMilliseconds();
  last_capture_started_ticks_ = now;


  // |next_frame_timestamps_| is not set if no input events were received since
  // the previous frame. In that case create FrameTimestamps instance without
  // setting |input_event_client_timestamp| and |input_event_received_time|.
  if (!next_frame_timestamps_)
    next_frame_timestamps_.reset(new FrameTimestamps());

  captured_frame_timestamps_ = std::move(next_frame_timestamps_);
  captured_frame_timestamps_->capture_started_time = now;

  capturer_->Capture(webrtc::DesktopRegion());
}

// static
WebrtcVideoStream::EncodedFrameWithTimestamps WebrtcVideoStream::EncodeFrame(
    VideoEncoder* encoder,
    std::unique_ptr<webrtc::DesktopFrame> frame,
    std::unique_ptr<WebrtcVideoStream::FrameTimestamps> timestamps,
    uint32_t target_bitrate_kbps,
    bool key_frame_request) {
  EncodedFrameWithTimestamps result;
  result.timestamps = std::move(timestamps);
  result.timestamps->encode_started_time = base::TimeTicks::Now();

  encoder->UpdateTargetBitrate(target_bitrate_kbps);
  result.frame = encoder->Encode(
      *frame, key_frame_request ? VideoEncoder::REQUEST_KEY_FRAME : 0);

  result.timestamps->encode_ended_time = base::TimeTicks::Now();

  return result;
}

void WebrtcVideoStream::OnFrameEncoded(EncodedFrameWithTimestamps frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  encode_pending_ = false;

  size_t frame_size = frame.frame ? frame.frame->data().size() : 0;

  // Generate HostFrameStats.
  HostFrameStats stats;
  stats.frame_size = frame_size;

  if (!frame.timestamps->input_event_received_time.is_null()) {
    stats.capture_pending_delay = frame.timestamps->capture_started_time -
                                  frame.timestamps->input_event_received_time;
    stats.latest_event_timestamp = base::TimeTicks::FromInternalValue(
        frame.timestamps->input_event_client_timestamp);
  }

  stats.capture_delay = frame.timestamps->capture_delay;

  // Total overhead time for IPC and threading when capturing frames.
  stats.capture_overhead_delay = (frame.timestamps->capture_ended_time -
                                  frame.timestamps->capture_started_time) -
                                 stats.capture_delay;

  stats.encode_pending_delay = frame.timestamps->encode_started_time -
                               frame.timestamps->capture_ended_time;

  stats.encode_delay = frame.timestamps->encode_ended_time -
                       frame.timestamps->encode_started_time;

  // TODO(sergeyu): Figure out how to measure send_pending time with WebRTC and
  // set it here.
  stats.send_pending_delay = base::TimeDelta();

  uint32_t frame_id = 0;
  if (frame.frame) {
    // Send the frame itself.
    webrtc::EncodedImageCallback::Result result =
        webrtc_transport_->video_encoder_factory()->SendEncodedFrame(
            std::move(frame.frame), frame.timestamps->capture_started_time);
    if (result.error != webrtc::EncodedImageCallback::Result::OK) {
      // TODO(sergeyu): Stop the stream.
      LOG(ERROR) << "Failed to send video frame.";
      return;
    }
    frame_id = result.frame_id;
  }

  // Send FrameStats message.
  if (video_stats_dispatcher_.is_connected())
    video_stats_dispatcher_.OnVideoFrameStats(frame_id, stats);

  // Simplistic adaptation of frame polling in the range 5 FPS to 30 FPS.
  // TODO(sergeyu): Move this logic to a separate class.
  float encoded_bits = frame_size * 8.0;
  uint32_t next_sched_ms = std::max(
      33, std::min(static_cast<int>(encoded_bits / target_bitrate_kbps_), 200));
  capture_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(next_sched_ms), this,
                       &WebrtcVideoStream::CaptureNextFrame);
}

}  // namespace protocol
}  // namespace remoting
