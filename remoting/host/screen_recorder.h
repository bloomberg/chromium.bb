// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SCREEN_RECORDER_H_
#define REMOTING_HOST_SCREEN_RECORDER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/timer.h"
#include "remoting/base/encoder.h"
#include "remoting/host/capturer.h"
#include "remoting/proto/video.pb.h"

namespace remoting {

namespace protocol {
class ConnectionToClient;
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
// |is_recording_| - If this is set to false there should be no activity on
//                      the capture thread by this object.
// |network_stopped_| - This state is to prevent activity on the network thread
//                      if set to false.
class ScreenRecorder : public base::RefCountedThreadSafe<ScreenRecorder> {
 public:

  // Construct a ScreenRecorder. Message loops and threads are provided.
  // This object does not own capturer and encoder.
  ScreenRecorder(MessageLoop* capture_loop,
                 MessageLoop* encode_loop,
                 MessageLoop* network_loop,
                 Capturer* capturer,
                 Encoder* encoder);

  virtual ~ScreenRecorder();

  // Start recording.
  void Start();

  // Stop the recording session. |done_task| is executed when recording is fully
  // stopped. This object cannot be used again after |task| is executed.
  void Stop(Task* done_task);

  // Set the maximum capture rate. This is denoted by number of updates
  // in one second. The actual system may run in a slower rate than the maximum
  // rate due to various factors, e.g. capture speed, encode speed and network
  // conditions.
  // This method should be called before Start() is called.
  void SetMaxRate(double rate);

  // Add a connection to this recording session.
  void AddConnection(scoped_refptr<protocol::ConnectionToClient> connection);

  // Remove a connection from receiving screen updates.
  void RemoveConnection(scoped_refptr<protocol::ConnectionToClient> connection);

  // Remove all connections.
  void RemoveAllConnections();

 private:
  // Getters for capturer and encoder.
  Capturer* capturer();
  Encoder* encoder();

  // Capturer thread ----------------------------------------------------------

  void DoStart();
  void DoStop(Task* done_task);
  void DoCompleteStop(Task* done_task);

  void DoSetMaxRate(double max_rate);

  // Hepler method to schedule next capture using the current rate.
  void StartCaptureTimer();

  void DoCapture();
  void CaptureDoneCallback(scoped_refptr<CaptureData> capture_data);
  void DoFinishOneRecording();

  // Network thread -----------------------------------------------------------

  // DoSendVideoPacket takes ownership of the |packet| and is responsible
  // for deleting it.
  void DoSendVideoPacket(VideoPacket* packet);

  void DoSendInit(scoped_refptr<protocol::ConnectionToClient> connection,
                  int width, int height);

  void DoAddConnection(scoped_refptr<protocol::ConnectionToClient> connection);
  void DoRemoveClient(scoped_refptr<protocol::ConnectionToClient> connection);
  void DoRemoveAllClients();

  // Signal network thread to cease activities.
  void DoStopOnNetworkThread(Task* done_task);

  // Callback for the last packet in one update. Deletes |packet| and
  // schedules next screen capture.
  void FrameSentCallback(VideoPacket* packet);

  // Encoder thread -----------------------------------------------------------

  void DoEncode(scoped_refptr<CaptureData> capture_data);

  // Perform stop operations on encode thread.
  void DoStopOnEncodeThread(Task* done_task);

  // EncodedDataAvailableCallback takes ownership of |packet|.
  void EncodedDataAvailableCallback(VideoPacket* packet);
  void SendVideoPacket(VideoPacket* packet);

  // Message loops used by this class.
  MessageLoop* capture_loop_;
  MessageLoop* encode_loop_;
  MessageLoop* network_loop_;

  // Reference to the capturer. This member is always accessed on the capture
  // thread.
  scoped_ptr<Capturer> capturer_;

  // Reference to the encoder. This member is always accessed on the encode
  // thread.
  scoped_ptr<Encoder> encoder_;

  // A list of clients connected to this hosts.
  // This member is always accessed on the network thread.
  typedef std::vector<scoped_refptr<protocol::ConnectionToClient> >
      ConnectionToClientList;
  ConnectionToClientList connections_;

  // Flag that indicates recording has been started. This variable should only
  // be used on the capture thread.
  bool is_recording_;

  // Flag that indicates network is being stopped. This variable should only
  // be used on the network thread.
  bool network_stopped_;

  // Timer that calls DoCapture.
  base::RepeatingTimer<ScreenRecorder> capture_timer_;

  // Count the number of recordings (i.e. capture or encode) happening.
  int recordings_;

  // Set to true if we've skipped last capture because there are too
  // many pending frames.
  int frame_skipped_;

  // Number of captures to perform every second. Written on the capture thread.
  double max_rate_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRecorder);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SCREEN_RECORDER_H_
