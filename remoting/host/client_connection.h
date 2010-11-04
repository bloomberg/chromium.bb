// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CLIENT_CONNECTION_H_
#define REMOTING_HOST_CLIENT_CONNECTION_H_

#include <deque>
#include <vector>

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "remoting/proto/internal.pb.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/stream_writer.h"
#include "remoting/protocol/video_writer.h"

namespace remoting {
namespace protocol {

// This class represents a remote viewer connected to the chromoting host
// through a libjingle connection. A viewer object is responsible for sending
// screen updates and other messages to the remote viewer. It is also
// responsible for receiving and parsing data from the remote viewer and
// delegating events to the event handler.
class ClientConnection : public base::RefCountedThreadSafe<ClientConnection> {
 public:
  class EventHandler {
   public:
    virtual ~EventHandler() {}

    // Handles an event received by the ClientConnection. Receiver will own the
    // ClientMessages in ClientMessageList and needs to delete them.
    // Note that the sender of messages will not reference messages
    // again so it is okay to clear |messages| in this method.
    virtual void HandleMessage(ClientConnection* viewer,
                               ChromotingClientMessage* message) = 0;

    // Called when the network connection is opened.
    virtual void OnConnectionOpened(ClientConnection* viewer) = 0;

    // Called when the network connection is closed.
    virtual void OnConnectionClosed(ClientConnection* viewer) = 0;

    // Called when the network connection has failed.
    virtual void OnConnectionFailed(ClientConnection* viewer) = 0;
  };

  // Constructs a ClientConnection object. |message_loop| is the message loop
  // that this object runs on. A viewer object receives events and messages from
  // a libjingle channel, these events are delegated to |handler|.
  // It is guranteed that |handler| is called only on the |message_loop|.
  ClientConnection(MessageLoop* message_loop,
                   EventHandler* handler);

  virtual ~ClientConnection();

  virtual void Init(protocol::Session* session);

  // Returns the connection in use.
  virtual protocol::Session* session();

  // Send information to the client for initialization.
  virtual void SendInitClientMessage(int width, int height);

  // Send encoded update stream data to the viewer.
  virtual void SendVideoPacket(const VideoPacket& packet);

  // Gets the number of update stream messages not yet transmitted.
  // Note that the value returned is an estimate using average size of the
  // most recent update streams.
  // TODO(hclam): Report this number accurately.
  virtual int GetPendingUpdateStreamMessages();

  // Disconnect the client connection. This method is allowed to be called
  // more than once and calls after the first one will be ignored.
  //
  // After this method is called all the send method calls will be ignored.
  virtual void Disconnect();

 protected:
  // Protected constructor used by unit test.
  ClientConnection();

 private:
  // Callback for protocol Session.
  void OnSessionStateChange(protocol::Session::State state);

  // Callback for MessageReader.
  void OnMessageReceived(ChromotingClientMessage* message);

  // Process a libjingle state change event on the |loop_|.
  void StateChangeTask(protocol::Session::State state);

  // Process a data buffer received from libjingle.
  void MessageReceivedTask(ChromotingClientMessage* message);

  void OnClosed();

  // The libjingle channel used to send and receive data from the remote client.
  scoped_refptr<protocol::Session> session_;

  ControlStreamWriter control_writer_;
  MessageReader event_reader_;
  scoped_ptr<VideoWriter> video_writer_;

  // The message loop that this object runs on.
  MessageLoop* loop_;

  // Event handler for handling events sent from this object.
  EventHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(ClientConnection);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_HOST_CLIENT_CONNECTION_H_
