// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONNECTION_TO_CLIENT_H_
#define REMOTING_PROTOCOL_CONNECTION_TO_CLIENT_H_

#include <deque>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/video_writer.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {
namespace protocol {

class ClientStub;
class HostStub;
class InputStub;
class HostControlDispatcher;
class HostEventDispatcher;

// This class represents a remote viewer connection to the chromoting
// host. It sets up all protocol channels and connects them to the
// stubs.
class ConnectionToClient : public base::NonThreadSafe {
 public:
  class EventHandler {
   public:
    virtual ~EventHandler() {}

    // Called when the network connection is opened.
    virtual void OnConnectionOpened(ConnectionToClient* connection) = 0;

    // Called when the network connection is closed.
    virtual void OnConnectionClosed(ConnectionToClient* connection) = 0;

    // Called when the network connection has failed.
    virtual void OnConnectionFailed(ConnectionToClient* connection) = 0;

    // Called when sequence number is updated.
    virtual void OnSequenceNumberUpdated(ConnectionToClient* connection,
                                         int64 sequence_number) = 0;
  };

  // Constructs a ConnectionToClient object for the |session|. Takes
  // ownership of |session|.
  ConnectionToClient(Session* session);
  virtual ~ConnectionToClient();

  // Set |event_handler| for connection events. |event_handler| is
  // guaranteed to be used only on the network thread. Must be called
  // once when this object is created.
  void SetEventHandler(EventHandler* event_handler);

  // Returns the connection in use.
  virtual Session* session();

  // Disconnect the client connection.
  virtual void Disconnect();

  // Update the sequence number when received from the client. EventHandler
  // will be called.
  virtual void UpdateSequenceNumber(int64 sequence_number);

  // Send encoded update stream data to the viewer.
  virtual VideoStub* video_stub();

  // Return pointer to ClientStub.
  virtual ClientStub* client_stub();

  // These two setters should be called before Init().
  virtual void set_host_stub(HostStub* host_stub);
  virtual void set_input_stub(InputStub* input_stub);

 private:
  // Callback for protocol Session.
  void OnSessionStateChange(Session::State state);

  // Callback for VideoReader::Init().
  void OnVideoInitialized(bool successful);

  void NotifyIfChannelsReady();

  void CloseOnError();

  // Stops writing in the channels.
  void CloseChannels();

  // Event handler for handling events sent from this object.
  EventHandler* handler_;

  // Stubs that are called for incoming messages.
  HostStub* host_stub_;
  InputStub* input_stub_;

  // The libjingle channel used to send and receive data from the remote client.
  scoped_ptr<Session> session_;

  scoped_ptr<VideoWriter> video_writer_;
  scoped_ptr<HostControlDispatcher> control_dispatcher_;
  scoped_ptr<HostEventDispatcher> input_dispatcher_;

  // State of the channels.
  bool control_connected_;
  bool input_connected_;
  bool video_connected_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionToClient);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CONNECTION_TO_CLIENT_H_
