// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/screen_recorder.h"

#include <algorithm>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "base/time.h"
#include "remoting/base/capture_data.h"
#include "remoting/base/tracer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/message_decoder.h"
#include "remoting/protocol/util.h"

using remoting::protocol::ConnectionToClient;

namespace remoting {

// By default we capture 20 times a second. This number is obtained by
// experiment to provide good latency.
static const double kDefaultCaptureRate = 20.0;

// Maximum number of frames that can be processed similtaneously.
// TODO(sergeyu): Should this be set to 1? Or should we change
// dynamically depending on how fast network and CPU are? Experement
// with it.
static const int kMaxRecordings = 2;

ScreenRecorder::ScreenRecorder(
    MessageLoop* capture_loop,
    MessageLoop* encode_loop,
    MessageLoop* network_loop,
    Capturer* capturer,
    Encoder* encoder)
    : capture_loop_(capture_loop),
      encode_loop_(encode_loop),
      network_loop_(network_loop),
      capturer_(capturer),
      encoder_(encoder),
      started_(false),
      recordings_(0),
      frame_skipped_(false),
      max_rate_(kDefaultCaptureRate) {
  DCHECK(capture_loop_);
  DCHECK(encode_loop_);
  DCHECK(network_loop_);
}

ScreenRecorder::~ScreenRecorder() {
  connections_.clear();
}

// Public methods --------------------------------------------------------------

void ScreenRecorder::Start() {
  capture_loop_->PostTask(
      FROM_HERE, NewTracedMethod(this, &ScreenRecorder::DoStart));
}

void ScreenRecorder::Pause() {
  capture_loop_->PostTask(
      FROM_HERE, NewTracedMethod(this, &ScreenRecorder::DoPause));
}

void ScreenRecorder::SetMaxRate(double rate) {
  capture_loop_->PostTask(
      FROM_HERE, NewTracedMethod(this, &ScreenRecorder::DoSetMaxRate, rate));
}

void ScreenRecorder::AddConnection(
    scoped_refptr<ConnectionToClient> connection) {
  ScopedTracer tracer("AddConnection");

  // Add the client to the list so it can receive update stream.
  network_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &ScreenRecorder::DoAddConnection, connection));
}

void ScreenRecorder::RemoveConnection(
    scoped_refptr<ConnectionToClient> connection) {
  network_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &ScreenRecorder::DoRemoveClient, connection));
}

void ScreenRecorder::RemoveAllConnections() {
  network_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &ScreenRecorder::DoRemoveAllClients));
}

// Private accessors -----------------------------------------------------------

Capturer* ScreenRecorder::capturer() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());
  DCHECK(capturer_.get());
  return capturer_.get();
}

Encoder* ScreenRecorder::encoder() {
  DCHECK_EQ(encode_loop_, MessageLoop::current());
  DCHECK(encoder_.get());
  return encoder_.get();
}

// Capturer thread -------------------------------------------------------------

void ScreenRecorder::DoStart() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());
  DCHECK(!started_);

  if (started_) {
    NOTREACHED() << "Record session already started.";
    return;
  }

  started_ = true;
  StartCaptureTimer();

  // Capture first frame immedately.
  DoCapture();
}

void ScreenRecorder::DoPause() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  if (!started_) {
    NOTREACHED() << "Record session not started.";
    return;
  }

  capture_timer_.Stop();
  started_ = false;
}

void ScreenRecorder::DoSetMaxRate(double max_rate) {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  // TODO(hclam): Should also check for small epsilon.
  DCHECK_GT(max_rate, 0.0) << "Rate is too small.";

  max_rate_ = max_rate;

  // Restart the timer with the new rate.
  if (started_) {
    capture_timer_.Stop();
    StartCaptureTimer();
  }
}

void ScreenRecorder::StartCaptureTimer() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  base::TimeDelta interval = base::TimeDelta::FromMilliseconds(
      static_cast<int>(base::Time::kMillisecondsPerSecond / max_rate_));
  capture_timer_.Start(interval, this, &ScreenRecorder::DoCapture);
}

