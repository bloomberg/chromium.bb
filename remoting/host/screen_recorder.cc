// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/screen_recorder.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/stl_util.h"
#include "base/sys_info.h"
#include "base/task.h"
#include "base/time.h"
#include "remoting/base/capture_data.h"
#include "remoting/proto/control.pb.h"
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

ScreenRecorder::ScreenRecorder(
    MessageLoop* capture_loop,
    MessageLoop* encode_loop,
    base::MessageLoopProxy* network_loop,
    Capturer* capturer,
    Encoder* encoder)
    : capture_loop_(capture_loop),
      encode_loop_(encode_loop),
      network_loop_(network_loop),
      capturer_(capturer),
      encoder_(encoder),
      is_recording_(false),
      network_stopped_(false),
      encoder_stopped_(false),
      max_recordings_(kMaxRecordings),
      recordings_(0),
      frame_skipped_(false),
      sequence_number_(0) {
  DCHECK(capture_loop_);
  DCHECK(encode_loop_);
  DCHECK(network_loop_);
}

ScreenRecorder::~ScreenRecorder() {
}

// Public methods --------------------------------------------------------------

void ScreenRecorder::Start() {
  capture_loop_->PostTask(
      FROM_HERE, base::Bind(&ScreenRecorder::DoStart, this));
}

void ScreenRecorder::Stop(const base::Closure& done_task) {
  if (MessageLoop::current() != capture_loop_) {
    capture_loop_->PostTask(FROM_HERE, base::Bind(
        &ScreenRecorder::Stop, this, done_task));
    return;
  }

  DCHECK(!done_task.is_null());

  capture_timer_.Stop();
  is_recording_ = false;

  network_loop_->PostTask(FROM_HERE, base::Bind(
      &ScreenRecorder::DoStopOnNetworkThread, this, done_task));
}

void ScreenRecorder::AddConnection(
    scoped_refptr<ConnectionToClient> connection) {
  capture_loop_->PostTask(
      FROM_HERE, base::Bind(&ScreenRecorder::DoInvalidateFullScreen, this));

  // Add the client to the list so it can receive update stream.
  network_loop_->PostTask(
      FROM_HERE, base::Bind(&ScreenRecorder::DoAddConnection,
                            this, connection));
}

void ScreenRecorder::RemoveConnection(
    scoped_refptr<ConnectionToClient> connection) {
  network_loop_->PostTask(
      FROM_HERE, base::Bind(&ScreenRecorder::DoRemoveClient, this, connection));
}

void ScreenRecorder::RemoveAllConnections() {
  network_loop_->PostTask(
      FROM_HERE, base::Bind(&ScreenRecorder::DoRemoveAllClients, this));
}

void ScreenRecorder::UpdateSequenceNumber(int64 sequence_number) {
  // Sequence number is used and written only on the capture thread.
  if (MessageLoop::current() != capture_loop_) {
    capture_loop_->PostTask(
        FROM_HERE, base::Bind(&ScreenRecorder::UpdateSequenceNumber,
                              this, sequence_number));
    return;
  }

  sequence_number_ = sequence_number;
}

// Private accessors -----------------------------------------------------------

Capturer* ScreenRecorder::capturer() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());
  DCHECK(capturer_);
  return capturer_;
}

Encoder* ScreenRecorder::encoder() {
  DCHECK_EQ(encode_loop_, MessageLoop::current());
  DCHECK(encoder_.get());
  return encoder_.get();
}

// Capturer thread -------------------------------------------------------------

void ScreenRecorder::DoStart() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  if (is_recording_) {
    NOTREACHED() << "Record session already started.";
    return;
  }

  is_recording_ = true;

  // Capture first frame immedately.
  DoCapture();
}

void ScreenRecorder::StartCaptureTimer() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  capture_timer_.Start(FROM_HERE,
                       scheduler_.NextCaptureDelay(),
                       this,
                       &ScreenRecorder::DoCapture);
}

void ScreenRecorder::DoCapture() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());
  // Make sure we have at most two oustanding recordings. We can simply return
  // if we can't make a capture now, the next capture will be started by the
  // end of an encode operation.
  if (recordings_ >= max_recordings_ || !is_recording_) {
    frame_skipped_ = true;
    return;
  }

  if (frame_skipped_)
    frame_skipped_ = false;

  // At this point we are going to perform one capture so save the current time.
  ++recordings_;
  DCHECK_LE(recordings_, max_recordings_);

  // Before doing a capture schedule for the next one.
  capture_timer_.Stop();
  capture_timer_.Start(FROM_HERE,
                       scheduler_.NextCaptureDelay(),
                       this,
                       &ScreenRecorder::DoCapture);

  // And finally perform one capture.
  capture_start_time_ = base::Time::Now();
  capturer()->CaptureInvalidRegion(
      base::Bind(&ScreenRecorder::CaptureDoneCallback, this));
}

