// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONNECTION_TO_CLIENT_H_
#define REMOTING_PROTOCOL_CONNECTION_TO_CLIENT_H_

#include <stdint.h>

#include <string>

#include "remoting/protocol/transport.h"

namespace webrtc {
class DesktopCapturer;
}  // namespace webrtc

namespace remoting {

class VideoEncoder;

namespace protocol {

class AudioStub;
class ClientStub;
class ClipboardStub;
class HostStub;
class InputStub;
class Session;
class VideoStream;

// This interface represents a remote viewer connection to the chromoting host.
// It sets up all protocol channels and connects them to the stubs.
class ConnectionToClient {
 public:
  class EventHandler {
   public:
    // Called when the network connection is authenticating
    virtual void OnConnectionAuthenticating(ConnectionToClient* connection) = 0;

    // Called when the network connection is authenticated.
    virtual void OnConnectionAuthenticated(ConnectionToClient* connection) = 0;

    // Called to request creation of video streams. May be called before or
    // after OnConnectionChannelsConnected().
    virtual void CreateVideoStreams(ConnectionToClient* connection) = 0;

    // Called when the network connection is authenticated and all
    // channels are connected.
    virtual void OnConnectionChannelsConnected(
        ConnectionToClient* connection) = 0;

    // Called when the network connection is closed or failed.
    virtual void OnConnectionClosed(ConnectionToClient* connection,
                                    ErrorCode error) = 0;

    // Called when a new input event is received.
    virtual void OnInputEventReceived(ConnectionToClient* connection,
                                      int64_t timestamp) = 0;

    // Called on notification of a route change event, which happens when a
    // channel is connected.
    virtual void OnRouteChange(ConnectionToClient* connection,
                               const std::string& channel_name,
                               const TransportRoute& route) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  ConnectionToClient() {}
  virtual ~ConnectionToClient() {}

  // Set |event_handler| for connection events. Must be called once when this
  // object is created.
  virtual void SetEventHandler(EventHandler* event_handler) = 0;

  // Returns the Session object for the connection.
  // TODO(sergeyu): Remove this method.
  virtual Session* session() = 0;

  // Disconnect the client connection.
  virtual void Disconnect(ErrorCode error) = 0;

  // Callback for HostEventDispatcher to be called with a timestamp for each
  // received event.
  virtual void OnInputEventReceived(int64_t timestamp) = 0;

  // Start video stream that sends screen content from |desktop_capturer| to the
  // client.
  virtual std::unique_ptr<VideoStream> StartVideoStream(
      std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer) = 0;

  // Get the stubs used by the host to transmit messages to the client.
  // The stubs must not be accessed before OnConnectionAuthenticated(), or
  // after OnConnectionClosed().
  // Note that the audio stub will be nullptr if audio is not enabled.
  virtual AudioStub* audio_stub() = 0;
  virtual ClientStub* client_stub() = 0;

  // Set the stubs which will handle messages we receive from the client. These
  // must be called in EventHandler::OnConnectionAuthenticated().
  virtual void set_clipboard_stub(ClipboardStub* clipboard_stub) = 0;
  virtual void set_host_stub(HostStub* host_stub) = 0;
  virtual void set_input_stub(InputStub* input_stub) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CONNECTION_TO_CLIENT_H_
