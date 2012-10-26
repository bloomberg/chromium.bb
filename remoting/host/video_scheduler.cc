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
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/message_decoder.h"
#include "remoting/protocol/util.h"

using remoting::protocol::ConnectionToClient;

namespace remoting {

// Maximum number of frames that can be processed similtaneously.
// TODO(hclam): Move this value to CaptureScheduler.
static const int kMaxRecordings = 2;

VideoScheduler::VideoScheduler(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    VideoFrameCapturer* capturer,
    scoped_ptr<VideoEncoder> encoder)
    : capture_task_runner_(capture_task_runner),
      encode_task_runner_(encode_task_runner),
      network_task_runner_(network_task_runner),
      capturer_(capturer),
      encoder_(encoder.Pass()),
      network_stopped_(false),
      encoder_stopped_(false),
      max_recordings_(kMaxRecordings),
      recordings_(0),
      frame_skipped_(false),
      sequence_number_(0) {
  DCHECK(capture_task_runner_);
  DCHECK(encode_task_runner_);
  DCHECK(network_task_runner_);
}

// Public methods --------------------------------------------------------------

void VideoScheduler::Start() {
  capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoScheduler::DoStart, this));
}

void VideoScheduler::Stop(const base::Closure& done_task) {
  if (!capture_task_runner_->BelongsToCurrentThread()) {
    capture_task_runner_->PostTask(FROM_HERE, base::Bind(
        &VideoScheduler::Stop, this, done_task));
    return;
  }

  DCHECK(!done_task.is_null());

  capturer()->Stop();
  capture_timer_.reset();

  network_task_runner_->PostTask(FROM_HERE, base::Bind(
      &VideoScheduler::DoStopOnNetworkThread, this, done_task));
}

void VideoScheduler::AddConnection(ConnectionToClient* connection) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  connections_.push_back(connection);

  capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoScheduler::DoInvalidateFullScreen, this));
}

void VideoScheduler::RemoveConnection(ConnectionToClient* connection) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  ConnectionToClientList::iterator it =
      std::find(connections_.begin(), connections_.end(), connection);
  if (it != connections_.end()) {
    connections_.erase(it);
  }
}

void VideoScheduler::RemoveAllConnections() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  connections_.clear();
}

void VideoScheduler::UpdateSequenceNumber(int64 sequence_number) {
  // Sequence number is used and written only on the capture thread.
  if (!capture_task_runner_->BelongsToCurrentThread()) {
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

VideoFrameCapturer* VideoScheduler::capturer() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  DCHECK(capturer_);
  return capturer_;
}

VideoEncoder* VideoScheduler::encoder() {
  DCHECK(encode_task_runner_->BelongsToCurrentThread());
  DCHECK(encoder_.get());
  return encoder_.get();
}

bool VideoScheduler::is_recording() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  return capture_timer_.get() != NULL;
}

// Capturer thread -------------------------------------------------------------

void VideoScheduler::DoStart() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (is_recording()) {
    NOTREACHED() << "Record session already started.";
    return;
  }

  capturer()->Start(
      base::Bind(&VideoScheduler::CursorShapeChangedCallback, this));

  capture_timer_.reset(new base::OneShotTimer<VideoScheduler>());

  // Capture first frame immedately.
  DoCapture();
}

void VideoScheduler::StartCaptureTimer() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  capture_timer_->Start(FROM_HERE,
                        scheduler_.NextCaptureDelay(),
                        this,
                        &VideoScheduler::DoCapture);
}

void VideoScheduler::DoCapture() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());
  // Make sure we have at most two oustanding recordings. We can simply return
  // if we can't make a capture now, the next capture will be started by the
  // end of an encode operation.
  if (recordings_ >= max_recordings_ || !is_recording()) {
    frame_skipped_ = true;
    return;
  }

  if (frame_skipped_)
    frame_skipped_ = false;

  // At this point we are going to perform one capture so save the current time.
  ++recordings_;
  DCHECK_LE(recordings_, max_recordings_);

  // Before doing a capture schedule for the next one.
  capture_timer_->Stop();
  capture_timer_->Start(FROM_HERE,
                        scheduler_.NextCaptureDelay(),
                        this,
                        &VideoScheduler::DoCapture);

  // And finally perform one capture.
  capture_start_time_ = base::Time::Now();
  capturer()->CaptureInvalidRegion(
      base::Bind(&VideoScheduler::CaptureDoneCallback, this));
}

void VideoScheduler::CaptureDoneCallback(
    scoped_refptr<CaptureData> capture_data) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (!is_recording())
    return;

  if (capture_data) {
    base::TimeDelta capture_time = base::Time::Now() - capture_start_time_;
    int capture_time_ms =
        static_cast<int>(capture_time.InMilliseconds());
    capture_data->set_capture_time_ms(capture_time_ms);
    scheduler_.RecordCaptureTime(capture_time);

    // The best way to get this value is by binding the sequence number to
    // the callback when calling CaptureInvalidRects(). However the callback
    // system doesn't allow this. Reading from the member variable is
    // accurate as long as capture is synchronous as the following statement
    // will obtain the most recent sequence number received.
    capture_data->set_client_sequence_number(sequence_number_);
  }

  encode_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoScheduler::DoEncode, this, capture_data));
}

