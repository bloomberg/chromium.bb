// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SCREEN_RECORDER_H_
#define REMOTING_HOST_SCREEN_RECORDER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "remoting/base/encoder.h"
#include "remoting/host/capturer.h"
#include "remoting/host/capture_scheduler.h"
#include "remoting/proto/video.pb.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

namespace protocol {
class ConnectionToClient;
class CursorShapeInfo;
}  // namespace protocol

class CaptureData;

// A class for controlling and coordinate Capturer, Encoder
// and NetworkChannel in a record session.
//
// THREADING
//
// This class works on three threads, namely capture, encode and network
// thread. The main function of this class is to coordinate and schedule
// capture, encode and transmission of data on different threads.
//
// The following is an example of timeline for operations scheduled.
//
// |       CAPTURE       ENCODE     NETWORK
// |    .............
// |    .  Capture  .
// |    .............
// |                  ............
// |                  .          .
// |    ............. .          .
// |    .  Capture  . .  Encode  .
// |    ............. .          .
// |                  .          .
// |                  ............
// |    ............. ............ ..........
// |    .  Capture  . .          . .  Send  .
// |    ............. .          . ..........
// |                  .  Encode  .
// |                  .          .
// |                  .          .
// |                  ............
// | Time
// v
//
// ScreenRecorder has the following responsibilities:
// 1. Make sure capture and encode occurs no more frequently than |rate|.
// 2. Make sure there is at most one outstanding capture not being encoded.
// 3. Distribute tasks on three threads on a timely fashion to minimize latency.
//
// This class has the following state variables:
// |capture_timer_| - If this is set to NULL there should be no activity on
//                    the capture thread by this object.
// |network_stopped_| - This state is to prevent activity on the network thread
//                      if set to false.
class ScreenRecorder : public base::RefCountedThreadSafe<ScreenRecorder> {
 public:

  // Construct a ScreenRecorder. Message loops and threads are provided.
  // This object does not own capturer but owns encoder.
  ScreenRecorder(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      Capturer* capturer,
      Encoder* encoder);

  // Start recording.
  void Start();

  // Stop the recording session. |done_task| is executed when recording is fully
  // stopped. This object cannot be used again after |task| is executed.
  void Stop(const base::Closure& done_task);

  // Add a connection to this recording session.
  void AddConnection(protocol::ConnectionToClient* connection);

  // Remove a connection from receiving screen updates.
  void RemoveConnection(protocol::ConnectionToClient* connection);

  // Remove all connections.
  void RemoveAllConnections();

  // Update the sequence number for tracing performance.
  void UpdateSequenceNumber(int64 sequence_number);

 private:
  friend class base::RefCountedThreadSafe<ScreenRecorder>;
  virtual ~ScreenRecorder();

  // Getters for capturer and encoder.
  Capturer* capturer();
  Encoder* encoder();

  bool is_recording();

  // Capturer thread ----------------------------------------------------------

  void DoStart();
  void DoSetMaxRate(double max_rate);

  // Hepler method to schedule next capture using the current rate.
  void StartCaptureTimer();

  void DoCapture();
  void CaptureDoneCallback(scoped_refptr<CaptureData> capture_data);
  void CursorShapeChangedCallback(
      scoped_ptr<protocol::CursorShapeInfo> cursor_data);
  void DoFinishOneRecording();
  void DoInvalidateFullScreen();

  // Network thread -----------------------------------------------------------

  void DoSendVideoPacket(scoped_ptr<VideoPacket> packet);

  void DoSendInit(scoped_refptr<protocol::ConnectionToClient> connection,
                  int width, int height);

  // Signal network thread to cease activities.
  void DoStopOnNetworkThread(const base::Closure& done_task);

  // Callback for VideoStub::ProcessVideoPacket() that is used for
  // each last packet in a frame.
  void VideoFrameSentCallback();

  // Send updated cursor shape to client.
  void DoSendCursorShape(scoped_ptr<protocol::CursorShapeInfo> cursor_shape);

  // Encoder thread -----------------------------------------------------------

  void DoEncode(scoped_refptr<CaptureData> capture_data);

  // Perform stop operations on encode thread.
  void DoStopOnEncodeThread(const base::Closure& done_task);

  void EncodedDataAvailableCallback(scoped_ptr<VideoPacket> packet);
  void SendVideoPacket(VideoPacket* packet);

  // Task runners used by this class.
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  // Reference to the capturer. This member is always accessed on the capture
  // thread.
  Capturer* capturer_;

  // Reference to the encoder. This member is always accessed on the encode
  // thread.
  scoped_ptr<Encoder> encoder_;

  // A list of clients connected to this hosts.
  // This member is always accessed on the network thread.
  typedef std::vector<protocol::ConnectionToClient*> ConnectionToClientList;
  ConnectionToClientList connections_;

  // Timer that calls DoCapture. Set to NULL when not recording.
  scoped_ptr<base::OneShotTimer<ScreenRecorder> > capture_timer_;

  // Per-thread flags that are set when the ScreenRecorder is
  // stopped. They must be used on the corresponding threads only.
  bool network_stopped_;
  bool encoder_stopped_;

  // Maximum simultaneous recordings allowed.
  int max_recordings_;

  // Count the number of recordings (i.e. capture or encode) happening.
  int recordings_;

  // Set to true if we've skipped last capture because there are too
  // many pending frames.
  int frame_skipped_;

  // Time when capture is started.
  base::Time capture_start_time_;

  // Time when encode is started.
  base::Time encode_start_time_;

  // This is a number updated by client to trace performance.
  int64 sequence_number_;

  // An object to schedule capturing.
  CaptureScheduler scheduler_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRecorder);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SCREEN_RECORDER_H_
