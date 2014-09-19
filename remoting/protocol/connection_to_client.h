// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONNECTION_TO_CLIENT_H_
#define REMOTING_PROTOCOL_CONNECTION_TO_CLIENT_H_

#include <deque>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "remoting/protocol/audio_writer.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

class ClientStub;
class ClipboardStub;
class HostControlDispatcher;
class HostEventDispatcher;
class HostStub;
class HostVideoDispatcher;
class InputStub;
class VideoStub;

// This class represents a remote viewer connection to the chromoting
// host. It sets up all protocol channels and connects them to the
// stubs.
class ConnectionToClient : public base::NonThreadSafe,
                           public Session::EventHandler {
 public:
  class EventHandler {
   public:
    // Called when the network connection is authenticating
    virtual void OnConnectionAuthenticating(ConnectionToClient* connection) = 0;

    // Called when the network connection is authenticated.
    virtual void OnConnectionAuthenticated(ConnectionToClient* connection) = 0;

    // Called when the network connection is authenticated and all
    // channels are connected.
    virtual void OnConnectionChannelsConnected(
        ConnectionToClient* connection) = 0;

    // Called when the network connection is closed or failed.
    virtual void OnConnectionClosed(ConnectionToClient* connection,
                                    ErrorCode error) = 0;

    // Called when sequence number is updated.
    virtual void OnSequenceNumberUpdated(ConnectionToClient* connection,
                                         int64 sequence_number) = 0;

    // Called on notification of a route change event, which happens when a
    // channel is connected.
    virtual void OnRouteChange(ConnectionToClient* connection,
                               const std::string& channel_name,
                               const TransportRoute& route) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  // Constructs a ConnectionToClient object for the |session|. Takes
  // ownership of |session|.
  explicit ConnectionToClient(Session* session);
  virtual ~ConnectionToClient();

  // Set |event_handler| for connection events. Must be called once when this
  // object is created.
  void SetEventHandler(EventHandler* event_handler);

  // Returns the connection in use.
  virtual Session* session();

  // Disconnect the client connection.
  virtual void Disconnect();

  // Update the sequence number when received from the client. EventHandler
  // will be called.
  virtual void UpdateSequenceNumber(int64 sequence_number);

  // Get the stubs used by the host to transmit messages to the client.
  // The stubs must not be accessed before OnConnectionAuthenticated(), or
  // after OnConnectionClosed().
  // Note that the audio stub will be NULL if audio is not enabled.
  virtual VideoStub* video_stub();
  virtual AudioStub* audio_stub();
  virtual ClientStub* client_stub();

  // Set/get the stubs which will handle messages we receive from the client.
  // All stubs MUST be set before the session's channels become connected.
  virtual void set_clipboard_stub(ClipboardStub* clipboard_stub);
  virtual ClipboardStub* clipboard_stub();
  virtual void set_host_stub(HostStub* host_stub);
  virtual HostStub* host_stub();
  virtual void set_input_stub(InputStub* input_stub);
  virtual InputStub* input_stub();

  // Session::EventHandler interface.
  virtual void OnSessionStateChange(Session::State state) OVERRIDE;
  virtual void OnSessionRouteChange(const std::string& channel_name,
                                    const TransportRoute& route) OVERRIDE;

 private:
  // Callback for channel initialization.
  void OnChannelInitialized(bool successful);

  void NotifyIfChannelsReady();

  void Close(ErrorCode error);

  // Stops writing in the channels.
  void CloseChannels();

  // Event handler for handling events sent from this object.
  EventHandler* handler_;

  // Stubs that are called for incoming messages.
  ClipboardStub* clipboard_stub_;
  HostStub* host_stub_;
  InputStub* input_stub_;

  // The libjingle channel used to send and receive data from the remote client.
  scoped_ptr<Session> session_;

  scoped_ptr<HostControlDispatcher> control_dispatcher_;
  scoped_ptr<HostEventDispatcher> event_dispatcher_;
  scoped_ptr<HostVideoDispatcher> video_dispatcher_;
  scoped_ptr<AudioWriter> audio_writer_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionToClient);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CONNECTION_TO_CLIENT_H_
