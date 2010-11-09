// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/session_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "remoting/base/capture_data.h"
#include "remoting/base/tracer.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/connection_to_client.h"
#include "remoting/protocol/message_decoder.h"
#include "remoting/protocol/util.h"

using remoting::protocol::ConnectionToClient;

namespace remoting {

// By default we capture 20 times a second. This number is obtained by
// experiment to provide good latency.
static const double kDefaultCaptureRate = 20.0;

// Interval that we perform rate regulation.
static const base::TimeDelta kRateControlInterval =
    base::TimeDelta::FromSeconds(1);

// We divide the pending update stream number by this value to determine the
// rate divider.
static const int kSlowDownFactor = 10;

// A list of dividers used to divide the max rate to determine the current
// capture rate.
static const int kRateDividers[] = {1, 2, 4, 8, 16};

SessionManager::SessionManager(
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
      rate_(kDefaultCaptureRate),
      started_(false),
      recordings_(0),
      max_rate_(kDefaultCaptureRate),
      rate_control_started_(false) {
  DCHECK(capture_loop_);
  DCHECK(encode_loop_);
  DCHECK(network_loop_);
}

SessionManager::~SessionManager() {
  connections_.clear();
}

// Public methods --------------------------------------------------------------

void SessionManager::Start() {
  capture_loop_->PostTask(
      FROM_HERE, NewTracedMethod(this, &SessionManager::DoStart));
}

void SessionManager::Pause() {
  capture_loop_->PostTask(
      FROM_HERE, NewTracedMethod(this, &SessionManager::DoPause));
}

void SessionManager::SetMaxRate(double rate) {
  capture_loop_->PostTask(
      FROM_HERE, NewTracedMethod(this, &SessionManager::DoSetMaxRate, rate));
}

void SessionManager::AddConnection(
    scoped_refptr<ConnectionToClient> connection) {
  // Gets the init information for the connection.
  capture_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &SessionManager::DoGetInitInfo, connection));
}

void SessionManager::RemoveConnection(
    scoped_refptr<ConnectionToClient> connection) {
  network_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &SessionManager::DoRemoveClient, connection));
}

void SessionManager::RemoveAllConnections() {
  network_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &SessionManager::DoRemoveAllClients));
}

// Private accessors -----------------------------------------------------------

Capturer* SessionManager::capturer() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());
  return capturer_.get();
}

Encoder* SessionManager::encoder() {
  DCHECK_EQ(encode_loop_, MessageLoop::current());
  return encoder_.get();
}

// Capturer thread -------------------------------------------------------------

void SessionManager::DoStart() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  if (started_) {
    NOTREACHED() << "Record session already started.";
    return;
  }

  started_ = true;
  DoCapture();

  // Starts the rate regulation.
  network_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &SessionManager::DoStartRateControl));
}

void SessionManager::DoPause() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  if (!started_) {
    NOTREACHED() << "Record session not started.";
    return;
  }

  started_ = false;

  // Pause the rate regulation.
  network_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &SessionManager::DoPauseRateControl));
}

void SessionManager::DoSetRate(double rate) {
  DCHECK_EQ(capture_loop_, MessageLoop::current());
  if (rate == rate_)
    return;

  // Change the current capture rate.
  rate_ = rate;

  // If we have already started then schedule the next capture with the new
  // rate.
  if (started_)
    ScheduleNextCapture();
}

void SessionManager::DoSetMaxRate(double max_rate) {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  // TODO(hclam): Should also check for small epsilon.
  if (max_rate != 0) {
    max_rate_ = max_rate;
    DoSetRate(max_rate);
  } else {
    NOTREACHED() << "Rate is too small.";
  }
}

void SessionManager::ScheduleNextCapture() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  ScopedTracer tracer("capture");

  TraceContext::tracer()->PrintString("Capture Scheduled");

  if (rate_ == 0)
    return;

  base::TimeDelta interval = base::TimeDelta::FromMilliseconds(
      static_cast<int>(base::Time::kMillisecondsPerSecond / rate_));
  capture_loop_->PostDelayedTask(
      FROM_HERE,
      NewTracedMethod(this, &SessionManager::DoCapture),
      interval.InMilliseconds());
}

void SessionManager::DoCapture() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());
  // Make sure we have at most two oustanding recordings. We can simply return
  // if we can't make a capture now, the next capture will be started by the
  // end of an encode operation.
  if (recordings_ >= 2 || !started_) {
    return;
  }
  TraceContext::tracer()->PrintString("Capture Started");

  base::Time now = base::Time::Now();
  base::TimeDelta interval = base::TimeDelta::FromMilliseconds(
      static_cast<int>(base::Time::kMillisecondsPerSecond / rate_));
  base::TimeDelta elapsed = now - last_capture_time_;

  // If this method is called sooner than the required interval we return
  // immediately
  if (elapsed < interval) {
    return;
  }

  // At this point we are going to perform one capture so save the current time.
  last_capture_time_ = now;
  ++recordings_;

  // Before we actually do a capture, schedule the next one.
  ScheduleNextCapture();

  // And finally perform one capture.
  DCHECK(capturer());

  capturer()->CaptureInvalidRects(
      NewCallback(this, &SessionManager::CaptureDoneCallback));
}

void SessionManager::CaptureDoneCallback(
    scoped_refptr<CaptureData> capture_data) {
  // TODO(hclam): There is a bug if the capturer doesn't produce any dirty
  // rects.
  DCHECK_EQ(capture_loop_, MessageLoop::current());
  TraceContext::tracer()->PrintString("Capture Done");
  encode_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &SessionManager::DoEncode, capture_data));
}

