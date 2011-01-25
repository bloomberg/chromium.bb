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
      is_recording_(false),
      network_stopped_(false),
      recordings_(0),
      frame_skipped_(false),
      max_rate_(kDefaultCaptureRate) {
  DCHECK(capture_loop_);
  DCHECK(encode_loop_);
  DCHECK(network_loop_);
}

ScreenRecorder::~ScreenRecorder() {
}

// Public methods --------------------------------------------------------------

void ScreenRecorder::Start() {
  capture_loop_->PostTask(
      FROM_HERE, NewTracedMethod(this, &ScreenRecorder::DoStart));
}

void ScreenRecorder::Stop(Task* done_task) {
  capture_loop_->PostTask(
      FROM_HERE, NewTracedMethod(this, &ScreenRecorder::DoStop, done_task));
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
  StartCaptureTimer();

  // Capture first frame immedately.
  DoCapture();
}

void ScreenRecorder::DoStop(Task* done_task) {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  if (!is_recording_) {
    NOTREACHED() << "Record session not started.";
    return;
  }

  capture_timer_.Stop();
  is_recording_ = false;

  DCHECK_GE(recordings_, 0);
  if (recordings_) {
     network_loop_->PostTask(
        FROM_HERE,
        NewTracedMethod(this,
                        &ScreenRecorder::DoStopOnNetworkThread, done_task));
    return;
  }

  DoCompleteStop(done_task);
}

void ScreenRecorder::DoCompleteStop(Task* done_task) {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  if (done_task) {
    done_task->Run();
    delete done_task;
  }
}

void ScreenRecorder::DoSetMaxRate(double max_rate) {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  // TODO(hclam): Should also check for small epsilon.
  DCHECK_GT(max_rate, 0.0) << "Rate is too small.";

  max_rate_ = max_rate;

  // Restart the timer with the new rate.
  if (is_recording_) {
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
  if (recordings_ >= kMaxRecordings || !is_recording_) {
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
  DCHECK_LE(recordings_, kMaxRecordings);

  // And finally perform one capture.
  capturer()->CaptureInvalidRects(
      NewCallback(this, &ScreenRecorder::CaptureDoneCallback));
}

void ScreenRecorder::CaptureDoneCallback(
    scoped_refptr<CaptureData> capture_data) {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  if (!is_recording_)
    return;

  TraceContext::tracer()->PrintString("Capture Done");
  encode_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &ScreenRecorder::DoEncode, capture_data));
}

void ScreenRecorder::DoFinishOneRecording() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  if (!is_recording_)
    return;

  // Decrement the number of recording in process since we have completed
  // one cycle.
  --recordings_;
  DCHECK_GE(recordings_, 0);

  // Try to do a capture again. Note that the following method may do nothing
  // if it is too early to perform a capture.
  DoCapture();
}

// Network thread --------------------------------------------------------------

void ScreenRecorder::DoSendVideoPacket(VideoPacket* packet) {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  TraceContext::tracer()->PrintString("DoSendVideoPacket");

  bool last = (packet->flags() & VideoPacket::LAST_PARTITION) != 0;

  if (network_stopped_) {
    delete packet;
    return;
  }

  for (ConnectionToClientList::const_iterator i = connections_.begin();
       i < connections_.end(); ++i) {
    Task* done_task = NULL;

    // Call FrameSentCallback() only for the last packet in the first
    // connection.
    if (last && i == connections_.begin()) {
      done_task = NewTracedMethod(this, &ScreenRecorder::FrameSentCallback,
                                  packet);
    } else {
      // TODO(hclam): Fix this code since it causes multiple deletion if there's
      // more than one connection.
      done_task = new DeleteTask<VideoPacket>(packet);
    }

    (*i)->video_stub()->ProcessVideoPacket(packet, done_task);
  }

  TraceContext::tracer()->PrintString("DoSendVideoPacket done");
}

void ScreenRecorder::FrameSentCallback(VideoPacket* packet) {
  delete packet;

  if (network_stopped_)
    return;

  capture_loop_->PostTask(
      FROM_HERE, NewTracedMethod(this, &ScreenRecorder::DoFinishOneRecording));
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

void ScreenRecorder::DoStopOnNetworkThread(Task* done_task) {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  // There could be tasks on the network thread when this method is being
  // executed. By setting the flag we'll not post anymore tasks from network
  // thread.
  //
  // After that a task is posted on encode thread to continue the stop
  // sequence.
  network_stopped_ = true;

  encode_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &ScreenRecorder::DoStopOnEncodeThread, done_task));
}

// Encoder thread --------------------------------------------------------------

void ScreenRecorder::DoEncode(
    scoped_refptr<CaptureData> capture_data) {
  DCHECK_EQ(encode_loop_, MessageLoop::current());
  TraceContext::tracer()->PrintString("DoEncode called");

  // Early out if there's nothing to encode.
  if (!capture_data->dirty_rects().size()) {
    capture_loop_->PostTask(
        FROM_HERE,
        NewTracedMethod(this, &ScreenRecorder::DoFinishOneRecording));
    return;
  }

  // TODO(hclam): Invalidate the full screen if there is a new connection added.
  TraceContext::tracer()->PrintString("Encode start");
  encoder()->Encode(
      capture_data, false,
      NewCallback(this, &ScreenRecorder::EncodedDataAvailableCallback));
  TraceContext::tracer()->PrintString("Encode Done");
}

void ScreenRecorder::DoStopOnEncodeThread(Task* done_task) {
  DCHECK_EQ(encode_loop_, MessageLoop::current());

  // When this method is being executed there are no more tasks on encode thread
  // for this object. We can then post a task to capture thread to finish the
  // stop sequence.
  capture_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &ScreenRecorder::DoCompleteStop, done_task));
}

void ScreenRecorder::EncodedDataAvailableCallback(VideoPacket* packet) {
  DCHECK_EQ(encode_loop_, MessageLoop::current());

  network_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &ScreenRecorder::DoSendVideoPacket, packet));
}

}  // namespace remoting
