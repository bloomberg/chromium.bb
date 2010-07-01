// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_RECORD_SESSION_H_
#define REMOTING_HOST_RECORD_SESSION_H_

#include <vector>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "remoting/base/protocol/chromotocol.pb.h"
#include "remoting/host/capturer.h"
#include "remoting/host/encoder.h"

namespace media {

class DataBuffer;

}  // namespace media

namespace remoting {

class Encoder;
class ClientConnection;

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
// SessionManager has the following responsibilities:
// 1. Make sure capture and encode occurs no more frequently than |rate|.
// 2. Make sure there is at most one outstanding capture not being encoded.
// 3. Distribute tasks on three threads on a timely fashion to minimize latency.
class SessionManager : public base::RefCountedThreadSafe<SessionManager> {
 public:

  // Construct a SessionManager. Message loops and threads are provided.
  // This object does not own capturer and encoder.
  SessionManager(MessageLoop* capture_loop,
                 MessageLoop* encode_loop,
                 MessageLoop* network_loop,
                 Capturer* capturer,
                 Encoder* encoder);

  virtual ~SessionManager();

  // Start recording.
  void Start();

  // Pause the recording session.
  void Pause();

  // Set the maximum capture rate. This is denoted by number of updates
  // in one second. The actual system may run in a slower rate than the maximum
  // rate due to various factors, e.g. capture speed, encode speed and network
  // conditions.
  // This method should be called before Start() is called.
  void SetMaxRate(double rate);

  // Add a client to this recording session.
  void AddClient(scoped_refptr<ClientConnection> client);

  // Remove a client from receiving screen updates.
  void RemoveClient(scoped_refptr<ClientConnection> client);

  // Remove all clients.
  void RemoveAllClients();

 private:
  void DoStart();
  void DoPause();
  void DoStartRateControl();
  void DoPauseRateControl();

  void DoCapture();
  void DoFinishEncode();

  void DoEncode(scoped_refptr<Capturer::CaptureData> capture_data);
  // DoSendUpdate takes ownership of header and is responsible for deleting it.
  void DoSendUpdate(const UpdateStreamPacketHeader* header,
                    const scoped_refptr<media::DataBuffer> data,
                    Encoder::EncodingState state);
  void DoSendInit(scoped_refptr<ClientConnection> client,
                  int width, int height);
  void DoGetInitInfo(scoped_refptr<ClientConnection> client);
  void DoSetRate(double rate);
  void DoSetMaxRate(double max_rate);
  void DoAddClient(scoped_refptr<ClientConnection> client);
  void DoRemoveClient(scoped_refptr<ClientConnection> client);
  void DoRemoveAllClients();
  void DoRateControl();

  // Hepler method to schedule next capture using the current rate.
  void ScheduleNextCapture();

  // Helper method to schedule next rate regulation task.
  void ScheduleNextRateControl();

  void CaptureDoneCallback(scoped_refptr<Capturer::CaptureData> capture_data);
  // EncodeDataAvailableTask takes ownership of header and is responsible for
  // deleting it.
  void EncodeDataAvailableTask(const UpdateStreamPacketHeader *header,
                               const scoped_refptr<media::DataBuffer>& data,
                               Encoder::EncodingState state);

  // Getters for capturer and encoder.
  Capturer* capturer();
  Encoder* encoder();

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
  // This member is always accessed on the NETWORK thread.
  // TODO(hclam): Have to scoped_refptr the clients since they have a shorter
  // lifetime than this object.
  typedef std::vector<scoped_refptr<ClientConnection> > ClientConnectionList;
  ClientConnectionList clients_;

  // The following members are accessed on the capture thread.
  double rate_;  // Number of captures to perform every second.
  bool started_;
  base::Time last_capture_time_; // Saves the time last capture started.
  int recordings_; // Count the number of recordings
                   // (i.e. capture or encode) happening.

  // The maximum rate is written on the capture thread and read on the network
  // thread.
  double max_rate_;  // Number of captures to perform every second.

  // The following member is accessed on the network thread.
  bool rate_control_started_;

  DISALLOW_COPY_AND_ASSIGN(SessionManager);
};

}  // namespace remoting

#endif  // REMOTING_HOST_RECORD_SESSION_H_