void ScreenRecorder::DoCapture() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());
  // Make sure we have at most two oustanding recordings. We can simply return
  // if we can't make a capture now, the next capture will be started by the
  // end of an encode operation.
  if (recordings_ >= kMaxRecordings || !started_) {
    frame_skipped_ = true;
    return;
  }

  if (frame_skipped_) {
    frame_skipped_ = false;
    capture_timer_.Reset();
  }

  TraceContext::tracer()->PrintString("Capture Started");

  // At this point we are going to perform one capture so save the current time.
  ++recordings_;

  // And finally perform one capture.
  capturer()->CaptureInvalidRects(
      NewCallback(this, &ScreenRecorder::CaptureDoneCallback));
}

void ScreenRecorder::CaptureDoneCallback(
    scoped_refptr<CaptureData> capture_data) {
  // TODO(hclam): There is a bug if the capturer doesn't produce any dirty
  // rects.
  DCHECK_EQ(capture_loop_, MessageLoop::current());
  TraceContext::tracer()->PrintString("Capture Done");
  encode_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &ScreenRecorder::DoEncode, capture_data));
}

void ScreenRecorder::DoFinishSend() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  // Decrement the number of recording in process since we have completed
  // one cycle.
  --recordings_;

  // Try to do a capture again. Note that the following method may do nothing
  // if it is too early to perform a capture.
  DoCapture();
}

// Network thread --------------------------------------------------------------

void ScreenRecorder::DoSendVideoPacket(VideoPacket* packet) {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  TraceContext::tracer()->PrintString("DoSendVideoPacket");

  bool last = (packet->flags() & VideoPacket::LAST_PARTITION) != 0;

  for (ConnectionToClientList::const_iterator i = connections_.begin();
       i < connections_.end(); ++i) {
    Task* done_task = NULL;

    // Call OnFrameSent() only for the last packet in the first connection.
    if (last && i == connections_.begin()) {
      done_task = NewTracedMethod(this, &ScreenRecorder::OnFrameSent, packet);
    } else {
      done_task = new DeleteTask<VideoPacket>(packet);
    }

    (*i)->video_stub()->ProcessVideoPacket(packet, done_task);
  }

  TraceContext::tracer()->PrintString("DoSendVideoPacket done");
}

void ScreenRecorder::OnFrameSent(VideoPacket* packet) {
  delete packet;
  capture_loop_->PostTask(
      FROM_HERE, NewTracedMethod(this, &ScreenRecorder::DoFinishSend));
}

void ScreenRecorder::DoAddConnection(
    scoped_refptr<ConnectionToClient> connection) {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  // TODO(hclam): Force a full frame for next encode.
  connections_.push_back(connection);
}

void ScreenRecorder::DoRemoveClient(
    scoped_refptr<ConnectionToClient> connection) {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  // TODO(hclam): Is it correct to do to a scoped_refptr?
  ConnectionToClientList::iterator it =
      std::find(connections_.begin(), connections_.end(), connection);
  if (it != connections_.end()) {
    connections_.erase(it);
  }
}

void ScreenRecorder::DoRemoveAllClients() {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  // Clear the list of connections.
  connections_.clear();
}

// Encoder thread --------------------------------------------------------------

void ScreenRecorder::DoEncode(
    scoped_refptr<CaptureData> capture_data) {
  DCHECK_EQ(encode_loop_, MessageLoop::current());
  TraceContext::tracer()->PrintString("DoEncode called");

  // Early out if there's nothing to encode.
  if (!capture_data->dirty_rects().size()) {
    capture_loop_->PostTask(
        FROM_HERE, NewTracedMethod(this, &ScreenRecorder::DoFinishSend));
    return;
  }

  // TODO(hclam): Enable |force_refresh| if a new connection was
  // added.
  TraceContext::tracer()->PrintString("Encode start");
  encoder()->Encode(capture_data, false,
                   NewCallback(this, &ScreenRecorder::EncodeDataAvailableTask));
  TraceContext::tracer()->PrintString("Encode Done");
}

void ScreenRecorder::EncodeDataAvailableTask(VideoPacket* packet) {
  DCHECK_EQ(encode_loop_, MessageLoop::current());

  network_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &ScreenRecorder::DoSendVideoPacket, packet));
}

}  // namespace remoting
