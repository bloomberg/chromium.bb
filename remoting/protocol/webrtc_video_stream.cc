// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_video_stream.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/base/constants.h"
#include "remoting/codec/webrtc_video_encoder_vpx.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/host_video_stats_dispatcher.h"
#include "remoting/protocol/webrtc_dummy_video_capturer.h"
#include "remoting/protocol/webrtc_frame_scheduler_simple.h"
#include "remoting/protocol/webrtc_transport.h"
#include "third_party/webrtc/api/mediastreaminterface.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/api/test/fakeconstraints.h"
#include "third_party/webrtc/media/base/videocapturer.h"

namespace remoting {
namespace protocol {

const char kStreamLabel[] = "screen_stream";
const char kVideoLabel[] = "screen_video";

struct WebrtcVideoStream::FrameStats {
  // The following fields is not null only for one frame after each incoming
  // input event.
  InputEventTimestamps input_event_timestamps;

  base::TimeTicks capture_started_time;
  base::TimeTicks capture_ended_time;
  base::TimeDelta capture_delay;
  base::TimeTicks encode_started_time;
  base::TimeTicks encode_ended_time;

  uint32_t capturer_id = 0;
};

struct WebrtcVideoStream::EncodedFrameWithStats {
  std::unique_ptr<WebrtcVideoEncoder::EncodedFrame> frame;
  std::unique_ptr<FrameStats> stats;
};

WebrtcVideoStream::WebrtcVideoStream()
    : video_stats_dispatcher_(kStreamLabel), weak_factory_(this) {}

WebrtcVideoStream::~WebrtcVideoStream() {
  if (stream_) {
    for (const auto& track : stream_->GetVideoTracks()) {
      stream_->RemoveTrack(track.get());
    }
    peer_connection_->RemoveStream(stream_.get());
  }
  encode_task_runner_->DeleteSoon(FROM_HERE, encoder_.release());
}

void WebrtcVideoStream::Start(
    std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer,
    WebrtcTransport* webrtc_transport,
    scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(webrtc_transport);
  DCHECK(desktop_capturer);
  DCHECK(encode_task_runner);

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory(
      webrtc_transport->peer_connection_factory());
  peer_connection_ = webrtc_transport->peer_connection();
  DCHECK(peer_connection_factory);
  DCHECK(peer_connection_);

  encode_task_runner_ = std::move(encode_task_runner);
  capturer_ = std::move(desktop_capturer);
  webrtc_transport_ = webrtc_transport;
  // TODO(isheriff): make this codec independent
  encoder_ = WebrtcVideoEncoderVpx::CreateForVP8();
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

  // AddTrack() may fail only if there is another track with the same name,
  // which is impossible because it's a brand new stream.
  bool result = stream_->AddTrack(video_track.get());
  DCHECK(result);

  // AddStream() may fail if there is another stream with the same name or when
  // the PeerConnection is closed, neither is expected.
  result = peer_connection_->AddStream(stream_.get());
  DCHECK(result);

  scheduler_.reset(new WebrtcFrameSchedulerSimple());
  scheduler_->Start(
      webrtc_transport_->video_encoder_factory(),
      base::Bind(&WebrtcVideoStream::CaptureNextFrame, base::Unretained(this)));

  video_stats_dispatcher_.Init(webrtc_transport_->CreateOutgoingChannel(
                                   video_stats_dispatcher_.channel_name()),
                               this);
}

void WebrtcVideoStream::SetEventTimestampsSource(
    scoped_refptr<InputEventTimestampsSource> event_timestamps_source) {
  event_timestamps_source_ = event_timestamps_source;
}

void WebrtcVideoStream::Pause(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scheduler_->Pause(pause);
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

void WebrtcVideoStream::OnCaptureResult(
    webrtc::DesktopCapturer::Result result,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  captured_frame_stats_->capture_ended_time = base::TimeTicks::Now();
  captured_frame_stats_->capture_delay =
      base::TimeDelta::FromMilliseconds(frame ? frame->capture_time_ms() : 0);

  WebrtcVideoEncoder::FrameParams frame_params;
  if (!scheduler_->OnFrameCaptured(frame.get(), &frame_params)) {
    return;
  }

  // TODO(sergeyu): Handle ERROR_PERMANENT result here.
  if (frame) {
    webrtc::DesktopVector dpi =
        frame->dpi().is_zero() ? webrtc::DesktopVector(kDefaultDpi, kDefaultDpi)
                               : frame->dpi();

    if (!frame_size_.equals(frame->size()) || !frame_dpi_.equals(dpi)) {
      frame_size_ = frame->size();
      frame_dpi_ = dpi;
      if (observer_)
        observer_->OnVideoSizeChanged(this, frame_size_, frame_dpi_);
    }
  }

  base::PostTaskAndReplyWithResult(
      encode_task_runner_.get(), FROM_HERE,
      base::Bind(&WebrtcVideoStream::EncodeFrame, encoder_.get(),
                 base::Passed(std::move(frame)), frame_params,
                 base::Passed(std::move(captured_frame_stats_))),
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

  captured_frame_stats_.reset(new FrameStats());
  captured_frame_stats_->capture_started_time = base::TimeTicks::Now();
  captured_frame_stats_->input_event_timestamps =
      event_timestamps_source_->TakeLastEventTimestamps();

  capturer_->CaptureFrame();
}

// static
WebrtcVideoStream::EncodedFrameWithStats WebrtcVideoStream::EncodeFrame(
    WebrtcVideoEncoder* encoder,
    std::unique_ptr<webrtc::DesktopFrame> frame,
    WebrtcVideoEncoder::FrameParams params,
    std::unique_ptr<WebrtcVideoStream::FrameStats> stats) {
  EncodedFrameWithStats result;
  result.stats = std::move(stats);
  result.stats->encode_started_time = base::TimeTicks::Now();
  result.frame = encoder->Encode(frame.get(), params);
  result.stats->encode_ended_time = base::TimeTicks::Now();
  if (frame) {
    result.stats->capturer_id = frame->capturer_id();
  }
  return result;
}

void WebrtcVideoStream::OnFrameEncoded(EncodedFrameWithStats frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  HostFrameStats stats;
  scheduler_->OnFrameEncoded(frame.frame.get(), &stats);

  if (!frame.frame) {
    return;
  }

  webrtc::EncodedImageCallback::Result result =
      webrtc_transport_->video_encoder_factory()->SendEncodedFrame(
          *frame.frame, frame.stats->capture_started_time);
  if (result.error != webrtc::EncodedImageCallback::Result::OK) {
    // TODO(sergeyu): Stop the stream.
    LOG(ERROR) << "Failed to send video frame.";
    return;
  }

  // Send FrameStats message.
  if (video_stats_dispatcher_.is_connected()) {
    stats.frame_size = frame.frame ? frame.frame->data.size() : 0;

    if (!frame.stats->input_event_timestamps.is_null()) {
      stats.capture_pending_delay =
          frame.stats->capture_started_time -
          frame.stats->input_event_timestamps.host_timestamp;
      stats.latest_event_timestamp =
          frame.stats->input_event_timestamps.client_timestamp;
    }

    stats.capture_delay = frame.stats->capture_delay;

    // Total overhead time for IPC and threading when capturing frames.
    stats.capture_overhead_delay =
        (frame.stats->capture_ended_time - frame.stats->capture_started_time) -
        stats.capture_delay;

    stats.encode_pending_delay =
        frame.stats->encode_started_time - frame.stats->capture_ended_time;

    stats.encode_delay =
        frame.stats->encode_ended_time - frame.stats->encode_started_time;

    stats.capturer_id = frame.stats->capturer_id;

    video_stats_dispatcher_.OnVideoFrameStats(result.frame_id, stats);
  }
}

}  // namespace protocol
}  // namespace remoting
