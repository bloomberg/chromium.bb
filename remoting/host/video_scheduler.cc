// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/video_scheduler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "base/time.h"
#include "remoting/base/capture_data.h"
#include "remoting/host/video_frame_capturer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/message_decoder.h"
#include "remoting/protocol/video_stub.h"
#include "remoting/protocol/util.h"

namespace remoting {

// Maximum number of frames that can be processed simultaneously.
// TODO(hclam): Move this value to CaptureScheduler.
static const int kMaxPendingCaptures = 2;

VideoScheduler::VideoScheduler(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    VideoFrameCapturer* capturer,
    scoped_ptr<VideoEncoder> encoder,
    protocol::ClientStub* client_stub,
    protocol::VideoStub* video_stub)
    : capture_task_runner_(capture_task_runner),
      encode_task_runner_(encode_task_runner),
      network_task_runner_(network_task_runner),
      capturer_(capturer),
      encoder_(encoder.Pass()),
      cursor_stub_(client_stub),
      video_stub_(video_stub),
      pending_captures_(0),
      did_skip_frame_(false),
      is_paused_(false),
      sequence_number_(0) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(capturer_);
  DCHECK(cursor_stub_);
  DCHECK(video_stub_);

  capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoScheduler::StartOnCaptureThread, this));
}

// Public methods --------------------------------------------------------------

void VideoScheduler::OnCaptureCompleted(
    scoped_refptr<CaptureData> capture_data) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (capture_data) {
    scheduler_.RecordCaptureTime(
        base::TimeDelta::FromMilliseconds(capture_data->capture_time_ms()));

    // The best way to get this value is by binding the sequence number to
    // the callback when calling CaptureInvalidRects(). However the callback
    // system doesn't allow this. Reading from the member variable is
    // accurate as long as capture is synchronous as the following statement
    // will obtain the most recent sequence number received.
    capture_data->set_client_sequence_number(sequence_number_);
  }

  encode_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoScheduler::EncodeFrame, this, capture_data));
}

void VideoScheduler::OnCursorShapeChanged(
    scoped_ptr<protocol::CursorShapeInfo> cursor_shape) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoScheduler::SendCursorShape, this,
                            base::Passed(&cursor_shape)));
}

void VideoScheduler::Stop(const base::Closure& done_task) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  DCHECK(!done_task.is_null());

  // Clear stubs to prevent further updates reaching the client.
  cursor_stub_ = NULL;
  video_stub_ = NULL;

  capture_task_runner_->PostTask(FROM_HERE,
      base::Bind(&VideoScheduler::StopOnCaptureThread, this, done_task));
}

void VideoScheduler::Pause(bool pause) {
  if (!capture_task_runner_->BelongsToCurrentThread()) {
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    capture_task_runner_->PostTask(
        FROM_HERE, base::Bind(&VideoScheduler::Pause, this, pause));
    return;
  }

  if (is_paused_ != pause) {
    is_paused_ = pause;

    // Restart captures if we're resuming and there are none scheduled.
    if (!is_paused_ && !capture_timer_->IsRunning())
      CaptureNextFrame();
  }
}

void VideoScheduler::UpdateSequenceNumber(int64 sequence_number) {
  if (!capture_task_runner_->BelongsToCurrentThread()) {
    DCHECK(network_task_runner_->BelongsToCurrentThread());
    capture_task_runner_->PostTask(
        FROM_HERE, base::Bind(&VideoScheduler::UpdateSequenceNumber,
                              this, sequence_number));
    return;
  }

  sequence_number_ = sequence_number;
}

// Private methods -----------------------------------------------------------

VideoScheduler::~VideoScheduler() {
}

// Capturer thread -------------------------------------------------------------

void VideoScheduler::StartOnCaptureThread() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  // Start the capturer and let it notify us of cursor shape changes.
  capturer_->Start(this);

  capture_timer_.reset(new base::OneShotTimer<VideoScheduler>());

  // Capture first frame immedately.
  CaptureNextFrame();
}