void VideoScheduler::CursorShapeChangedCallback(
    scoped_ptr<protocol::CursorShapeInfo> cursor_shape) {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (!is_recording())
    return;

  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoScheduler::DoSendCursorShape, this,
                            base::Passed(cursor_shape.Pass())));
}

void VideoScheduler::DoFinishOneRecording() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  if (!is_recording())
    return;

  // Decrement the number of recording in process since we have completed
  // one cycle.
  --recordings_;
  DCHECK_GE(recordings_, 0);

  // Try to do a capture again only if |frame_skipped_| is set to true by
  // capture timer.
  if (frame_skipped_)
    DoCapture();
}

void VideoScheduler::DoInvalidateFullScreen() {
  DCHECK(capture_task_runner_->BelongsToCurrentThread());

  SkRegion region;
  region.op(SkIRect::MakeSize(capturer_->size_most_recent()),
            SkRegion::kUnion_Op);
  capturer_->InvalidateRegion(region);
}

// Network thread --------------------------------------------------------------

void VideoScheduler::DoSendVideoPacket(scoped_ptr<VideoPacket> packet) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (network_stopped_ || connections_.empty())
    return;

  base::Closure callback;
  if ((packet->flags() & VideoPacket::LAST_PARTITION) != 0)
    callback = base::Bind(&VideoScheduler::VideoFrameSentCallback, this);

  // TODO(sergeyu): Currently we send the data only to the first
  // connection. Send it to all connections if necessary.
  connections_.front()->video_stub()->ProcessVideoPacket(
      packet.Pass(), callback);
}

void VideoScheduler::VideoFrameSentCallback() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (network_stopped_)
    return;

  capture_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoScheduler::DoFinishOneRecording, this));
}

void VideoScheduler::DoStopOnNetworkThread(const base::Closure& done_task) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  // There could be tasks on the network thread when this method is being
  // executed. By setting the flag we'll not post anymore tasks from network
  // thread.
  //
  // After that a task is posted on encode thread to continue the stop
  // sequence.
  network_stopped_ = true;

  encode_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoScheduler::DoStopOnEncodeThread,
                            this, done_task));
}

void VideoScheduler::DoSendCursorShape(
    scoped_ptr<protocol::CursorShapeInfo> cursor_shape) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (network_stopped_ || connections_.empty())
    return;

  // TODO(sergeyu): Currently we send the data only to the first
  // connection. Send it to all connections if necessary.
  connections_.front()->client_stub()->SetCursorShape(*cursor_shape);
}

// Encoder thread --------------------------------------------------------------

void VideoScheduler::DoEncode(
    scoped_refptr<CaptureData> capture_data) {
  DCHECK(encode_task_runner_->BelongsToCurrentThread());

  if (encoder_stopped_)
    return;

  // Early out if there's nothing to encode.
  if (!capture_data || capture_data->dirty_region().isEmpty()) {
    // Send an empty video packet to keep network active.
    scoped_ptr<VideoPacket> packet(new VideoPacket());
    packet->set_flags(VideoPacket::LAST_PARTITION);
    network_task_runner_->PostTask(
        FROM_HERE, base::Bind(&VideoScheduler::DoSendVideoPacket,
                              this, base::Passed(packet.Pass())));
    return;
  }

  encode_start_time_ = base::Time::Now();
  encoder()->Encode(
      capture_data, false,
      base::Bind(&VideoScheduler::EncodedDataAvailableCallback, this));
}

void VideoScheduler::DoStopOnEncodeThread(const base::Closure& done_task) {
  DCHECK(encode_task_runner_->BelongsToCurrentThread());

  encoder_stopped_ = true;

  // When this method is being executed there are no more tasks on encode thread
  // for this object. We can then post a task to capture thread to finish the
  // stop sequence.
  capture_task_runner_->PostTask(FROM_HERE, done_task);
}

void VideoScheduler::EncodedDataAvailableCallback(
    scoped_ptr<VideoPacket> packet) {
  DCHECK(encode_task_runner_->BelongsToCurrentThread());

  if (encoder_stopped_)
    return;

  bool last = (packet->flags() & VideoPacket::LAST_PACKET) != 0;
  if (last) {
    base::TimeDelta encode_time = base::Time::Now() - encode_start_time_;
    int encode_time_ms =
        static_cast<int>(encode_time.InMilliseconds());
    packet->set_encode_time_ms(encode_time_ms);
    scheduler_.RecordEncodeTime(encode_time);
  }

  network_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoScheduler::DoSendVideoPacket, this,
                            base::Passed(packet.Pass())));
}

}  // namespace remoting
