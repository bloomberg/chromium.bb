// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/video_frame_pump.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/task_runner_util.h"
#include "base/time/time.h"
#include "remoting/host/capture_scheduler.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/cursor_shape_stub.h"
#include "remoting/protocol/video_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

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
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    scoped_ptr<webrtc::DesktopCapturer> capturer,
    scoped_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor,
    scoped_ptr<VideoEncoder> encoder,
    protocol::CursorShapeStub* cursor_stub,
    protocol::VideoStub* video_stub)
    : capture_task_runner_(capture_task_runner),
      encode_task_runner_(encode_task_runner),
      network_task_runner_(network_task_runner),
      capturer_(capturer.Pass()),
      mouse_cursor_monitor_(mouse_cursor_monitor.Pass()),
      encoder_(encoder.Pass()),
      cursor_stub_(cursor_stub),
      video_stub_(video_stub),
      latest_event_timestamp_(0) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(capturer_);
  DCHECK(mouse_cursor_monitor_);
  DCHECK(encoder_);
  DCHECK(cursor_stub_);
  DCHECK(video_stub_);
}

// Public methods --------------------------------------------------------------

void VideoFramePump::Start() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  keep_alive_timer_.reset(new base::DelayTimer<VideoFramePump>(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kKeepAlivePacketIntervalMs),
      this, &VideoFramePump::SendKeepAlivePacket));

  capture_scheduler_.reset(new CaptureScheduler(
      base::Bind(&VideoFramePump::CaptureNextFrame, this)));
  capture_scheduler_->Start();

  capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoFramePump::StartOnCaptureThread, this));
}

void VideoFramePump::Stop() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  // Clear stubs to prevent further updates reaching the client.
  cursor_stub_ = nullptr;
  video_stub_ = nullptr;

  capture_scheduler_.reset();
  keep_alive_timer_.reset();

  capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoFramePump::StopOnCaptureThread, this));
}

void VideoFramePump::Pause(bool pause) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  capture_scheduler_->Pause(pause);
}

void VideoFramePump::SetLatestEventTimestamp(int64 latest_event_timestamp) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  latest_event_timestamp_ = latest_event_timestamp;
}

void VideoFramePump::SetLosslessEncode(bool want_lossless) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  encode_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoEncoder::SetLosslessEncode,
                            base::Unretained(encoder_.get()), want_lossless));
}

void VideoFramePump::SetLosslessColor(bool want_lossless) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  encode_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoEncoder::SetLosslessColor,
                            base::Unretained(encoder_.get()), want_lossless));
}

// Private methods -----------------------------------------------------------

VideoFramePump::~VideoFramePump() {
  // Destroy the capturer and encoder on their respective threads.
  capture_task_runner_->DeleteSoon(FROM_HERE, capturer_.release());
  capture_task_runner_->DeleteSoon(FROM_HERE, mouse_cursor_monitor_.release());
  encode_task_runner_->DeleteSoon(FROM_HERE, encoder_.release());
}

// Capturer thread -------------------------------------------------------------

webrtc::SharedMemory* VideoFramePump::CreateSharedMemory(size_t size) {
  return nullptr;
}

void VideoFramePump::OnCaptureCompleted(webrtc::DesktopFrame* frame) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoFramePump::EncodeAndSendFrame, this,
                            base::Passed(make_scoped_ptr(frame))));
}

void VideoFramePump::OnMouseCursor(webrtc::MouseCursor* cursor) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  scoped_ptr<webrtc::MouseCursor> owned_cursor(cursor);

  scoped_ptr<protocol::CursorShapeInfo> cursor_proto(
      new protocol::CursorShapeInfo());
  cursor_proto->set_width(cursor->image()->size().width());
  cursor_proto->set_height(cursor->image()->size().height());
  cursor_proto->set_hotspot_x(cursor->hotspot().x());
  cursor_proto->set_hotspot_y(cursor->hotspot().y());

  cursor_proto->set_data(std::string());
  uint8_t* current_row = cursor->image()->data();
  for (int y = 0; y < cursor->image()->size().height(); ++y) {
    cursor_proto->mutable_data()->append(
        current_row,
        current_row + cursor->image()->size().width() *
            webrtc::DesktopFrame::kBytesPerPixel);
    current_row += cursor->image()->stride();
  }

  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoFramePump::SendCursorShape, this,
                            base::Passed(&cursor_proto)));
}

void VideoFramePump::OnMouseCursorPosition(
    webrtc::MouseCursorMonitor::CursorState state,
    const webrtc::DesktopVector& position) {
  // We're not subscribing to mouse position changes.
  NOTREACHED();
}

void VideoFramePump::StartOnCaptureThread() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  mouse_cursor_monitor_->Init(this, webrtc::MouseCursorMonitor::SHAPE_ONLY);
  capturer_->Start(this);
}

void VideoFramePump::StopOnCaptureThread() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  // This doesn't deleted already captured frames, so encoder can keep using the
  // frames that were captured previously.
  capturer_.reset();

  mouse_cursor_monitor_.reset();
}

void VideoFramePump::CaptureNextFrameOnCaptureThread() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  // Capture mouse shape first and then screen content.
  mouse_cursor_monitor_->Capture();
  capturer_->Capture(webrtc::DesktopRegion());
}

// Network thread --------------------------------------------------------------

void VideoFramePump::CaptureNextFrame() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  capture_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoFramePump::CaptureNextFrameOnCaptureThread, this));
}

void VideoFramePump::EncodeAndSendFrame(
    scoped_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!video_stub_)
    return;

  capture_scheduler_->OnCaptureCompleted();

  // Even when |frame| is nullptr we still need to post it to the encode thread
  // to make sure frames are freed in the same order they are received and
  // that we don't start capturing frame n+2 before frame n is freed.
  base::PostTaskAndReplyWithResult(
      encode_task_runner_.get(), FROM_HERE,
      base::Bind(&EncodeFrame, encoder_.get(), base::Passed(&frame)),
      base::Bind(&VideoFramePump::SendEncodedFrame, this,
                 latest_event_timestamp_, base::TimeTicks::Now()));
}

void VideoFramePump::SendEncodedFrame(int64 latest_event_timestamp,
                                      base::TimeTicks timestamp,
                                      scoped_ptr<VideoPacket> packet) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!video_stub_)
    return;

  if (g_enable_timestamps)
    packet->set_timestamp(timestamp.ToInternalValue());

  packet->set_latest_event_timestamp(latest_event_timestamp);

  capture_scheduler_->OnFrameEncoded(
      base::TimeDelta::FromMilliseconds(packet->encode_time_ms()));

  video_stub_->ProcessVideoPacket(
      packet.Pass(), base::Bind(&VideoFramePump::OnVideoPacketSent, this));
}

void VideoFramePump::OnVideoPacketSent() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!video_stub_)
    return;

  capture_scheduler_->OnFrameSent();
  keep_alive_timer_->Reset();
}

void VideoFramePump::SendKeepAlivePacket() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  video_stub_->ProcessVideoPacket(
      make_scoped_ptr(new VideoPacket()),
      base::Bind(&VideoFramePump::OnKeepAlivePacketSent, this));
}

void VideoFramePump::OnKeepAlivePacketSent() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (keep_alive_timer_)
    keep_alive_timer_->Reset();
}

void VideoFramePump::SendCursorShape(
    scoped_ptr<protocol::CursorShapeInfo> cursor_shape) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!cursor_stub_)
    return;

  cursor_stub_->SetCursorShape(*cursor_shape);
}

}  // namespace remoting