void VideoScheduler::StopOnCaptureThread(const base::Closure& done_task) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  // Stop |capturer_| and clear it to prevent pending tasks from using it.
  capturer_->Stop();
  capturer_ = NULL;

  // |capture_timer_| must be destroyed on the thread on which it is used.
  capture_timer_.reset();

  // Activity on the encode thread will stop implicitly as a result of
  // captures having stopped.
  network_task_runner_->PostTask(FROM_HERE, done_task);
}

void VideoScheduler::ScheduleNextCapture() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  capture_timer_->Start(FROM_HERE,
                        scheduler_.NextCaptureDelay(),
                        this,
                        &VideoScheduler::CaptureNextFrame);
}

void VideoScheduler::CaptureNextFrame() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  // If we are stopping (|capturer_| is NULL), or paused, then don't capture.
  if (!capturer_ || is_paused_)
    return;

  // Make sure we have at most two oustanding recordings. We can simply return
  // if we can't make a capture now, the next capture will be started by the
  // end of an encode operation.
  if (pending_captures_ >= kMaxPendingCaptures) {
    did_skip_frame_ = true;
    return;
  }

  did_skip_frame_ = false;

  // At this point we are going to perform one capture so save the current time.
  pending_captures_++;
  DCHECK_LE(pending_captures_, kMaxPendingCaptures);

  // Before doing a capture schedule for the next one.
  ScheduleNextCapture();

  // And finally perform one capture.
  capturer_->CaptureFrame();
}

void VideoScheduler::FrameCaptureCompleted() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  // Decrement the pending capture count.
  pending_captures_--;
  DCHECK_GE(pending_captures_, 0);

  // If we've skipped a frame capture because too we had too many captures
  // pending then schedule one now.
  if (did_skip_frame_)
    CaptureNextFrame();
}

// Network thread --------------------------------------------------------------

void VideoScheduler::SendVideoPacket(scoped_ptr<VideoPacket> packet) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!video_stub_)
    return;

  base::Closure callback;
  if ((packet->flags() & VideoPacket::LAST_PARTITION) != 0)
    callback = base::Bind(&VideoScheduler::VideoFrameSentCallback, this);

  video_stub_->ProcessVideoPacket(packet.Pass(), callback);
}

void VideoScheduler::VideoFrameSentCallback() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!video_stub_)
    return;

  capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoScheduler::FrameCaptureCompleted, this));
}

void VideoScheduler::SendCursorShape(
    scoped_ptr<protocol::CursorShapeInfo> cursor_shape) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!cursor_stub_)
    return;

  cursor_stub_->SetCursorShape(*cursor_shape);
}

// Encoder thread --------------------------------------------------------------

void VideoScheduler::EncodeFrame(
    scoped_refptr<CaptureData> capture_data) {
  DCHECK(encode_task_runner_->BelongsToCurrentThread());

  // If there is nothing to encode then send an empty keep-alive packet.
  if (!capture_data || capture_data->dirty_region().isEmpty()) {
    scoped_ptr<VideoPacket> packet(new VideoPacket());
    packet->set_flags(VideoPacket::LAST_PARTITION);
    network_task_runner_->PostTask(
        FROM_HERE, base::Bind(&VideoScheduler::SendVideoPacket, this,
                              base::Passed(&packet)));
    return;
  }

  encoder_->Encode(
      capture_data, false,
      base::Bind(&VideoScheduler::EncodedDataAvailableCallback, this));
}

void VideoScheduler::EncodedDataAvailableCallback(
    scoped_ptr<VideoPacket> packet) {
  DCHECK(encode_task_runner_->BelongsToCurrentThread());

  bool last = (packet->flags() & VideoPacket::LAST_PACKET) != 0;
  if (last) {
    scheduler_.RecordEncodeTime(
        base::TimeDelta::FromMilliseconds(packet->encode_time_ms()));
  }

  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoScheduler::SendVideoPacket, this,
                            base::Passed(&packet)));
}

}  // namespace remoting