void SessionManager::DoFinishEncode() {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  // Decrement the number of recording in process since we have completed
  // one cycle.
  --recordings_;

  // Try to do a capture again. Note that the following method may do nothing
  // if it is too early to perform a capture.
  if (rate_ > 0)
    DoCapture();
}

void SessionManager::DoGetInitInfo(
    scoped_refptr<ConnectionToClient> connection) {
  DCHECK_EQ(capture_loop_, MessageLoop::current());

  ScopedTracer tracer("init");

  // Sends the init message to the connection.
  network_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &SessionManager::DoSendInit, connection,
                      capturer()->width(), capturer()->height()));

  // And then add the connection to the list so it can receive update stream.
  // It is important we do so in such order or the connection will receive
  // update stream before init message.
  network_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &SessionManager::DoAddClient, connection));
}

// Network thread --------------------------------------------------------------

void SessionManager::DoStartRateControl() {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  if (rate_control_started_) {
    NOTREACHED() << "Rate regulation already started";
    return;
  }
  rate_control_started_ = true;
  ScheduleNextRateControl();
}

void SessionManager::DoPauseRateControl() {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  if (!rate_control_started_) {
    NOTREACHED() << "Rate regulation not started";
    return;
  }
  rate_control_started_ = false;
}

void SessionManager::ScheduleNextRateControl() {
  ScopedTracer tracer("Rate Control");
  network_loop_->PostDelayedTask(
      FROM_HERE,
      NewTracedMethod(this, &SessionManager::DoRateControl),
      kRateControlInterval.InMilliseconds());
}

void SessionManager::DoRateControl() {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  // If we have been paused then shutdown the rate regulation loop.
  if (!rate_control_started_)
    return;

  int max_pending_update_streams = 0;
  for (size_t i = 0; i < connections_.size(); ++i) {
    max_pending_update_streams =
        std::max(max_pending_update_streams,
                 connections_[i]->GetPendingUpdateStreamMessages());
  }

  // If |slow_down| equals zero, we have no slow down.
  size_t slow_down = max_pending_update_streams / kSlowDownFactor;
  // Set new_rate to -1 for checking later.
  double new_rate = -1;
  // If the slow down is too large.
  if (slow_down >= arraysize(kRateDividers)) {
    // Then we stop the capture completely.
    new_rate = 0;
  } else {
    // Slow down the capture rate using the divider.
    new_rate = max_rate_ / kRateDividers[slow_down];
  }
  DCHECK_NE(new_rate, -1.0);

  // Then set the rate.
  capture_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &SessionManager::DoSetRate, new_rate));
  ScheduleNextRateControl();
}

void SessionManager::DoSendVideoPacket(VideoPacket* packet) {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  TraceContext::tracer()->PrintString("DoSendUpdate");

  for (ConnectionToClientList::const_iterator i = connections_.begin();
       i < connections_.end(); ++i) {
    (*i)->SendVideoPacket(*packet);
  }
  delete packet;

  TraceContext::tracer()->PrintString("DoSendUpdate done");
}

void SessionManager::DoSendInit(scoped_refptr<ConnectionToClient> connection,
                                int width, int height) {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  // Sends the connection init information.
  protocol::NotifyResolutionRequest* message =
      new protocol::NotifyResolutionRequest();
  message->set_width(width);
  message->set_height(height);
  connection->client_stub()->NotifyResolution(message,
                                              NewDeleteMessageTask(message));
}

void SessionManager::DoAddClient(scoped_refptr<ConnectionToClient> connection) {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  // TODO(hclam): Force a full frame for next encode.
  connections_.push_back(connection);
}

void SessionManager::DoRemoveClient(
    scoped_refptr<ConnectionToClient> connection) {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  // TODO(hclam): Is it correct to do to a scoped_refptr?
  ConnectionToClientList::iterator it
      = std::find(connections_.begin(), connections_.end(), connection);
  if (it != connections_.end()) {
    connections_.erase(it);
  }
}

void SessionManager::DoRemoveAllClients() {
  DCHECK_EQ(network_loop_, MessageLoop::current());

  // Clear the list of connections.
  connections_.clear();
}

// Encoder thread --------------------------------------------------------------

void SessionManager::DoEncode(
    scoped_refptr<CaptureData> capture_data) {
  DCHECK_EQ(encode_loop_, MessageLoop::current());
  TraceContext::tracer()->PrintString("DoEncode called");

  // Early out if there's nothing to encode.
  if (!capture_data->dirty_rects().size()) {
    capture_loop_->PostTask(
        FROM_HERE, NewTracedMethod(this, &SessionManager::DoFinishEncode));
    return;
  }

  // TODO(hclam): Enable |force_refresh| if a new connection was
  // added.
  TraceContext::tracer()->PrintString("Encode start");
  encoder_->Encode(capture_data, false,
                   NewCallback(this, &SessionManager::EncodeDataAvailableTask));
  TraceContext::tracer()->PrintString("Encode Done");
}

void SessionManager::EncodeDataAvailableTask(VideoPacket* packet) {
  DCHECK_EQ(encode_loop_, MessageLoop::current());

  bool last = (packet->flags() & VideoPacket::LAST_PACKET) != 0;

  // Before a new encode task starts, notify connected clients a new update
  // stream is coming.
  // Notify this will keep a reference to the DataBuffer in the
  // task. The ownership will eventually pass to the ConnectionToClients.
  network_loop_->PostTask(
      FROM_HERE,
      NewTracedMethod(this, &SessionManager::DoSendVideoPacket, packet));

  if (last) {
    capture_loop_->PostTask(
        FROM_HERE, NewTracedMethod(this, &SessionManager::DoFinishEncode));
  }
}

}  // namespace remoting
