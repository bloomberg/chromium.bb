// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/video_frame_pump.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/video_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

namespace {

// Helper used to encode frames on the encode thread.
//
// TODO(sergeyu): This functions doesn't do much beside calling
// VideoEncoder::Encode(). It's only needed to handle empty frames properly and
// that logic can be moved to VideoEncoder implementations.
scoped_ptr<VideoPacket> EncodeFrame(VideoEncoder* encoder,
                                    scoped_ptr<webrtc::DesktopFrame> frame) {
  // If there is nothing to encode then send an empty packet.
  if (!frame || frame->updated_region().is_empty())
    return make_scoped_ptr(new VideoPacket());

  return encoder->Encode(*frame);
}

}  // namespace

// Interval between empty keep-alive frames. These frames are sent only when the
// stream is paused or inactive for some other reason (e.g. when blocked on
// capturer). To prevent PseudoTCP from resetting congestion window this value
// must be smaller than the minimum RTO used in PseudoTCP, which is 250ms.
static const int kKeepAlivePacketIntervalMs = 200;

static bool g_enable_timestamps = false;

// static
void VideoFramePump::EnableTimestampsForTests() {
  g_enable_timestamps = true;
}

VideoFramePump::VideoFramePump(
    scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
    scoped_ptr<webrtc::DesktopCapturer> capturer,
    scoped_ptr<VideoEncoder> encoder,
    protocol::VideoStub* video_stub)
    : encode_task_runner_(encode_task_runner),
      capturer_(capturer.Pass()),
      encoder_(encoder.Pass()),
      video_stub_(video_stub),
      keep_alive_timer_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kKeepAlivePacketIntervalMs),
          base::Bind(&VideoFramePump::SendKeepAlivePacket,
                     base::Unretained(this)),
          false),
      capture_scheduler_(base::Bind(&VideoFramePump::CaptureNextFrame,
                                    base::Unretained(this))),
      latest_event_timestamp_(0),
      weak_factory_(this) {
  DCHECK(encoder_);
  DCHECK(video_stub_);

  capturer_->Start(this);
  capture_scheduler_.Start();
}

VideoFramePump::~VideoFramePump() {
  encode_task_runner_->DeleteSoon(FROM_HERE, encoder_.release());
}

void VideoFramePump::Pause(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());

  capture_scheduler_.Pause(pause);
}

void VideoFramePump::SetLatestEventTimestamp(int64 latest_event_timestamp) {
  DCHECK(thread_checker_.CalledOnValidThread());

  latest_event_timestamp_ = latest_event_timestamp;
}

void VideoFramePump::SetLosslessEncode(bool want_lossless) {
  DCHECK(thread_checker_.CalledOnValidThread());

  encode_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoEncoder::SetLosslessEncode,
                            base::Unretained(encoder_.get()), want_lossless));
}

void VideoFramePump::SetLosslessColor(bool want_lossless) {
  DCHECK(thread_checker_.CalledOnValidThread());

  encode_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoEncoder::SetLosslessColor,
                            base::Unretained(encoder_.get()), want_lossless));
}

webrtc::SharedMemory* VideoFramePump::CreateSharedMemory(size_t size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return nullptr;
}

void VideoFramePump::OnCaptureCompleted(webrtc::DesktopFrame* frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  capture_scheduler_.OnCaptureCompleted();

  // Even when |frame| is nullptr we still need to post it to the encode thread
  // to make sure frames are freed in the same order they are received and
  // that we don't start capturing frame n+2 before frame n is freed.
  base::PostTaskAndReplyWithResult(
      encode_task_runner_.get(), FROM_HERE,
      base::Bind(&EncodeFrame, encoder_.get(),
                 base::Passed(make_scoped_ptr(frame))),
      base::Bind(&VideoFramePump::SendEncodedFrame, weak_factory_.GetWeakPtr(),
                 latest_event_timestamp_, base::TimeTicks::Now()));
}

void VideoFramePump::CaptureNextFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());

  capturer_->Capture(webrtc::DesktopRegion());
}

void VideoFramePump::SendEncodedFrame(int64 latest_event_timestamp,
                                      base::TimeTicks timestamp,
                                      scoped_ptr<VideoPacket> packet) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (g_enable_timestamps)
    packet->set_timestamp(timestamp.ToInternalValue());

  packet->set_latest_event_timestamp(latest_event_timestamp);

  capture_scheduler_.OnFrameEncoded(
      base::TimeDelta::FromMilliseconds(packet->encode_time_ms()));

  video_stub_->ProcessVideoPacket(packet.Pass(),
                                  base::Bind(&VideoFramePump::OnVideoPacketSent,
                                             weak_factory_.GetWeakPtr()));
}

void VideoFramePump::OnVideoPacketSent() {
  DCHECK(thread_checker_.CalledOnValidThread());

  capture_scheduler_.OnFrameSent();
  keep_alive_timer_.Reset();
}

void VideoFramePump::SendKeepAlivePacket() {
  DCHECK(thread_checker_.CalledOnValidThread());

  video_stub_->ProcessVideoPacket(
      make_scoped_ptr(new VideoPacket()),
      base::Bind(&VideoFramePump::OnKeepAlivePacketSent,
                 weak_factory_.GetWeakPtr()));
}

void VideoFramePump::OnKeepAlivePacketSent() {
  DCHECK(thread_checker_.CalledOnValidThread());

  keep_alive_timer_.Reset();
}

}  // namespace remoting
