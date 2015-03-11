// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_
#define REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/errors.h"

namespace remoting {

class SignalStrategy;

namespace protocol {

class AudioStub;
class Authenticator;
class CandidateSessionConfig;
class ClientStub;
class ClipboardStub;
class ExtensionMessage;
class HostStub;
class InputStub;
class SessionConfig;
class TransportFactory;
struct TransportRoute;
class VideoStub;

class ConnectionToHost {
 public:
  // The UI implementations maintain corresponding definitions of this
  // enumeration in webapp/client_session.js and
  // android/java/src/org/chromium/chromoting/jni/JniInterface.java. Be sure to
  // update these locations if you make any changes to the ordering.
  enum State {
    INITIALIZING,
    CONNECTING,
    AUTHENTICATED,
    CONNECTED,
    FAILED,
    CLOSED,
  };

  class HostEventCallback {
   public:
    virtual ~HostEventCallback() {}

    // Called when state of the connection changes.
    virtual void OnConnectionState(State state, ErrorCode error) = 0;

    // Called when ready state of the connection changes. When |ready|
    // is set to false some data sent by the peers may be
    // delayed. This is used to indicate in the UI when connection is
    // temporarily broken.
    virtual void OnConnectionReady(bool ready) = 0;

    // Called when the route type (direct vs. STUN vs. proxied) changes.
    virtual void OnRouteChanged(const std::string& channel_name,
                                const protocol::TransportRoute& route) = 0;
  };

  virtual ~ConnectionToHost() {}

  // Allows to set a custom protocol configuration (e.g. for tests). Cannot be
  // called after Connect().
  virtual void set_candidate_config(
      scoped_ptr<CandidateSessionConfig> config) = 0;

  // Set the stubs which will handle messages from the host.
  // The caller must ensure that stubs out-live the connection.
  // Unless otherwise specified, all stubs must be set before Connect()
  // is called.
  virtual void set_client_stub(ClientStub* client_stub) = 0;
  virtual void set_clipboard_stub(ClipboardStub* clipboard_stub) = 0;
  virtual void set_video_stub(VideoStub* video_stub) = 0;
  // If no audio stub is specified then audio will not be requested.
  virtual void set_audio_stub(AudioStub* audio_stub) = 0;

  // Initiates a connection to the host specified by |host_jid|.
  // |signal_strategy| is used to signal to the host, and must outlive the
  // connection. Data channels will be negotiated over |transport_factory|.
  // |authenticator| will be used to authenticate the session and data channels.
  // |event_callback| will be notified of changes in the state of the connection
  // and must outlive the ConnectionToHost.
  // Caller must set stubs (see below) before calling Connect.
  virtual void Connect(SignalStrategy* signal_strategy,
                       scoped_ptr<TransportFactory> transport_factory,
                       scoped_ptr<Authenticator> authenticator,
                       const std::string& host_jid,
                       HostEventCallback* event_callback) = 0;

  // Returns the session configuration that was negotiated with the host.
  virtual const SessionConfig& config() = 0;

  // Stubs for sending data to the host.
  virtual ClipboardStub* clipboard_forwarder() = 0;
  virtual HostStub* host_stub() = 0;
  virtual InputStub* input_stub() = 0;

  // Return the current state of ConnectionToHost.
  virtual State state() const = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_