void ScreenRecorder::CaptureDoneCallback(
    scoped_refptr<CaptureData> capture_data) {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  if (!is_recording_)
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

  encode_loop_->PostTask(
      FROM_HERE, base::Bind(&ScreenRecorder::DoEncode, this, capture_data));
}

void ScreenRecorder::DoFinishOneRecording() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  if (!is_recording_)
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

void ScreenRecorder::DoInvalidateFullScreen() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  capturer_->InvalidateFullScreen();
}

// Network thread --------------------------------------------------------------

void ScreenRecorder::DoSendVideoPacket(VideoPacket* packet) {
  DCHECK(network_loop_->BelongsToCurrentThread());

  bool last = (packet->flags() & VideoPacket::LAST_PARTITION) != 0;

  if (network_stopped_ || connections_.empty()) {
    delete packet;
    return;
  }

  for (ConnectionToClientList::const_iterator i = connections_.begin();
       i < connections_.end(); ++i) {
    base::Closure done_task;

    // Call FrameSentCallback() only for the last packet in the first
    // connection.
    if (last && i == connections_.begin()) {
      done_task = base::Bind(&ScreenRecorder::FrameSentCallback, this, packet);
    } else {
      // TODO(hclam): Fix this code since it causes multiple deletion if there's
      // more than one connection.
      done_task = base::Bind(&DeletePointer<VideoPacket>, packet);
    }

    (*i)->video_stub()->ProcessVideoPacket(packet, done_task);
  }
}

void ScreenRecorder::FrameSentCallback(VideoPacket* packet) {
  delete packet;

  if (network_stopped_)
    return;

  capture_loop_->PostTask(
      FROM_HERE, base::Bind(&ScreenRecorder::DoFinishOneRecording, this));
}

void ScreenRecorder::DoAddConnection(
    scoped_refptr<ConnectionToClient> connection) {
  DCHECK(network_loop_->BelongsToCurrentThread());

  connections_.push_back(connection);
}

void ScreenRecorder::DoRemoveClient(
    scoped_refptr<ConnectionToClient> connection) {
  DCHECK(network_loop_->BelongsToCurrentThread());

  ConnectionToClientList::iterator it =
      std::find(connections_.begin(), connections_.end(), connection);
  if (it != connections_.end()) {
    connections_.erase(it);
  }
}

void ScreenRecorder::DoRemoveAllClients() {
  DCHECK(network_loop_->BelongsToCurrentThread());

  // Clear the list of connections.
  connections_.clear();
}

void ScreenRecorder::DoStopOnNetworkThread(const base::Closure& done_task) {
  DCHECK(network_loop_->BelongsToCurrentThread());

  // There could be tasks on the network thread when this method is being
  // executed. By setting the flag we'll not post anymore tasks from network
  // thread.
  //
  // After that a task is posted on encode thread to continue the stop
  // sequence.
  network_stopped_ = true;

  encode_loop_->PostTask(
      FROM_HERE, base::Bind(&ScreenRecorder::DoStopOnEncodeThread,
                            this, done_task));
}

// Encoder thread --------------------------------------------------------------

void ScreenRecorder::DoEncode(
    scoped_refptr<CaptureData> capture_data) {
  DCHECK_EQ(encode_loop_, MessageLoop::current());

  // Early out if there's nothing to encode.
  if (!capture_data || capture_data->dirty_region().isEmpty()) {
    // Send an empty video packet to keep network active.
    VideoPacket* packet = new VideoPacket();
    packet->set_flags(VideoPacket::LAST_PARTITION);
    network_loop_->PostTask(
        FROM_HERE, base::Bind(&ScreenRecorder::DoSendVideoPacket,
                              this, packet));
    return;
  }

  encode_start_time_ = base::Time::Now();
  encoder()->Encode(
      capture_data, false,
      base::Bind(&ScreenRecorder::EncodedDataAvailableCallback, this));
}

void ScreenRecorder::DoStopOnEncodeThread(const base::Closure& done_task) {
  DCHECK_EQ(encode_loop_, MessageLoop::current());

  encoder_stopped_ = true;

  // When this method is being executed there are no more tasks on encode thread
  // for this object. We can then post a task to capture thread to finish the
  // stop sequence.
  capture_loop_->PostTask(FROM_HERE, done_task);
}

void ScreenRecorder::EncodedDataAvailableCallback(VideoPacket* packet) {
  DCHECK_EQ(encode_loop_, MessageLoop::current());

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

  network_loop_->PostTask(
      FROM_HERE, base::Bind(&ScreenRecorder::DoSendVideoPacket, this, packet));
}

}  // namespace remoting
